// Verificador del Selection Sort en ensamblador SIMDE
// Simula las operaciones registro a registro para comprobar
// que el .simde produce el resultado correcto.

#include <cassert>
#include <iostream>
#include <vector>

int main() {
  // Simular memoria SIMDE (1024 palabras de 32 bits)
  std::vector<int> mem(1024, 0);
  int R[33] = {0};  // R0..R32, R0 siempre vale 0

  // --- Configurar datos de prueba ---
  int n = 10;
  int ptr_src = 10;
  int ptr_dest = 30;

  mem[0] = n;
  mem[1] = ptr_src;
  mem[2] = ptr_dest;

  // Vector con positivos, negativos y ceros
  int source[] = {5, -3, 0, 8, -1, 7, -8, 2, 4, -5};
  for (int i = 0; i < n; ++i) {
    mem[ptr_src + i] = source[i];
  }

  // --- Ejecutar simulación del ensamblador ---

  // FASE 1: Cargar parámetros
  R[1] = mem[R[0] + 0];  // LW R1 0(R0)    -> n
  R[2] = mem[R[0] + 1];  // LW R2 1(R0)    -> ptr_src
  R[3] = mem[R[0] + 2];  // LW R3 2(R0)    -> ptr_dest

  // FASE 2: Copiar origen a destino
  R[4] = R[2] + R[0];  // ADD R4 R2 R0   -> current_src
  R[5] = R[3] + R[0];  // ADD R5 R3 R0   -> current_dest
  R[6] = R[2] + R[1];  // ADD R6 R2 R1   -> end_src

  // COPY:
  do {
    R[7] = mem[R[4] + 0];  // LW R7 0(R4)
    mem[R[5] + 0] = R[7];  // SW R7 0(R5)
    R[4] = R[4] + 1;       // ADDI R4 R4 #1
    R[5] = R[5] + 1;       // ADDI R5 R5 #1
  } while (R[4] != R[6]);  // BNE R4 R6 COPY

  // FASE 3: Selection Sort sobre destino
  R[17] = R[0] + 1;     // ADDI R17 R0 #1
  R[9] = R[3] + R[1];   // ADD R9 R3 R1
  R[9] = R[9] - R[17];  // SUB R9 R9 R17  -> end_outer
  R[11] = R[3] + R[1];  // ADD R11 R3 R1   -> end_inner
  R[8] = R[3] + R[0];   // ADD R8 R3 R0    -> addr_i

  // OUTER:
  do {
    R[12] = R[8] + R[0];  // ADD R12 R8 R0   -> addr_min = addr_i
    R[10] = R[8] + 1;     // ADDI R10 R8 #1  -> addr_j = addr_i+1

    // INNER:
    do {
      R[13] = mem[R[10] + 0];  // LW R13 0(R10) -> val_j
      R[14] = mem[R[12] + 0];  // LW R14 0(R12) -> val_min
      if (R[14] > R[13]) {     // BGT R14 R13 NEWMIN
        // NEWMIN:
        R[12] = R[10] + R[0];  // ADD R12 R10 R0
      }
      // CONT:
      R[10] = R[10] + 1;  // ADDI R10 R10 #1
    } while (R[10] != R[11]);  // BNE R10 R11 INNER

    if (R[12] != R[8]) {       // BEQ R12 R8 NOSWAP (negado)
      R[15] = mem[R[8] + 0];   // LW R15 0(R8)
      R[16] = mem[R[12] + 0];  // LW R16 0(R12)
      mem[R[8] + 0] = R[16];   // SW R16 0(R8)
      mem[R[12] + 0] = R[15];  // SW R15 0(R12)
    }
    // NOSWAP:
    R[8] = R[8] + 1;  // ADDI R8 R8 #1
  } while (R[8] != R[9]);  // BNE R8 R9 OUTER

  // --- Mostrar resultados ---
  std::cout << "Original:  ";
  for (int i = 0; i < n; ++i) {
    std::cout << mem[ptr_src + i] << " ";
  }
  std::cout << "\n";

  std::cout << "Ordenado:  ";
  for (int i = 0; i < n; ++i) {
    std::cout << mem[ptr_dest + i] << " ";
  }
  std::cout << "\n";

  // --- Verificar que está ordenado ---
  for (int i = 0; i < n - 1; ++i) {
    assert(mem[ptr_dest + i] <= mem[ptr_dest + i + 1]);
  }

  // --- Verificar que el origen NO se modificó ---
  for (int i = 0; i < n; ++i) {
    assert(mem[ptr_src + i] == source[i]);
  }

  std::cout << "Todo correcto!\n";
  return 0;
}