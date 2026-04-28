# Planificador VLIW automático para SIMDE

Conjunto de scripts para automatizar el apartado b) de la práctica.

## Estructura

- `vliw_scheduler.py` — convierte un `.pla` en un `.vliw` aplicando
  list scheduling greedy con prioridad por altura. Respeta latencias,
  conflictos de unidad funcional y dependencias RAW/WAR/WAW.
- `run_vliw_batch.py` — ejecuta el simulador `simdeVLIWLite.jar` sobre
  los 5 ficheros `order*.mem` con dos configuraciones (sin fallos y
  con 10% de fallos de caché) y muestra una tabla de resultados.

## Flujo de trabajo

Coloca en una misma carpeta:

- los dos `.jar` del aula virtual (`simdeVLIWLite.jar` y el otro)
- los 5 ficheros `order1.mem`...`order5.mem`
- tu `insertion_optimized.pla`
- los dos scripts (`vliw_scheduler.py`, `run_vliw_batch.py`)

Después:

```bash
# 1. Generar el .vliw a partir del .pla
python3 vliw_scheduler.py insertion_optimized.pla insertion_optimized.vliw

# 2. Ejecutar el simulador con todos los .mem en batch
python3 run_vliw_batch.py insertion_optimized 30
```

El segundo comando lanza 30 réplicas para el caso de 10% de fallos
(como pide el guion) y muestra el promedio.

## Configuración de la máquina (por defecto)

Latencias y número de unidades funcionales según la memoria del
proyecto SIMDE (Tabla 4-2):

| Tipo UF       | Latencia | Número | Instrucciones |
|---------------|----------|--------|---------------|
| 0 SumaEnt     | 1        | 2      | ADD, ADDI, SUB, OR, AND, XOR, NOR, SLLV, SRLV |
| 1 MultEnt     | 2        | 2      | MULT |
| 2 SumaFlot    | 4        | 2      | ADDF, SUBF |
| 3 MultFlot    | 6        | 2      | MULTF |
| 4 Memoria     | 4 (9 fallo) | 2   | LF, SF, LW, SW |
| 5 Salto       | 2        | 1      | BNE, BEQ, BGT, BGTF |

## Algoritmo

1. **Parser de `.pla`**: tokeniza líneas, identifica instrucciones y
   etiquetas, ignora comentarios.
2. **Análisis de registros**: para cada instrucción determina qué
   registros lee y escribe.
3. **Identificación de bloques básicos**: separa el código en BBs
   delimitados por etiquetas-destino y saltos.
4. **Construcción del DAG de dependencias** (por bloque):
   - **RAW**: B lee lo que A escribió.
   - **WAW**: B escribe lo que A escribió.
   - **WAR**: B escribe lo que A leyó (importante en VLIW: una
     operación previa puede estar leyendo el registro durante toda su
     latencia, así que escribir antes lo corrompería).
5. **List scheduling greedy**: en cada ciclo, asigna las instrucciones
   listas (todas sus dependencias terminadas) a UFs libres, ordenadas
   por altura (longitud del camino más largo hasta el final del DAG).
6. **Emisión del `.vliw`**: cada ciclo es una IL. Los ciclos sin
   instrucciones útiles se emiten como ILs vacías (con `0` operaciones)
   porque el simulador emite exactamente una IL por ciclo, así que un
   hueco vacío es necesario para respetar las latencias.

## Limitaciones conocidas

El planificador es **conservador**: no aplica predicación ni salto
retardado. Tras cada salto deja `latencia_salto` ciclos de margen
antes del siguiente bloque básico (por la semántica conservadora del
modelo). Estas optimizaciones se pueden añadir editando el `.vliw`
generado a mano.

Tampoco hace **software pipelining a nivel VLIW** (planificar una
iteración del bucle como una pieza de varias iteraciones solapadas);
solo respeta el pipelining que ya hayas hecho en el `.pla`.
