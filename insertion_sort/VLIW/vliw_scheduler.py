#!/usr/bin/env python3
"""Planificador VLIW automático para SIMDE.

Lee un fichero .pla y produce un fichero .vliw con una planificación
estática válida usando list scheduling greedy con prioridad por altura.

Uso:
    python3 vliw_scheduler.py insertion_optimized.pla insertion_optimized.vliw

Configuración por defecto de la máquina VLIW de SIMDE (de la memoria
del proyecto, Tabla 4-2):
    Tipo UF       | Lat | NUF | Instrucciones
    --------------|-----|-----|----------------------------------------
    0 SumaEnt     |  1  |  2  | ADD, ADDI, SUB, OR, AND, XOR, NOR,
                  |     |     |   SLLV, SRLV
    1 MultEnt     |  2  |  2  | MULT
    2 SumaFlot    |  4  |  2  | ADDF, SUBF
    3 MultFlot    |  6  |  2  | MULTF
    4 Memoria     |  4  |  2  | LF, SF, LW, SW
    5 Salto       |  2  |  1  | BNE, BEQ, BGT
"""

import re
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from typing import Optional


# --- Configuración de la máquina VLIW por defecto ---
TIPO_SUMAENT = 0
TIPO_MULTENT = 1
TIPO_SUMAFLOT = 2
TIPO_MULTFLOT = 3
TIPO_MEMORIA = 4
TIPO_SALTO = 5

LATENCIAS = {
    TIPO_SUMAENT: 1,
    TIPO_MULTENT: 2,
    TIPO_SUMAFLOT: 4,
    TIPO_MULTFLOT: 6,
    TIPO_MEMORIA: 4,
    TIPO_SALTO: 2,
}

NUM_UF = {
    TIPO_SUMAENT: 2,
    TIPO_MULTENT: 2,
    TIPO_SUMAFLOT: 2,
    TIPO_MULTFLOT: 2,
    TIPO_MEMORIA: 2,
    TIPO_SALTO: 1,
}

# Mapeo opcode -> tipo de UF
OPCODE_A_TIPO = {
    'ADD': TIPO_SUMAENT, 'ADDI': TIPO_SUMAENT, 'SUB': TIPO_SUMAENT,
    'OR': TIPO_SUMAENT, 'AND': TIPO_SUMAENT, 'XOR': TIPO_SUMAENT,
    'NOR': TIPO_SUMAENT, 'SLLV': TIPO_SUMAENT, 'SRLV': TIPO_SUMAENT,
    'MULT': TIPO_MULTENT,
    'ADDF': TIPO_SUMAFLOT, 'SUBF': TIPO_SUMAFLOT,
    'MULTF': TIPO_MULTFLOT,
    'LF': TIPO_MEMORIA, 'SF': TIPO_MEMORIA,
    'LW': TIPO_MEMORIA, 'SW': TIPO_MEMORIA,
    'BNE': TIPO_SALTO, 'BEQ': TIPO_SALTO,
    'BGT': TIPO_SALTO, 'BGTF': TIPO_SALTO,
}

OPCODES_SALTO = {'BNE', 'BEQ', 'BGT', 'BGTF'}


@dataclass
class Instr:
    """Representa una instrucción del .pla, ya parseada."""
    idx: int                         # índice 0-based en el .pla
    opcode: str
    operandos: list                  # tokens originales sin parsear
    etiqueta: Optional[str] = None   # etiqueta que precede a esta instr
    destino: Optional[str] = None    # etiqueta destino si es salto
    # Análisis de registros (calculado después)
    escribe: list = field(default_factory=list)   # regs que escribe
    lee: list = field(default_factory=list)       # regs que lee


