// Simulador secuencial de ciclos para Insertion Sort (versión flotante) en
// SIMDE Replica instrucción por instrucción el insertion.pla corregido con
// LF/SF y BGTF, contando cada tipo de instrucción ejecutada y calculando los
// ciclos totales con las latencias por defecto de SIMDE.
//
// Usa los 6 vectores oficiales (order0.mem … order5.mem).
//
// Latencias por defecto (ejecución secuencial):
//   Integer Add  (ADD, ADDI, SUB) : 1 ciclo
//   Integer Mult (MULT)           : 2 ciclos
//   Float Add    (ADDF, SUBF)     : 4 ciclos
//   Float Mult   (MULTF)          : 6 ciclos
//   Memory       (LW, SW, LF, SF): 4 ciclos
//   Jump/Branch  (BNE, BEQ, BGT,
//                 BGTF, BNEF, BEQF): 2 ciclos
//
// Compilar: g++ -std=c++17 -O2 -o insertion_seq insertion_sequential.cpp
// Ejecutar: ./insertion_seq

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

// ─── Latencias por defecto de SIMDE ─────────────────────────────
constexpr int kLatIntAdd = 1;
constexpr int kLatIntMult = 2;
constexpr int kLatFpAdd = 4;
constexpr int kLatFpMult = 6;
constexpr int kLatMemory = 4;
constexpr int kLatJump = 2;

// ─── Contadores de instrucciones ────────────────────────────────
struct Stats {
  int int_add_count = 0;   // ADD, ADDI, SUB
  int int_mult_count = 0;  // MULT
  int fp_add_count = 0;    // ADDF, SUBF
  int fp_mult_count = 0;   // MULTF
  int memory_count = 0;    // LW, SW, LF, SF
  int jump_count = 0;      // BNE, BEQ, BGT, BGTF, BNEF, BEQF

  int TotalInstructions() const {
    return int_add_count + int_mult_count + fp_add_count + fp_mult_count +
           memory_count + jump_count;
  }

  int TotalCycles() const {
    return int_add_count * kLatIntAdd + int_mult_count * kLatIntMult +
           fp_add_count * kLatFpAdd + fp_mult_count * kLatFpMult +
           memory_count * kLatMemory + jump_count * kLatJump;
  }

  void PrintAmdahl() const {
    int total = TotalCycles();
    if (total == 0) return;
    std::cout << "\n  --- Fracciones de tiempo (para Amdahl) ---\n";
    auto Frac = [&](const std::string& name, int count, int lat) {
      double f = static_cast<double>(count * lat) / total;
      printf("  %-23s: %6.2f%%\n", name.c_str(), f * 100.0);
    };
    Frac("Integer Add", int_add_count, kLatIntAdd);
    Frac("Integer Mult", int_mult_count, kLatIntMult);
    Frac("Float Add", fp_add_count, kLatFpAdd);
    Frac("Float Mult", fp_mult_count, kLatFpMult);
    Frac("Memory", memory_count, kLatMemory);
    Frac("Jump/Branch", jump_count, kLatJump);
  }

  void Print() const {
    std::cout << "\n  ========== RESULTADOS SECUENCIALES ==========\n";
    std::cout << "  Tipo instruccion       | Cuenta | Latencia | Ciclos\n";
    std::cout << "  -----------------------+--------+----------+-------\n";

    auto Row = [](const std::string& name, int count, int lat) {
      int cycles = count * lat;
      printf("  %-23s| %6d | %8d | %6d\n", name.c_str(), count, lat, cycles);
    };

    Row("Integer Add (ADD/ADDI)", int_add_count, kLatIntAdd);
    Row("Integer Mult (MULT)", int_mult_count, kLatIntMult);
    Row("Float Add (ADDF/SUBF)", fp_add_count, kLatFpAdd);
    Row("Float Mult (MULTF)", fp_mult_count, kLatFpMult);
    Row("Memory (LW/SW/LF/SF)", memory_count, kLatMemory);
    Row("Jump (BNE/BEQ/BGT/BGTF)", jump_count, kLatJump);

    std::cout << "  -----------------------+--------+----------+-------\n";
    printf("  %-23s| %6d |          | %6d\n", "TOTAL", TotalInstructions(),
           TotalCycles());
    std::cout << "  ================================================\n";

    PrintAmdahl();
  }
};

void IntAdd(Stats& s) { s.int_add_count++; }
void Memory(Stats& s) { s.memory_count++; }
void Jump(Stats& s) { s.jump_count++; }

// ─── Caso de prueba (.mem) ──────────────────────────────────────
struct MemFile {
  std::string name;
  int n;
  int ptr_src;
  int ptr_dest;
  std::vector<double> values;
};

