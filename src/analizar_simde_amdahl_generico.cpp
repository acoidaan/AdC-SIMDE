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
  // Mapa por defecto para TU programa actual de 32 instrucciones.
  // Si cambias el ensamblador, usa un archivo de mapeo externo.
  std::unordered_map<int, std::string> m;

  // Integer Add: ADD, ADDI, SUB
  for (int id : {3, 4, 5, 8, 9, 11, 12, 13, 14, 15, 16, 17, 22, 23, 30})
    m[id] = "Integer Add";

  // Memory: LW, SW
  for (int id : {0, 1, 2, 6, 7, 18, 19, 26, 27, 28, 29}) m[id] = "Memory";

  // Jump: BNE, BEQ, BGT
  for (int id : {10, 20, 21, 24, 25, 31}) m[id] = "Jump";

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

    // Formatos aceptados:
    // 6,Memory
    // 6 Memory
    // 6;Memory
    char sep = 0;
    for (char c : {',', ';', ' ', '\t'}) {
      if (line.find(c) != std::string::npos) {
        sep = c;
        break;
      }
    }
    if (!sep) {
      std::cerr << "Línea de mapeo inválida (" << line_no << "): " << line
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
      std::cerr << "Línea de mapeo inválida (" << line_no << "): " << line
                << "\n";
      return false;
    }

    try {
      int id = std::stoi(left);
      out[id] = right;
    } catch (...) {
      std::cerr << "InstructionId no válido en línea " << line_no << ": "
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

static long long estimate_replication_cycles(const json& rep) {
  // Si existe un campo explícito de ciclos, úsalo
  if (rep.contains("cycles") && rep["cycles"].is_number_integer()) {
    return rep["cycles"].get<long long>();
  }
  if (rep.contains("stats") && rep["stats"].contains("cycles") &&
      rep["stats"]["cycles"].is_number_integer()) {
    return rep["stats"]["cycles"].get<long long>();
  }

  // Si no, estimamos por el mayor tiempo de vida observado en las instancias
  long long best = 0;
  if (!rep.contains("stats") || !rep["stats"].contains("instances")) return 0;

  const auto& instances = rep["stats"]["instances"];
  for (auto it = instances.begin(); it != instances.end(); ++it) {
    const auto& obj = it.value();

    long long p = obj.value("prefetchCycles", 0);
    long long d = obj.value("decodeCycles", 0);
    long long is = obj.value("issueCycles", 0);
    long long ex = obj.value("executeCycles", 0);
    long long wb = obj.value("writeBackCycles", 0);

    // La key suele codificar el instante de entrada aproximado en SIMDE batch.
    long long start = 0;
    try {
      start = std::stoll(it.key());
    } catch (...) {
      start = 0;
    }

    best = std::max(best, start + p + d + is + ex + wb);
  }
  return best;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Uso:\n"
              << "  " << argv[0] << " <batch_stats.json> [mapa.txt]\n\n"
              << "Ejemplos:\n"
              << "  " << argv[0] << " batch_stats.json\n"
              << "  " << argv[0] << " batch_stats_50.json mapa.txt\n\n"
              << "Si no pasas mapa.txt, usa el mapa por defecto de tu programa "
                 "actual de 32 instrucciones.\n"
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
    std::cerr << "JSON inválido: " << e.what() << "\n";
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
  std::cout << "=== ANALISIS SIMDE / AMDHAL ===\n";
  std::cout << "Archivo: " << json_path << "\n";
  std::cout << "Replicaciones detectadas: " << root.size() << "\n";
  std::cout << "Ciclos medios estimados: " << mean(replication_cycles) << "\n";
  std::cout << "Desviacion tipica muestral: "
            << stddev_sample(replication_cycles) << "\n";
  if (!replication_cycles.empty()) {
    const auto [mn_it, mx_it] = std::minmax_element(replication_cycles.begin(),
                                                    replication_cycles.end());
    std::cout << "Min/Max ciclos: " << *mn_it << " / " << *mx_it << "\n";
  }
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

  std::cout << "\n=== NOTA PARA AMDHAL ===\n";
  std::cout << "Usa f_exec como fraccion mejorable si analizas reduccion de "
               "latencia de una UF.\n";
  std::cout << "Formula: Speedup = 1 / ((1 - f) + f / k)\n";

  return 0;
}
