#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "json.hpp"

using json = nlohmann::json;

struct UFStats {
  long long instances = 0;
  long long committed = 0;
  long long prefetch = 0;
  long long decode = 0;
  long long issue = 0;
  long long execute = 0;
  long long writeback = 0;
};

static std::string trim(const std::string& s) {
  const auto b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  const auto e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

static std::unordered_map<int, std::string> default_map_for_current_program() {
  // Mapa para el insertion sort FLOTANTE de 28 instrucciones:
  //
  //  0: LW R1 0(R0)          → Memory
  //  1: LW R2 1(R0)          → Memory
  //  2: LW R3 2(R0)          → Memory
  //  3: ADD R4 R2 R0         → Integer Add
  //  4: ADD R5 R3 R0         → Integer Add
  //  5: ADD R6 R2 R1         → Integer Add
  //  6: LF F1 0(R4)          → Memory
  //  7: SF F1 0(R5)          → Memory
  //  8: ADDI R4 R4 #1        → Integer Add
  //  9: ADDI R5 R5 #1        → Integer Add
  // 10: BNE R4 R6 COPY       → Jump
  // 11: ADDI R10 R0 #1       → Integer Add
  // 12: ADD R8 R3 R10        → Integer Add
  // 13: ADD R9 R3 R1         → Integer Add
  // 14: BEQ R8 R9 END        → Jump
  // 15: LF F2 0(R8)          → Memory
  // 16: ADD R12 R8 R0        → Integer Add
  // 17: BEQ R12 R3 INSERT    → Jump
  // 18: ADDI R13 R12 #-1     → Integer Add
  // 19: LF F3 0(R13)         → Memory
  // 20: BGTF F3 F2 SHIFT     → Jump
  // 21: BEQ R0 R0 INSERT     → Jump
  // 22: SF F3 0(R12)         → Memory
  // 23: ADD R12 R13 R0       → Integer Add
  // 24: BEQ R0 R0 INNER      → Jump
  // 25: SF F2 0(R12)         → Memory
  // 26: ADDI R8 R8 #1        → Integer Add
  // 27: BEQ R0 R0 OUTER      → Jump

  std::unordered_map<int, std::string> m;

  // Integer Add: ADD, ADDI (12 instrucciones)
  for (int id : {3, 4, 5, 8, 9, 11, 12, 13, 16, 18, 23, 26})
    m[id] = "Integer Add";

  // Memory: LW, LF, SF (9 instrucciones)
  for (int id : {0, 1, 2, 6, 7, 15, 19, 22, 25}) m[id] = "Memory";

  // Jump: BNE, BEQ, BGTF (7 instrucciones)
  for (int id : {10, 14, 17, 20, 21, 24, 27}) m[id] = "Jump";

  return m;
}

static bool load_mapping_file(const std::string& path,
                              std::unordered_map<int, std::string>& out) {
  std::ifstream f(path);
  if (!f) return false;

  std::string line;
  int line_no = 0;
  while (std::getline(f, line)) {
    ++line_no;
    line = trim(line);
    if (line.empty() || line[0] == '#') continue;

    char sep = 0;
    for (char c : {',', ';', ' ', '\t'}) {
      if (line.find(c) != std::string::npos) {
        sep = c;
        break;
      }
    }
    if (!sep) {
      std::cerr << "Linea de mapeo invalida (" << line_no << "): " << line
                << "\n";
      return false;
    }

    std::string left, right;
    std::stringstream ss(line);
    if (sep == ' ' || sep == '\t') {
      ss >> left >> right;
    } else {
      std::getline(ss, left, sep);
      std::getline(ss, right);
    }

    left = trim(left);
    right = trim(right);

    if (left.empty() || right.empty()) {
      std::cerr << "Linea de mapeo invalida (" << line_no << "): " << line
                << "\n";
      return false;
    }

    try {
      int id = std::stoi(left);
      out[id] = right;
    } catch (...) {
      std::cerr << "InstructionId no valido en linea " << line_no << ": "
                << left << "\n";
      return false;
    }
  }

  return true;
}

static double mean(const std::vector<double>& v) {
  if (v.empty()) return 0.0;
  return std::accumulate(v.begin(), v.end(), 0.0) /
         static_cast<double>(v.size());
}

static double stddev_sample(const std::vector<double>& v) {
  if (v.size() < 2) return 0.0;
  const double m = mean(v);
  double acc = 0.0;
  for (double x : v) acc += (x - m) * (x - m);
  return std::sqrt(acc / static_cast<double>(v.size() - 1));
}

static long long extract_max_numeric_key(const json& obj) {
  long long max_key = -1;
  for (auto it = obj.begin(); it != obj.end(); ++it) {
    try {
      long long k = std::stoll(it.key());
      max_key = std::max(max_key, k);
    } catch (...) {
    }
  }
  return max_key;
}

static long long estimate_replication_cycles(const json& rep) {
  // 1) Campo explícito "cycles".
  if (rep.contains("cycles") && rep["cycles"].is_number_integer()) {
    return rep["cycles"].get<long long>();
  }
  if (rep.contains("stats") && rep["stats"].contains("cycles") &&
      rep["stats"]["cycles"].is_number_integer()) {
    return rep["stats"]["cycles"].get<long long>();
  }

  if (!rep.contains("stats")) return 0;
  const auto& stats = rep["stats"];

  // 2) statuses: mapa { ciclo -> contadores }. La clave max = total ciclos.
  if (stats.contains("statuses") && stats["statuses"].is_object()) {
    long long c = extract_max_numeric_key(stats["statuses"]);
    if (c > 0) return c;
  }

  // 3) unitUsage: mapa { unidad -> { ciclo -> uso } }.
  //    Cualquier sub-mapa tiene claves de ciclo; usamos "prefetch".
  if (stats.contains("unitUsage") && stats["unitUsage"].is_object()) {
    const auto& uu = stats["unitUsage"];
    // Buscar la primera sub-entrada que tenga claves numéricas.
    for (auto it = uu.begin(); it != uu.end(); ++it) {
      if (it.value().is_object()) {
        long long c = extract_max_numeric_key(it.value());
        if (c > 0) return c;
      }
    }
  }

  // 4) Fallback: NO usamos instance keys (son IDs, no ciclos).
  //    Solo retornamos el mayor ciclo de vida de una instancia como
  //    cota inferior muy conservadora.
  if (stats.contains("instances") && stats["instances"].is_object()) {
    long long best = 0;
    for (auto it = stats["instances"].begin(); it != stats["instances"].end();
         ++it) {
      const auto& obj = it.value();
      long long life =
          obj.value("prefetchCycles", 0) + obj.value("decodeCycles", 0) +
          obj.value("issueCycles", 0) + obj.value("executeCycles", 0) +
          obj.value("writeBackCycles", 0);
      best = std::max(best, life);
    }
    if (best > 0) {
      std::cerr << "AVISO: ciclos estimados por fallback (cota inferior), "
                   "puede no ser preciso.\n";
    }
    return best;
  }

  return 0;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Uso:\n"
              << "  " << argv[0] << " <batch_stats.json> [mapa.txt]\n\n"
              << "Ejemplos:\n"
              << "  " << argv[0] << " batch_stats.json\n"
              << "  " << argv[0] << " batch_stats.json mapa.txt\n\n"
              << "Sin mapa.txt usa el mapa por defecto del insertion sort\n"
              << "flotante de 28 instrucciones.\n"
              << "Formato mapa.txt: instructionId,tipoUF\n"
              << "Ejemplo:\n"
              << "  0,Memory\n"
              << "  3,Integer Add\n"
              << "  10,Jump\n";
    return 1;
  }

  const std::string json_path = argv[1];

  std::unordered_map<int, std::string> id_to_group;
  if (argc >= 3) {
    if (!load_mapping_file(argv[2], id_to_group)) {
      std::cerr << "No se pudo cargar el archivo de mapeo: " << argv[2] << "\n";
      return 1;
    }
  } else {
    id_to_group = default_map_for_current_program();
  }

  std::ifstream f(json_path);
  if (!f) {
    std::cerr << "No se pudo abrir el JSON: " << json_path << "\n";
    return 1;
  }

  json root;
  try {
    f >> root;
  } catch (const std::exception& e) {
    std::cerr << "JSON invalido: " << e.what() << "\n";
    return 1;
  }

  if (!root.is_array()) {
    std::cerr << "Se esperaba un array de replicaciones en el JSON.\n";
    return 1;
  }

  std::map<std::string, UFStats> totals;
  std::vector<double> replication_cycles;

  long long unknown_insts = 0;
  std::map<int, long long> unknown_ids_count;

  for (const auto& rep : root) {
    replication_cycles.push_back(
        static_cast<double>(estimate_replication_cycles(rep)));

    if (!rep.contains("stats") || !rep["stats"].contains("instances")) continue;
    const auto& instances = rep["stats"]["instances"];

    for (auto it = instances.begin(); it != instances.end(); ++it) {
      const auto& obj = it.value();
      const int instructionId = obj.value("instructionId", -1);

      auto map_it = id_to_group.find(instructionId);
      if (map_it == id_to_group.end()) {
        ++unknown_insts;
        ++unknown_ids_count[instructionId];
        continue;
      }

      UFStats& s = totals[map_it->second];
      s.instances++;
      if (obj.value("commited", false)) s.committed++;
      s.prefetch += obj.value("prefetchCycles", 0);
      s.decode += obj.value("decodeCycles", 0);
      s.issue += obj.value("issueCycles", 0);
      s.execute += obj.value("executeCycles", 0);
      s.writeback += obj.value("writeBackCycles", 0);
    }
  }

  const double reps = static_cast<double>(root.size());

  std::cout << std::fixed << std::setprecision(3);
  std::cout << "=== ANALISIS SIMDE / AMDAHL ===\n";
  std::cout << "Archivo: " << json_path << "\n";
  std::cout << "Replicaciones detectadas: " << root.size() << "\n";
  std::cout << "Ciclos medios estimados: " << mean(replication_cycles) << "\n";
  std::cout << "Desviacion tipica muestral: "
            << stddev_sample(replication_cycles) << "\n";
  if (!replication_cycles.empty()) {
    const auto [mn_it, mx_it] = std::minmax_element(replication_cycles.begin(),
                                                    replication_cycles.end());
    std::cout << "Min/Max ciclos: " << *mn_it << " / " << *mx_it << "\n";

    // Mediana
    std::vector<double> sorted_cycles = replication_cycles;
    std::sort(sorted_cycles.begin(), sorted_cycles.end());
    double mediana = 0.0;
    size_t sz = sorted_cycles.size();
    if (sz % 2 == 0)
      mediana = (sorted_cycles[sz / 2 - 1] + sorted_cycles[sz / 2]) / 2.0;
    else
      mediana = sorted_cycles[sz / 2];
    std::cout << "Mediana ciclos: " << mediana << "\n";
  }
  std::cout << "\n";

  // --- Detalle por replicación ---
  std::cout << "=== CICLOS POR REPLICACION ===\n";
  for (size_t i = 0; i < replication_cycles.size(); ++i) {
    printf("  Repl %3zu: %8.0f ciclos\n", i + 1, replication_cycles[i]);
  }
  std::cout << "  ---------------------------------\n";
  printf("  MEDIA:    %8.1f ciclos\n", mean(replication_cycles));
  printf("  MEDIANA:  %8.0f ciclos\n", [&]() {
    std::vector<double> sc = replication_cycles;
    std::sort(sc.begin(), sc.end());
    size_t sz = sc.size();
    if (sz == 0) return 0.0;
    return (sz % 2 == 0) ? (sc[sz / 2 - 1] + sc[sz / 2]) / 2.0 : sc[sz / 2];
  }());
  printf("  DESV.TIP: %8.1f ciclos\n", stddev_sample(replication_cycles));
  std::cout << "\n";

  if (unknown_insts > 0) {
    std::cout << "ADVERTENCIA: hay " << unknown_insts
              << " instancias dinamicas sin grupo asignado.\n";
    std::cout << "InstructionIds sin mapear:\n";
    for (const auto& [id, count] : unknown_ids_count) {
      std::cout << "  id=" << id << " -> " << count << " instancias\n";
    }
    std::cout << "\n";
  }

  long long total_execute = 0;
  for (const auto& [group, s] : totals) total_execute += s.execute;

  std::cout << "=== RESUMEN POR UNIDAD FUNCIONAL ===\n";
  std::cout << std::left << std::setw(16) << "UF" << std::right << std::setw(14)
            << "Inst/repl" << std::setw(14) << "Commit/repl" << std::setw(12)
            << "%Commit" << std::setw(16) << "Exec/repl" << std::setw(12)
            << "f_exec"
            << "\n";

  for (const auto& [group, s] : totals) {
    const double inst_per_rep = s.instances / reps;
    const double com_per_rep = s.committed / reps;
    const double commit_rate =
        (s.instances == 0)
            ? 0.0
            : (100.0 * s.committed / static_cast<double>(s.instances));
    const double exec_per_rep = s.execute / reps;
    const double f_exec =
        (total_execute == 0) ? 0.0
                             : (s.execute / static_cast<double>(total_execute));

    std::cout << std::left << std::setw(16) << group << std::right
              << std::setw(14) << inst_per_rep << std::setw(14) << com_per_rep
              << std::setw(12) << commit_rate << std::setw(16) << exec_per_rep
              << std::setw(12) << f_exec << "\n";
  }

  std::cout << "\n=== DETALLE MEDIO POR FASE ===\n";
  std::cout << std::left << std::setw(16) << "UF" << std::right << std::setw(12)
            << "Prefetch" << std::setw(12) << "Decode" << std::setw(12)
            << "Issue" << std::setw(12) << "Execute" << std::setw(12)
            << "WriteBk"
            << "\n";

  for (const auto& [group, s] : totals) {
    std::cout << std::left << std::setw(16) << group << std::right
              << std::setw(12) << (s.prefetch / reps) << std::setw(12)
              << (s.decode / reps) << std::setw(12) << (s.issue / reps)
              << std::setw(12) << (s.execute / reps) << std::setw(12)
              << (s.writeback / reps) << "\n";
  }

  std::cout << "\n=== NOTA PARA AMDAHL ===\n";
  std::cout << "Usa f_exec como fraccion mejorable si analizas reduccion de "
               "latencia de una UF.\n";
  std::cout << "Formula: Speedup = 1 / ((1 - f) + f / k)\n";

  return 0;
}