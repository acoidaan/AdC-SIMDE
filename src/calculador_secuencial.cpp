// Calculador de ciclos secuenciales para Selection Sort en SIMDE
// Simula instrucción por instrucción, contando ciclos con las
// latencias por defecto de SIMDE.
//
// Latencias por defecto:
//   Integer Add (ADD, ADDI, SUB): 1 ciclo
//   Memory (LW, SW):              4 ciclos
//   Jump (BNE, BEQ, BGT):         2 ciclos

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

constexpr int kLatIntAdd = 1;
constexpr int kLatMemory = 4;
constexpr int kLatJump = 2;

struct Stats {
  int total_cycles = 0;
  int int_add_count = 0;
  int memory_count = 0;
  int jump_count = 0;
  int total_instructions = 0;
};

void IntAdd(Stats& s) {
  s.total_cycles += kLatIntAdd;
  s.int_add_count++;
  s.total_instructions++;
}

void Memory(Stats& s) {
  s.total_cycles += kLatMemory;
  s.memory_count++;
  s.total_instructions++;
}

void Jump(Stats& s) {
  s.total_cycles += kLatJump;
  s.jump_count++;
  s.total_instructions++;
}

Stats SimulateSequential(const std::vector<int>& source) {
  // Preparar memoria
  std::vector<int> mem(1024, 0);
  int R[33] = {0};
  Stats s;

  int n = static_cast<int>(source.size());
  int ptr_src = 10;
  int ptr_dest = 10 + n + 5;

  mem[0] = n;
  mem[1] = ptr_src;
  mem[2] = ptr_dest;
  for (int i = 0; i < n; ++i) {
    mem[ptr_src + i] = source[i];
  }

  // === FASE 1: Cargar parámetros ===
  R[1] = mem[0];
  Memory(s);  // LW R1 0(R0)
  R[2] = mem[1];
  Memory(s);  // LW R2 1(R0)
  R[3] = mem[2];
  Memory(s);  // LW R3 2(R0)

  // === FASE 2: Copiar origen a destino ===
  R[4] = R[2] + R[0];
  IntAdd(s);  // ADD R4 R2 R0
  R[5] = R[3] + R[0];
  IntAdd(s);  // ADD R5 R3 R0
  R[6] = R[2] + R[1];
  IntAdd(s);  // ADD R6 R2 R1

  // COPY loop
  do {
    R[7] = mem[R[4]];
    Memory(s);  // LW R7 0(R4)
    mem[R[5]] = R[7];
    Memory(s);  // SW R7 0(R5)
    R[4] = R[4] + 1;
    IntAdd(s);  // ADDI R4 R4 #1
    R[5] = R[5] + 1;
    IntAdd(s);  // ADDI R5 R5 #1
    Jump(s);    // BNE R4 R6 COPY
  } while (R[4] != R[6]);

  // === FASE 3: Selection Sort ===
  R[17] = 1;
  IntAdd(s);  // ADDI R17 R0 #1
  R[9] = R[3] + R[1];
  IntAdd(s);  // ADD R9 R3 R1
  R[9] = R[9] - R[17];
  IntAdd(s);  // SUB R9 R9 R17
  R[11] = R[3] + R[1];
  IntAdd(s);  // ADD R11 R3 R1
  R[8] = R[3] + R[0];
  IntAdd(s);  // ADD R8 R3 R0

  // OUTER loop
  do {
    R[12] = R[8] + R[0];
    IntAdd(s);  // ADD R12 R8 R0
    R[10] = R[8] + 1;
    IntAdd(s);  // ADDI R10 R8 #1

    // INNER loop
    do {
      R[13] = mem[R[10]];
      Memory(s);  // LW R13 0(R10)
      R[14] = mem[R[12]];
      Memory(s);  // LW R14 0(R12)

      if (R[14] > R[13]) {
        // BGT taken → salta a NEWMIN
        Jump(s);  // BGT R14 R13 NEWMIN
        R[12] = R[10] + R[0];
        IntAdd(s);  // ADD R12 R10 R0
      } else {
        // BGT not taken → ejecuta BEQ incondicional
        Jump(s);  // BGT R14 R13 NEWMIN
        Jump(s);  // BEQ R0 R0 CONT
      }

      // CONT:
      R[10] = R[10] + 1;
      IntAdd(s);  // ADDI R10 R10 #1
      Jump(s);    // BNE R10 R11 INNER
    } while (R[10] != R[11]);

    // Swap check
    if (R[12] != R[8]) {
      Jump(s);  // BEQ R12 R8 NOSWAP (not taken)
      R[15] = mem[R[8]];
      Memory(s);  // LW R15 0(R8)
      R[16] = mem[R[12]];
      Memory(s);  // LW R16 0(R12)
      mem[R[8]] = R[16];
      Memory(s);  // SW R16 0(R8)
      mem[R[12]] = R[15];
      Memory(s);  // SW R15 0(R12)
    } else {
      Jump(s);  // BEQ R12 R8 NOSWAP (taken)
    }

    // NOSWAP:
    R[8] = R[8] + 1;
    IntAdd(s);  // ADDI R8 R8 #1
    Jump(s);    // BNE R8 R9 OUTER
  } while (R[8] != R[9]);

  // Verificar resultado
  for (int i = 0; i < n - 1; ++i) {
    assert(mem[ptr_dest + i] <= mem[ptr_dest + i + 1]);
  }

  return s;
}

void PrintStats(const std::string& name, const std::vector<int>& data,
                int superscalar_cycles = 0) {
  Stats s = SimulateSequential(data);
  std::cout << "=== " << name << " (n=" << data.size() << ") ===\n";
  std::cout << "Ciclos secuenciales totales: " << s.total_cycles << "\n";
  std::cout << "Instrucciones totales:       " << s.total_instructions << "\n";
  std::cout << "  IntAdd (" << kLatIntAdd << " ciclo):   " << s.int_add_count
            << " instrucciones, " << s.int_add_count * kLatIntAdd
            << " ciclos\n";
  std::cout << "  Memory (" << kLatMemory << " ciclos):  " << s.memory_count
            << " instrucciones, " << s.memory_count * kLatMemory << " ciclos\n";
  std::cout << "  Jump   (" << kLatJump << " ciclos):  " << s.jump_count
            << " instrucciones, " << s.jump_count * kLatJump << " ciclos\n";

  if (superscalar_cycles > 0) {
    double speedup = static_cast<double>(s.total_cycles) / superscalar_cycles;
    std::cout << "Ciclos superescalar:         " << superscalar_cycles << "\n";
    std::cout << "Aceleración (seq/super):     " << speedup << "x\n";
  }
  std::cout << "\n";
}

int main() {
  // Caso de prueba: el mismo vector que usamos en SIMDE
  PrintStats("Vector SIMDE", {5, -3, 0, 8, -1, 7, -8, 2, 4, -5}, 528);

  // Otros casos de prueba
  PrintStats("Ya ordenado", {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});

  PrintStats("Invertido (peor caso swaps)", {10, 9, 8, 7, 6, 5, 4, 3, 2, 1});

  PrintStats("Todos iguales", {5, 5, 5, 5, 5, 5, 5, 5, 5, 5});

  // Vector de 5 elementos (por si la competición usa otro tamaño)
  PrintStats("Pequeño n=5", {3, 1, 4, 1, 5});

  // Vector de 20 elementos
  std::vector<int> v20;
  for (int i = 20; i >= 1; --i) v20.push_back(i);
  PrintStats("Invertido n=20", v20);

  return 0;
}