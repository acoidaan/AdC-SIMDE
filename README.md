# Equipo: Mono Núcleo

# Algoritmo: Insertion Sort (punto flotante)

# Práctica: AC2526 - Máquina Superescalar (SIMDE)

## ARCHIVOS ENTREGADOS

### 1. insertion.pla

   Código ensamblador SIMDE — Insertion Sort con punto flotante.

- 28 instrucciones
- Usa LF/SF para cargar/almacenar flotantes
- Usa BGTF para comparar flotantes
- Lee parámetros de memoria:
       MEM[0] = n (tamaño del vector)
       MEM[1] = dirección base del vector origen
       MEM[2] = dirección base del vector destino
- Solo modifica el vector DESTINO; el origen queda intacto
- Primero copia origen → destino, luego ordena destino in-place

### 2. insertion_sequential.cpp

   Simulador secuencial en C++ (estilo Google C++).

- Replica instrucción por instrucción el insertion.pla
- Cuenta instrucciones por tipo: Integer Add, Memory, Jump
- Calcula ciclos secuenciales con las latencias por defecto de SIMDE:
       Integer Add (ADD/ADDI): 1 ciclo
       Memory (LW/LF/SW/SF):  4 ciclos
       Jump (BNE/BEQ/BGTF):   2 ciclos
- Incluye los 6 vectores oficiales (order0.mem a order5.mem)
- Verifica automáticamente que el resultado está bien ordenado
- Compilar: g++ -std=c++17 -O2 -o insertion_seq insertion_sequential.cpp
- Ejecutar: ./insertion_seq

### 3. analizar_simde_amdahl_generico.cpp + json.hpp

   Analizador de estadísticas batch de SIMDE.

- Lee el JSON que exporta SIMDE tras una Batch Execution
- Muestra ciclos por replicación, media, mediana y desv. típica
- Calcula fracciones de ejecución por UF (para ley de Amdahl)
- Compilar: g++ -std=c++17 -O2 -o analizar analizar_simde_amdahl_generico.cpp
- Ejecutar: ./analizar batch_stats.json

### 4. Ficheros .mem oficiales (order0.mem a order5.mem)

   Los 6 vectores de prueba del profesor.

- order0.mem: n=8,  mixto (solo para pruebas, NO va al Excel)
- order1.mem: n=32, ya ordenado      → Ejemplo 1 del Excel
- order2.mem: n=32, invertido        → Ejemplo 2 del Excel
- order3.mem: n=32, aleatorio        → Ejemplo 3 del Excel
- order4.mem: n=32, casi ordenado    → Ejemplo 4 del Excel
- order5.mem: n=32, repetidos        → Ejemplo 5 del Excel

### 5. Excel de la hoja compartida

   Valores de ciclos secuenciales reportados:

- Ejemplo 1 (order1, ordenado):       1179 ciclos
- Ejemplo 2 (order2, invertido):      8836 ciclos
- Ejemplo 3 (order3, aleatorio):      5120 ciclos
- Ejemplo 4 (order4, casi ordenado):  1691 ciclos
- Ejemplo 5 (order5, repetidos):      4002 ciclos

## CÓMO VERIFICAR

### Paso 1: Comprobar que ordena correctamente

   Cargar insertion.pla en SIMDE junto con cualquier order*.mem.
   Ejecutar y comprobar que el vector destino queda ordenado
   de menor a mayor. El vector origen NO debe modificarse.

### Paso 2: Verificar ciclos secuenciales

   Compilar y ejecutar insertion_sequential.cpp.
   Los ciclos deben coincidir con los reportados arriba.
   El programa también verifica que el resultado es correcto
   para cada vector.

### Paso 3: Comprobar en SIMDE sin caché

   Ejecutar insertion.pla en SIMDE con 0% cache fault y
   configuración por defecto. Comparar el número de ciclos
   del superescalar con los ciclos secuenciales para calcular
   la aceleración.

## NOTAS IMPORTANTES

- El código NO usa multiplicación entera (MULT), ni operaciones
  de punto flotante (ADDF/SUBF/MULTF). Solo usa Integer Add,
  Memory y Jump. Las UF de mult entera, float add y float mult
  tienen 0% de utilización.
