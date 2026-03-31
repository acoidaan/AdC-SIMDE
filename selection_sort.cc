/*
 * Selection Sort para SIMDE
 * Solo modifica el vector destino
 */

#include <iostream>
#include <vector>

void SelectionSort(const int* source, int* dest, int n) {
  // Paso 1: Copiar vector origen al destino
  for (int i = 0; i < n; ++i) {
    dest[i] = source[i];
  }

  // Paso 2: Ordenar destino in-place
  for (int i = 0; i < n; ++i) {
    int min_idx = i;
    for (int j = i + 1; j < n; ++j) {
      if (dest[j] < dest[min_idx]) {
        min_idx = j;
      }
    }

    // Swap solo si el mínimo no está ya en su sitio
    if (min_idx != i) {
      int temp = dest[i];
      dest[i] = dest[min_idx];
      dest[min_idx] = temp;
    }
  }
}

int main() {
  // Simular la estructura de memoria de SIMDE
  // Caso de prueba con positivos, negativos y ceros
  std::vector<int> source = {5, -3, 0, 8, -1, 7, -8, 2, 4, -5};
  int n = static_cast<int>(source.size());
  std::vector<int> dest(n);

  std::cout << "Original: ";
  for (int i = 0; i < n; ++i) {
    std::cout << source[i] << " ";
  }
  std::cout << "\n";

  SelectionSort(source.data(), dest.data(), n);

  std::cout << "Ordenado: ";
  for (int i = 0; i < n; ++i) {
    std::cout << dest[i] << " ";
  }
  std::cout << "\n";

  return 0;
}