def parsear_pla(ruta):
    """Parsea un fichero .pla y devuelve (instrucciones, etiqueta_a_idx).

    - Las líneas comentario (//) y vacías se ignoran.
    - Las etiquetas (terminadas en :) se asocian a la siguiente instrucción.
    - Una etiqueta al final del fichero sin instrucción siguiente (típico:
      END:) se mapea al índice len(instrucciones), que está fuera del
      rango y se interpretará como "fin de programa".
    - Los operandos se conservan tal cual; el análisis de registros se hace
      en analizar_registros().
    """
    instrucciones = []
    etiqueta_a_idx = {}
    etiqueta_pendiente = None

    with open(ruta, encoding='utf-8') as f:
        for linea in f:
            # Limpiar comentarios y espacios
            linea = re.sub(r'//.*$', '', linea).strip()
            if not linea:
                continue

            # ¿Es una línea de etiqueta sola? (LABEL:)
            m = re.match(r'^([A-Za-z_]\w*)\s*:\s*$', linea)
            if m:
                etiqueta_pendiente = m.group(1)
                continue

            # ¿Es etiqueta + instrucción en la misma línea?
            m = re.match(r'^([A-Za-z_]\w*)\s*:\s*(.+)$', linea)
            if m:
                etiqueta_pendiente = m.group(1)
                linea = m.group(2)

            # Parsear opcode y operandos
            tokens = re.split(r'[\s,]+', linea)
            tokens = [t for t in tokens if t]
            if not tokens:
                continue

            opcode = tokens[0].upper()
            operandos = tokens[1:]
            idx = len(instrucciones)

            instr = Instr(idx=idx, opcode=opcode, operandos=operandos,
                          etiqueta=etiqueta_pendiente)

            # Si es salto, el último operando es la etiqueta destino
            if opcode in OPCODES_SALTO:
                instr.destino = operandos[-1]

            if etiqueta_pendiente:
                etiqueta_a_idx[etiqueta_pendiente] = idx
                etiqueta_pendiente = None

            instrucciones.append(instr)

    # Etiqueta huérfana al final del fichero: la mapeamos a "fin de
    # programa" (índice fuera del rango de instrucciones)
    if etiqueta_pendiente is not None:
        etiqueta_a_idx[etiqueta_pendiente] = len(instrucciones)

    return instrucciones, etiqueta_a_idx


def analizar_registros(instr):
    """Determina qué registros lee y escribe una instrucción.

    Convenio MIPS-like de SIMDE:
      ADD/SUB/.../ADDF/SUBF/MULT/MULTF Rd, Ro1, Ro2  -> escribe Rd, lee Ro1, Ro2
      ADDI Rd, Ro1, #Inm                              -> escribe Rd, lee Ro1
      LF/LW Rd, Inm(Ro)                               -> escribe Rd, lee Ro
      SF/SW Ro, Inm(Rd)                               -> lee Ro y Rd, escribe NADA
        (el Rd aquí es base de la dirección, no se modifica; SIMDE lo
         considera un READ del registro de dirección)
      BNE/BEQ/BGT Ro1, Ro2, etiqueta                  -> lee Ro1, Ro2
    """
    op = instr.opcode
    args = instr.operandos
    escribe, lee = [], []

    if op in ('ADD', 'SUB', 'MULT', 'OR', 'AND', 'XOR', 'NOR',
              'SLLV', 'SRLV', 'ADDF', 'SUBF', 'MULTF'):
        # 3 registros: Rd, Ro1, Ro2
        escribe.append(args[0])
        lee.extend([args[1], args[2]])
    elif op == 'ADDI':
        # ADDI Rd, Ro1, #Inm
        escribe.append(args[0])
        lee.append(args[1])
    elif op in ('LF', 'LW'):
        # Rd, Inm(Ro)
        escribe.append(args[0])
        m = re.match(r'.*\(([^)]+)\)', args[1])
        if m:
            lee.append(m.group(1))
    elif op in ('SF', 'SW'):
        # Ro, Inm(Rd) -- ambos registros se LEEN
        lee.append(args[0])
        m = re.match(r'.*\(([^)]+)\)', args[1])
        if m:
            lee.append(m.group(1))
    elif op in OPCODES_SALTO:
        # BNE/BEQ/BGT/BGTF Ro1, Ro2, etiqueta
        lee.extend([args[0], args[1]])

    instr.escribe = escribe
    instr.lee = lee


def identificar_bloques_basicos(instrucciones, etiqueta_a_idx):
    """Devuelve lista de (inicio, fin_exclusivo) de cada bloque básico.

    Un BB empieza en:
      - la primera instrucción
      - cualquier instrucción que sea destino de un salto (tiene etiqueta
        que aparece como destino)
      - la instrucción inmediatamente después de un salto

    Un BB termina en:
      - una instrucción de salto (incluida)
      - la instrucción anterior al inicio del siguiente BB
    """
    n = len(instrucciones)
    if n == 0:
        return []

    # Conjunto de inicios de BB
    inicios = {0}
    destinos_salto = set()
    for instr in instrucciones:
        if instr.destino is not None:
            if instr.destino in etiqueta_a_idx:
                destinos_salto.add(etiqueta_a_idx[instr.destino])
            inicios.add(instr.idx + 1)  # instr siguiente al salto
    inicios.update(destinos_salto)
    inicios = sorted(i for i in inicios if i < n)

    # Construir bloques
    bloques = []
    for k, ini in enumerate(inicios):
        fin = inicios[k + 1] if k + 1 < len(inicios) else n
        bloques.append((ini, fin))
    return bloques


