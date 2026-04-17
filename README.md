# AdC-SIMDE — Práctica Superescalar

Implementación de algoritmos de ordenamiento en ensamblador SIMDE para la asignatura de Arquitectura de Computadores (2025-2026).

## Algoritmo final: Insertion Sort (punto flotante)

> **Revisores:** todo lo necesario está en la carpeta [`insertion_sort/`](./insertion_sort/).

### Archivos principales

| Archivo | Descripción |
|---|---|
| [`insertion_sort/insertion.pla`](./insertion_sort/insertion.pla) | Código ensamblador SIMDE (28 instrucciones) |
| [`insertion_sort/src/insertion_sequential.cpp`](./insertion_sort/src/insertion_sequential.cpp) | Calculador de ciclos secuenciales (C++17) |
| [`insertion_sort/src/analizar_simde_amdahl_generico.cpp`](./insertion_sort/src/analizar_simde_amdahl_generico.cpp) | Analizador de estadísticas batch / Amdahl (C++17) |
| [`insertion_sort/src/json.hpp`](./insertion_sort/src/json.hpp) | Librería JSON (nlohmann, dependencia del analizador) |
| [`insertion_sort/mem/order0.mem` … `order5.mem`](./insertion_sort/mem/) | 6 vectores de prueba oficiales |

### Cómo funciona el programa

El programa lee tres parámetros de memoria:

```
MEM[0] = n          (tamaño del vector)
MEM[1] = dir_origen (dirección base del vector fuente)
MEM[2] = dir_destino(dirección base del vector destino)
```

1. **Copia** el vector origen al destino usando `LF`/`SF` (flotantes)
2. **Ordena** el destino in-place con Insertion Sort, comparando con `BGTF`
3. El vector **origen NO se modifica**

> **Nota:** `BGTF` (branch if greater than, float) fue confirmada por el profesor Iván Castilla el 7 de abril de 2026 como instrucción disponible en SIMDE.

### Ciclos secuenciales (para el Excel)

| Ejemplo | Fichero | Tipo | Ciclos |
|---|---|---|---|
| 1 | `order1.mem` | Ordenado (mejor caso) | **1179** |
| 2 | `order2.mem` | Invertido (peor caso) | **8836** |
| 3 | `order3.mem` | Aleatorio | **5120** |
| 4 | `order4.mem` | Casi ordenado | **1691** |
| 5 | `order5.mem` | Repetidos | **4002** |

`order0.mem` (n=8) es solo para pruebas y no se incluye en el Excel.

### Guía de verificación

#### 1. Comprobar que ordena correctamente

Cargar `insertion.pla` + cualquier `order*.mem` en SIMDE → ejecutar → el vector destino debe quedar ordenado de menor a mayor. El vector origen debe permanecer intacto.

#### 2. Verificar ciclos secuenciales

```bash
cd insertion_sort/src
g++ -std=c++17 -O2 -o insertion_seq insertion_sequential.cpp
./insertion_seq
```

El programa simula instrucción por instrucción el `.pla`, calcula los ciclos con las latencias por defecto de SIMDE y verifica que cada vector queda correctamente ordenado.

#### 3. Analizar batch execution (opcional)

```bash
cd insertion_sort/src
g++ -std=c++17 -O2 -o analizar analizar_simde_amdahl_generico.cpp
./analizar batch_stats.json
```

### Desglose de instrucciones

El programa usa **3 tipos** de unidades funcionales:

| Tipo | Instrucciones | Latencia |
|---|---|---|
| Integer Add | `ADD`, `ADDI` (12 instr. estáticas) | 1 ciclo |
| Memory | `LW`, `LF`, `SF` (9 instr. estáticas) | 4 ciclos |
| Jump | `BNE`, `BEQ`, `BGTF` (7 instr. estáticas) | 2 ciclos |

**No se usan:** Integer Multiply, Float Add, Float Multiply (0% utilización).

### Latencias por defecto de SIMDE

| Unidad funcional | Latencia (ciclos) |
|---|---|
| Integer Add | 1 |
| Integer Multiply | 2 |
| Float Add | 4 |
| Float Multiply | 6 |
| Memory | 4 |
| Jump/Branch | 2 |
