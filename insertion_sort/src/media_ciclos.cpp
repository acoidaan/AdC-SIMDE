#include <iostream>
#include <sstream>
#include <string>

int main() {
  std::string line;
  double sum = 0.0;
  int count = 0;

  while (std::getline(std::cin, line)) {
    // Saltar líneas vacías o encabezados
    if (line.empty() || !isdigit(line[0])) continue;

    std::istringstream iss(line);
    int index;
    double value;

    if (iss >> index >> value) {
      sum += value;
      count++;
    }
  }

  if (count > 0) {
    std::cout << "Media: " << (sum / count) << std::endl;
  } else {
    std::cout << "No se leyeron datos válidos.\n";
  }

  return 0;
}