def construir_dag(instrucciones, ini, fin):
    """Construye el DAG de dependencias para un bloque básico [ini, fin).

    Tres tipos de aristas (i -> j significa "j depende de i"):
      RAW: i escribe X, j lee X después
      WAR: i lee X, j escribe X después (importante en VLIW)
      WAW: i escribe X, j escribe X después
    Además: cualquier instrucción debe ir antes que el salto final del BB.
    """
    deps = defaultdict(set)  # deps[j] = {i : i debe terminar antes que j}

    # Última escritura/lectura de cada registro vista hasta ahora
    ultima_escritura = {}
    ultimas_lecturas = defaultdict(list)

    for k in range(ini, fin):
        instr = instrucciones[k]

        # RAW: este lee algo que alguien escribió antes
        for r in instr.lee:
            if r in ultima_escritura:
                deps[k].add(ultima_escritura[r])
        # WAW: este escribe algo que alguien escribió antes
        for r in instr.escribe:
            if r in ultima_escritura:
                deps[k].add(ultima_escritura[r])
        # WAR: este escribe algo que alguien leyó antes
        for r in instr.escribe:
            for prev in ultimas_lecturas.get(r, []):
                if prev != k:
                    deps[k].add(prev)

        # Actualizar últimas lecturas/escrituras
        for r in instr.escribe:
            ultima_escritura[r] = k
            ultimas_lecturas[r] = []  # tras escritura, las lecturas previas
                                       # ya están "consumidas"
        for r in instr.lee:
            ultimas_lecturas[r].append(k)

    # Restricción de salto: el salto del BB (si lo hay) debe ir el último
    salto_idx = None
    for k in range(ini, fin):
        if instrucciones[k].opcode in OPCODES_SALTO:
            salto_idx = k
            break
    if salto_idx is not None:
        for k in range(ini, fin):
            if k != salto_idx:
                deps[salto_idx].add(k)

    return deps


def calcular_alturas(deps, ini, fin, instrucciones):
    """Calcula la altura (longitud del camino más largo) de cada
    instrucción en el DAG. Mayor altura = mayor prioridad.
    """
    # deps[j] = {i: i->j}. Necesitamos los hijos (sucesores).
    sucesores = defaultdict(set)
    for j, ins in deps.items():
        for i in ins:
            sucesores[i].add(j)

    altura = {}
    def calc(k):
        if k in altura:
            return altura[k]
        lat_k = LATENCIAS[OPCODE_A_TIPO[instrucciones[k].opcode]]
        if not sucesores[k]:
            altura[k] = lat_k
            return lat_k
        altura[k] = lat_k + max(calc(s) for s in sucesores[k])
        return altura[k]

    for k in range(ini, fin):
        calc(k)
    return altura


