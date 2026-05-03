#!/usr/bin/env python3
"""Barrido de configuraciones de UF para los apartados d) y e).

CORRECCIONES vs versión anterior:
  - Usa -s y -v por separado (no -i basename)
  - El flag -c acepta solo 5 valores (la UF de salto siempre es 1)
  - El nombre real del jar es 'SIMDE-VLIW-Lite.jar'

Configuración por defecto (50 €): 2,2,2,2,2 + 1 salto (fija) = 11 UFs
  d) cada UF añadida cuesta +5 €
  e) cada UF eliminada ahorra -2 €

Uso:
    python3 barrido_uf.py <basename>
"""

import re
import subprocess
import sys
from pathlib import Path

JAR = 'SIMDE-VLIW-Lite.jar'
MEMS = ['order1.mem', 'order2.mem', 'order3.mem', 'order4.mem', 'order5.mem']

# Configuración por defecto: SumaEnt, MultEnt, SumaFlot, MultFlot, Mem
# (la UF de salto SIEMPRE es 1, no se pasa al simulador)
UF_DEFAULT = [2, 2, 2, 2, 2]
N_UF_BASE = sum(UF_DEFAULT) + 1  # 10 + 1 salto = 11
COSTE_BASE = 50  # €
COSTE_ANIADIR = 5  # € por UF añadida
COSTE_QUITAR = 2  # € ahorrados por UF eliminada

# Configuraciones a probar. La UF de salto siempre es 1 y no aparece.
CONFIGS = [
    ('BASELINE', [2, 2, 2, 2, 2], 'config por defecto, 11 UFs, 50€'),

    # --- APARTADO D: añadir UFs (no se puede añadir Salto) ---
    ('D1', [2, 2, 2, 2, 3], 'D: +1 Memoria'),
    ('D2', [3, 2, 2, 2, 2], 'D: +1 SumaEnt'),
    ('D3', [3, 2, 2, 2, 3], 'D: +1 SumaEnt, +1 Memoria'),
    ('D4', [4, 2, 2, 2, 4], 'D: agresivo (+2 SumaEnt, +2 Mem)'),
    ('D5', [2, 3, 2, 2, 2], 'D: +1 MultEnt (control: UF ociosa)'),

    # --- APARTADO E: quitar UFs (la de salto no se puede quitar) ---
    ('E1', [2, 0, 0, 0, 2], 'E: quitar las 6 UFs Mult/Flot ociosas'),
    ('E2', [1, 0, 0, 0, 2], 'E: además quitar 1 SumaEnt'),
    ('E3', [2, 0, 0, 0, 1], 'E: además quitar 1 Memoria'),
    ('E4', [1, 0, 0, 0, 1], 'E: agresivo (-1 SumaEnt, -1 Mem)'),
    ('E5', [2, 1, 1, 1, 2], 'E: quitar solo 3 UFs (1 de cada Mult/Flot)'),
    ('E6', [2, 2, 0, 0, 2], 'E: quitar solo las 4 UFs Flot'),
]


def coste(uf):
    """Coste total de una configuración (incluyendo la UF de salto)."""
    diff = sum(uf) - sum(UF_DEFAULT)
    if diff > 0:
        return COSTE_BASE + diff * COSTE_ANIADIR
    return COSTE_BASE + diff * COSTE_QUITAR  # diff negativo, baja el coste


def ejecutar(basename, mem, uf):
    """Ejecuta una simulación y devuelve los ciclos como float."""
    cmd = ['java', '-jar', JAR,
           '-s', f'{basename}.pla',
           '-v', f'{basename}.vliw',
           '-m', mem,
           '-c', ','.join(str(x) for x in uf)]

    try:
        out = subprocess.check_output(cmd, stderr=subprocess.STDOUT,
                                      text=True, timeout=180)
    except subprocess.CalledProcessError as e:
        print(f'  ERR ({e.returncode}): {e.output[:200]}', file=sys.stderr)
        return None
    except FileNotFoundError:
        print(f'ERROR: no se encuentra "{JAR}" en {Path.cwd()}',
              file=sys.stderr)
        sys.exit(1)

    # Probar varios patrones posibles para los ciclos
    patrones = [
        r'(?:Average\s+)?[Cc]iclos?\s*[:=]\s*([\d.]+)',
        r'(?:Average\s+)?[Cc]ycles?\s*[:=]\s*([\d.]+)',
        r'[Tt]otal\s*[:=]?\s*([\d.]+)\s*[Cc]iclos?',
        r'(?:Number\s+of\s+)?[Cc]ycles?\s+(?:executed)?\s*[:=]?\s*([\d.]+)',
        # patrón "X ciclos" o "X cycles" al final de línea
        r'^\s*([\d.]+)\s*$',
    ]
    for pat in patrones:
        m = re.search(pat, out, re.MULTILINE)
        if m:
            return float(m.group(1))

    # Si nada matchea, mostrar la salida bruta y devolver None
    print(f'  AVISO: no se pudo extraer ciclos de la salida:', file=sys.stderr)
    print(out[:500], file=sys.stderr)
    return None