// ─── Simulador ──────────────────────────────────────────────────
// Replica EXACTAMENTE la secuencia de instrucciones del insertion.pla
// versión flotante (LF/SF + BGTF).
Stats Simulate(const MemFile& mf) {
  const int n = mf.n;
  const int ptr_src = mf.ptr_src;
  const int ptr_dest = mf.ptr_dest;

  std::vector<double> mem(256, 0.0);

  // GPR (R0 siempre = 0).
  double R[33] = {0};
  // FPR.
  double F[33] = {0};

  mem[0] = n;
  mem[1] = ptr_src;
  mem[2] = ptr_dest;
  for (int i = 0; i < n; ++i) {
    mem[ptr_src + i] = mf.values[i];
  }

  Stats s;

  // ============================================================
  // FASE 1: Cargar parámetros
  // ============================================================
  R[1] = mem[(int)R[0] + 0];
  Memory(s);  // LW R1 0(R0)     — n
  R[2] = mem[(int)R[0] + 1];
  Memory(s);  // LW R2 1(R0)     — src
  R[3] = mem[(int)R[0] + 2];
  Memory(s);  // LW R3 2(R0)     — dest

  // ============================================================
  // FASE 2: Copiar origen a destino (flotantes)
  // ============================================================
  R[4] = R[2] + R[0];
  IntAdd(s);  // ADD R4 R2 R0
  R[5] = R[3] + R[0];
  IntAdd(s);  // ADD R5 R3 R0
  R[6] = R[2] + R[1];
  IntAdd(s);  // ADD R6 R2 R1

  // COPY:
  do {
    F[1] = mem[(int)R[4] + 0];
    Memory(s);  // LF F1 0(R4)
    mem[(int)R[5] + 0] = F[1];
    Memory(s);  // SF F1 0(R5)
    R[4] = R[4] + 1;
    IntAdd(s);  // ADDI R4 R4 #1
    R[5] = R[5] + 1;
    IntAdd(s);  // ADDI R5 R5 #1
    Jump(s);    // BNE R4 R6 COPY
  } while (R[4] != R[6]);

  // ============================================================
  // FASE 3: Insertion Sort (flotante)
  // ============================================================
  R[10] = R[0] + 1;
  IntAdd(s);  // ADDI R10 R0 #1

  R[8] = R[3] + R[10];
  IntAdd(s);  // ADD R8 R3 R10   — i = dest+1
  R[9] = R[3] + R[1];
  IntAdd(s);  // ADD R9 R3 R1    — end = dest+n

  // OUTER:
  while (true) {
    // BEQ R8 R9 END
    Jump(s);
    if (R[8] == R[9]) break;

    // LF F2 0(R8)          — key = A[i]
    F[2] = mem[(int)R[8] + 0];
    Memory(s);

    // ADD R12 R8 R0        — j = i
    R[12] = R[8] + R[0];
    IntAdd(s);

    // INNER:
    while (true) {
      // BEQ R12 R3 INSERT
      Jump(s);
      if (R[12] == R[3]) break;

      // ADDI R13 R12 #-1
      R[13] = R[12] + (-1);
      IntAdd(s);

      // LF F3 0(R13)        — A[j-1]
      F[3] = mem[(int)R[13] + 0];
      Memory(s);

      // BGTF F3 F2 SHIFT
      Jump(s);
      if (F[3] > F[2]) {
        // — Camino SHIFT —
        // SF F3 0(R12)
        mem[(int)R[12] + 0] = F[3];
        Memory(s);

        // ADD R12 R13 R0
        R[12] = R[13] + R[0];
        IntAdd(s);

        // BEQ R0 R0 INNER
        Jump(s);
        continue;
      }

      // BEQ R0 R0 INSERT
      Jump(s);
      break;
    }

    // INSERT:
    // SF F2 0(R12)
    mem[(int)R[12] + 0] = F[2];
    Memory(s);

    // ADDI R8 R8 #1
    R[8] = R[8] + 1;
    IntAdd(s);

    // BEQ R0 R0 OUTER
    Jump(s);
  }

  // ============================================================
  // Verificación: vector destino ordenado correctamente.
  // ============================================================
  std::vector<double> result(n);
  for (int i = 0; i < n; ++i) {
    result[i] = mem[ptr_dest + i];
  }

  std::vector<double> expected = mf.values;
  std::sort(expected.begin(), expected.end());

  bool ok = true;
  for (int i = 0; i < n; ++i) {
    if (std::abs(result[i] - expected[i]) > 1e-9) {
      ok = false;
      break;
    }
  }

  if (!ok) {
    std::cerr << "ERROR en " << mf.name
              << ": vector destino NO ordenado correctamente.\n";
    std::cerr << "  Esperado: ";
    for (double v : expected) std::cerr << v << " ";
    std::cerr << "\n  Obtenido: ";
    for (double v : result) std::cerr << v << " ";
    std::cerr << "\n";
    std::exit(1);
  }

  return s;
}