def planificar_bloque(instrucciones, ini, fin, ciclo_inicio):
    """Aplica list scheduling al bloque básico [ini, fin).

    Devuelve un dict {idx_instr: (ciclo, tipo_uf, num_uf)} y el ciclo
    en el que termina el bloque (siguiente ciclo libre).
    """
    deps = construir_dag(instrucciones, ini, fin)
    altura = calcular_alturas(deps, ini, fin, instrucciones)

    # Estado del scheduler
    pendientes = set(range(ini, fin))
    ciclo_termina = {}      # ciclo en el que cada instr termina (lat)
    ocupacion = defaultdict(set)  # ocupacion[ciclo] = {(tipo, num_uf), ...}
    asignacion = {}         # idx -> (ciclo, tipo_uf, num_uf)

    ciclo = ciclo_inicio
    while pendientes:
        # Listas: instrucciones cuyas dependencias ya terminaron
        listas = []
        for k in pendientes:
            if all(d in ciclo_termina and ciclo_termina[d] <= ciclo
                   for d in deps[k]):
                listas.append(k)
        # Ordenar por altura desc, luego por idx asc para estabilidad
        listas.sort(key=lambda k: (-altura[k], k))

        # Intentar asignar cada lista a un hueco libre en este ciclo
        for k in listas:
            tipo = OPCODE_A_TIPO[instrucciones[k].opcode]
            for num in range(NUM_UF[tipo]):
                if (tipo, num) not in ocupacion[ciclo]:
                    # Asignar
                    asignacion[k] = (ciclo, tipo, num)
                    ocupacion[ciclo].add((tipo, num))
                    ciclo_termina[k] = ciclo + LATENCIAS[tipo]
                    pendientes.discard(k)
                    break

        ciclo += 1
        # Salvaguarda contra bucle infinito
        if ciclo > ciclo_inicio + 10000:
            raise RuntimeError(f'Scheduler atascado en bloque [{ini},{fin})')

    fin_bloque = max(ciclo_termina.values()) if ciclo_termina else ciclo_inicio
    return asignacion, fin_bloque


def planificar_programa(instrucciones, etiqueta_a_idx):
    """Planifica todos los bloques básicos en orden y devuelve la
    asignación global y el mapeo etiqueta -> instr_larga.

    NO se compactan ciclos vacíos: cada ciclo se convierte en una
    instrucción larga, posiblemente vacía. Esto es necesario porque en
    SIMDE el simulador VLIW emite exactamente una IL por ciclo, así que
    una IL vacía es equivalente a un ciclo de espera (necesario para
    cumplir las latencias entre operaciones dependientes).
    """
    bloques = identificar_bloques_basicos(instrucciones, etiqueta_a_idx)
    asignacion = {}  # idx_pla -> (ciclo, tipo, num)
    ciclo = 0

    for ini, fin in bloques:
        asign, ciclo = planificar_bloque(instrucciones, ini, fin, ciclo)
        asignacion.update(asign)

    # Determinar el número total de ciclos = número de ILs
    if asignacion:
        ciclo_max = max(c for c, _, _ in asignacion.values())
        n_il = ciclo_max + 1
    else:
        n_il = 0

    # Mapeo etiqueta -> id_il. Una etiqueta apunta a la IL en la que
    # vive la primera instrucción del PLA con esa etiqueta. Las
    # etiquetas huérfanas al final del programa (END) van a n_il.
    etiqueta_a_il = {}
    for et, idx in etiqueta_a_idx.items():
        if idx in asignacion:
            etiqueta_a_il[et] = asignacion[idx][0]
        else:
            etiqueta_a_il[et] = n_il

    return asignacion, etiqueta_a_il, n_il


def verificar_vliw(instrucciones, asignacion, n_il):
    """Emula la función chkDependencias del simulador VLIW de SIMDE
    para detectar violaciones RAW antes de probar en el simulador.

    El simulador procesa cada IL así:
      1. chkDestinoOp para cada op: marca chk[R_destino].lat = latencia
      2. chkFuenteOp para cada op: si chk[R_fuente].lat > 0 y el reg
         que lo marcó tiene id < id(op_actual) -> ERRRAW
      3. Decrementa todas las latencias en 1
    """
    # chk_gpr[R] = (lat, reg_id) ; chk_fpr[R] = (lat, reg_id)
    chk_gpr = defaultdict(lambda: [0, -1])
    chk_fpr = defaultdict(lambda: [0, -1])

    # Agrupar ops por IL
    por_il = defaultdict(list)
    for k, (il, _, _) in asignacion.items():
        por_il[il].append(k)

    for il in range(n_il):
        ops_il = sorted(por_il[il])

        # Paso 1: marcar destinos
        for k in ops_il:
            instr = instrucciones[k]
            tipo = OPCODE_A_TIPO[instr.opcode]
            lat = LATENCIAS[tipo]
            for r in instr.escribe:
                tabla = chk_fpr if r.startswith('F') else chk_gpr
                if tabla[r][0] < lat:
                    tabla[r] = [lat, k]

        # Paso 2: chequear fuentes
        for k in ops_il:
            instr = instrucciones[k]
            for r in instr.lee:
                tabla = chk_fpr if r.startswith('F') else chk_gpr
                if tabla[r][0] > 0 and tabla[r][1] < k:
                    return (il, k, r, tabla[r][1])

        # Paso 3: decrementar
        for tabla in (chk_gpr, chk_fpr):
            for r in tabla:
                if tabla[r][0] > 0:
                    tabla[r][0] -= 1

    return None


