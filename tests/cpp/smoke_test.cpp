#include "helios/version.hpp"

#include <cassert>

int main() {
  assert(!helios::version().empty());
  return 0;
}