// ─── 6 ficheros oficiales ───────────────────────────────────────
std::vector<MemFile> BuildOfficialTests() {
  std::vector<MemFile> tests;

  tests.push_back({"order0.mem (n=8, mixto)",
                   8,
                   10,
                   18,
                   {108.2, 10.5, 99.56, 8.5, 8.3, 73.75, 6.7, 56.67}});

  tests.push_back({"order1.mem (n=32, ordenado)",
                   32,
                   10,
                   42,
                   {1.12,  2.45,  3.78,  4.01,  5.67,  6.89,  7.23,  8.54,
                    9.11,  10.33, 11.88, 12.02, 13.44, 14.56, 15.77, 16.21,
                    17.89, 18.34, 19.56, 20.01, 21.44, 22.67, 23.89, 24.12,
                    25.56, 26.78, 27.91, 28.34, 29.56, 30.11, 31.45, 32.88}});

  tests.push_back(
      {"order2.mem (n=32, invertido)",
       32,
       10,
       42,
       {99.5, 95.2, 91.8, 88.4, 85.1, 82.7, 79.3, 76.0, 73.6, 70.2, 67.9,
        64.5, 61.1, 58.8, 55.4, 52.1, 49.7, 46.3, 43.0, 40.6, 37.2, 34.9,
        31.5, 28.1, 25.8, 22.4, 19.1, 16.7, 13.3, 10.0, 7.6,  4.2}});

  tests.push_back({"order3.mem (n=32, aleatorio)",
                   32,
                   10,
                   42,
                   {45.23, 12.89, 78.41, 3.56,  56.12, 89.04, 21.77, 67.32,
                    9.15,  34.66, 82.01, 15.48, 94.73, 41.29, 27.85, 63.11,
                    5.92,  51.04, 73.28, 18.66, 39.44, 87.21, 10.55, 60.98,
                    2.33,  98.12, 31.47, 55.67, 14.22, 71.03, 48.59, 25.14}});

  tests.push_back({"order4.mem (n=32, casi ordenado)",
                   32,
                   10,
                   42,
                   {1.05,  2.15,  3.25,  4.35,  15.88, 6.55,  7.65,  8.75,
                    9.85,  10.95, 11.05, 12.15, 13.25, 14.35, 5.45,  16.55,
                    17.65, 18.75, 19.85, 20.95, 21.05, 22.15, 23.25, 24.35,
                    32.95, 26.55, 27.65, 28.75, 29.85, 30.95, 31.05, 25.45}});

  tests.push_back({"order5.mem (n=32, repetidos)",
                   32,
                   10,
                   42,
                   {5.5, 5.5, 1.2, 8.8, 5.5, 1.2, 8.8, 8.8, 5.5, 1.2, 5.5,
                    5.5, 8.8, 1.2, 1.2, 5.5, 8.8, 5.5, 1.2, 8.8, 5.5, 1.2,
                    5.5, 8.8, 8.8, 1.2, 5.5, 1.2, 8.8, 5.5, 5.5, 1.2}});

  return tests;
}

// ─── Main ───────────────────────────────────────────────────────
int main() {
  std::cout << "============================================================\n";
  std::cout << " Simulador Secuencial - Insertion Sort FLOTANTE\n";
  std::cout << " (insertion.pla con LF/SF + BGTF) — 28 instrucciones\n";
  std::cout << " 6 vectores oficiales de la competicion\n";
  std::cout << "============================================================\n";

  auto tests = BuildOfficialTests();

  struct Summary {
    std::string name;
    int n;
    int instructions;
    int cycles;
    int int_add;
    int memory;
    int jump;
  };
  std::vector<Summary> summaries;

  for (const auto& mf : tests) {
    std::cout << "\n>>> " << mf.name << "\n";
    std::cout << "    Valores: ";
    for (size_t i = 0; i < mf.values.size(); ++i) {
      if (i > 0) std::cout << ", ";
      std::cout << mf.values[i];
    }
    std::cout << "\n";

    Stats s = Simulate(mf);
    s.Print();

    summaries.push_back({mf.name, mf.n, s.TotalInstructions(), s.TotalCycles(),
                         s.int_add_count, s.memory_count, s.jump_count});
  }

  // ─── Tabla resumen ──────────────────────────────────────────
  std::cout << "\n\n";
  std::cout << "============================================================\n";
  std::cout << "              RESUMEN - TODOS LOS VECTORES\n";
  std::cout << "============================================================\n";
  printf("  %-40s | %3s | %5s | %6s\n", "Fichero", "n", "Instr", "Ciclos");
  std::cout
      << "  ----------------------------------------+-----+-------+-------\n";
  for (const auto& sm : summaries) {
    printf("  %-40s | %3d | %5d | %6d\n", sm.name.c_str(), sm.n,
           sm.instructions, sm.cycles);
  }

  std::cout << "\n  --- Desglose por tipo (para el Excel) ---\n";
  printf("  %-40s | %6s | %6s | %6s | %6s | %6s\n", "Fichero", "IntAdd",
         "Memory", "Jump", "Instr", "Ciclos");
  std::cout << "  "
               "----------------------------------------+--------+--------+----"
               "----+--------+-------\n";
  for (const auto& sm : summaries) {
    printf("  %-40s | %6d | %6d | %6d | %6d | %6d\n", sm.name.c_str(),
           sm.int_add, sm.memory, sm.jump, sm.instructions, sm.cycles);
  }

  std::cout << "\n*** Todos los vectores ordenados correctamente. ***\n";
  return 0;
}