def emitir_vliw(instrucciones, asignacion, etiqueta_a_il, n_il, ruta):
    """Genera el fichero .vliw en el formato esperado por SIMDE.

    Formato:
        N
        <noper>\\t<id1> <tipo1> <num1> <pred1> [destino predT predF if salto] ...
        ...
    """
    # Agrupar por instrucción larga
    por_il = defaultdict(list)
    for k, (il, tipo, num) in asignacion.items():
        por_il[il].append((k, tipo, num))

    with open(ruta, 'w', encoding='utf-8') as f:
        f.write(f'{n_il}\n')
        for il in range(n_il):
            ops = sorted(por_il[il], key=lambda x: x[0])
            f.write(f'{len(ops)}')
            for idx, tipo, num in ops:
                instr = instrucciones[idx]
                pred = 0  # p0 = siempre verdadero
                f.write(f'\t{idx} {tipo} {num} {pred}')
                if instr.opcode in OPCODES_SALTO:
                    destino_il = etiqueta_a_il.get(instr.destino, 0)
                    pred_true, pred_false = 0, 0  # sin predicación
                    f.write(f' {destino_il} {pred_true} {pred_false}')
            f.write('\n')


def main():
    if len(sys.argv) < 3:
        print('Uso: vliw_scheduler.py entrada.pla salida.vliw [--lat-extra N]',
              file=sys.stderr)
        print('  --lat-extra N : añade N ciclos extra a cada latencia',
              file=sys.stderr)
        print('                  (por defecto 1, prueba con 0 si quieres '
              'apurar al máximo)', file=sys.stderr)
        sys.exit(1)

    pla_path, vliw_path = sys.argv[1], sys.argv[2]
    lat_extra = 1  # por defecto, conservador
    if '--lat-extra' in sys.argv:
        i = sys.argv.index('--lat-extra')
        lat_extra = int(sys.argv[i + 1])

    # Ajustar latencias globalmente
    for tipo in LATENCIAS:
        LATENCIAS[tipo] += lat_extra
    if lat_extra > 0:
        print(f'(usando latencias +{lat_extra} ciclos extra para ser '
              f'conservador con el simulador)')

    print(f'Parseando {pla_path}...')
    instrucciones, etiqueta_a_idx = parsear_pla(pla_path)
    for instr in instrucciones:
        analizar_registros(instr)
    print(f'  -> {len(instrucciones)} instrucciones, '
          f'{len(etiqueta_a_idx)} etiquetas')

    print('Identificando bloques básicos...')
    bloques = identificar_bloques_basicos(instrucciones, etiqueta_a_idx)
    print(f'  -> {len(bloques)} bloques')
    for i, (ini, fin) in enumerate(bloques):
        et = ''
        if instrucciones[ini].etiqueta:
            et = f' ({instrucciones[ini].etiqueta})'
        print(f'    BB{i}: [{ini}, {fin}){et}')

    print('Planificando...')
    asign, et2il, n_il = planificar_programa(instrucciones, etiqueta_a_idx)
    print(f'  -> {n_il} instrucciones largas '
          f'(compactación {len(instrucciones)}/{n_il} = '
          f'{len(instrucciones)/n_il:.2f} ops/IL)')

    print('Verificando ausencia de RAW (emulando chkDependencias)...')
    err = verificar_vliw(instrucciones, asign, n_il)
    if err is not None:
        il, k, r, idA = err
        instr = instrucciones[k]
        instrA = instrucciones[idA]
        print(f'  ERROR: en IL {il}, op {k} ({instr.opcode} '
              f'{" ".join(instr.operandos)}) lee {r}, pero op {idA} '
              f'({instrA.opcode} {" ".join(instrA.operandos)}) '
              f'aún tiene latencia pendiente.', file=sys.stderr)
        print('  El .vliw se va a generar igualmente, pero SIMDE lo '
              'rechazará con ERRRAW.', file=sys.stderr)
    else:
        print('  OK: ninguna violación RAW detectada.')

    print(f'Emitiendo {vliw_path}...')
    emitir_vliw(instrucciones, asign, et2il, n_il, vliw_path)
    print('Listo.')


if __name__ == '__main__':
    main()