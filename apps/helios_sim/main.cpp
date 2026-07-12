#include "helios/version.hpp"

#include <iostream>

int main() {
  std::cout << "Helios simulator scaffold v" << helios::version() << '\n';
  return 0;
}