def main():
    if len(sys.argv) < 2:
        print(f'Uso: {sys.argv[0]} <basename>', file=sys.stderr)
        sys.exit(1)

    basename = sys.argv[1]

    if not Path(JAR).exists():
        print(f'ERROR: no se encuentra "{JAR}"', file=sys.stderr)
        sys.exit(1)
    if not Path(f'{basename}.pla').exists():
        print(f'ERROR: no se encuentra "{basename}.pla"', file=sys.stderr)
        sys.exit(1)
    if not Path(f'{basename}.vliw').exists():
        print(f'ERROR: no se encuentra "{basename}.vliw"', file=sys.stderr)
        sys.exit(1)

    mems = [m for m in MEMS if Path(m).exists()]
    if not mems:
        print('ERROR: no se encontró ningún order*.mem', file=sys.stderr)
        sys.exit(1)
    print(f'Memorias disponibles: {", ".join(mems)}\n')

    resultados = {}
    for nombre, uf, desc in CONFIGS:
        c = coste(uf)
        n_uf = sum(uf) + 1  # +1 por la de salto
        print(f'>> {nombre}  c={c}€  uf={uf}+[1 salto] (={n_uf} UFs)')
        print(f'   {desc}')
        resultados[nombre] = {}
        for mem in mems:
            ciclos = ejecutar(basename, mem, uf)
            resultados[nombre][mem] = ciclos
            print(f'   {mem}: {ciclos}')
        print()

    # === Tabla resumen ===
    print('=' * 95)
    print('RESUMEN — ciclos por configuración y memoria')
    print('=' * 95)
    cab = f'{"Config":<10} {"Coste":>6} '
    for mem in mems:
        cab += f'{mem.replace(".mem", ""):>10} '
    cab += f'{"Σciclos":>10}'
    print(cab)
    print('-' * 95)

    base = resultados.get('BASELINE', {})
    base_total = sum(v for v in base.values() if v is not None)

    for nombre, uf, _ in CONFIGS:
        r = resultados[nombre]
        total = sum(v for v in r.values() if v is not None)
        c = coste(uf)
        linea = f'{nombre:<10} {c:>5}€ '
        for mem in mems:
            v = r.get(mem)
            linea += f'{v:>10.0f} ' if v is not None else f'{"ERR":>10} '
        linea += f'{total:>10.0f}'
        print(linea)
    print()

    # === Aceleraciones y ratios ===
    if base_total <= 0:
        print('No se puede calcular aceleración sin BASELINE.')
        return

    print('=' * 95)
    print('ACELERACIÓN Y RATIO COSTE/ACELERACIÓN (vs BASELINE)')
    print('=' * 95)
    print(f'{"Config":<10} {"Coste":>6} {"Δ€":>5} {"Σciclos":>10} '
          f'{"Aceler.":>9} {"Δ€/Acel":>9}  {"Comentario":<35}')
    print('-' * 95)
    for nombre, uf, desc in CONFIGS:
        r = resultados[nombre]
        total = sum(v for v in r.values() if v is not None)
        c = coste(uf)
        dc = c - COSTE_BASE
        aceler = base_total / total if total > 0 else float('nan')
        ratio = dc / aceler if aceler else float('nan')
        print(f'{nombre:<10} {c:>5}€ {dc:>+4}€ {total:>10.0f} '
              f'{aceler:>9.4f} {ratio:>+9.4f}  {desc:<35}')
    print('-' * 95)
    print('NOTA: en d) buscar ratio MÍNIMO POSITIVO (poco coste por mucha aceleración).')
    print('      en e) buscar ratio MÁS NEGATIVO (mucho ahorro sin perder aceleración).')


if __name__ == '__main__':
    main()