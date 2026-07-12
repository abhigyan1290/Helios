#include "helios/resources.hpp"

#include <stdexcept>

namespace helios {

void validate_resources(const Resources& resources) {
  if (resources.gpu_count < 0) {
    throw std::invalid_argument("resource GPU count must be non-negative");
  }
  if (resources.cpu_count < 0) {
    throw std::invalid_argument("resource CPU count must be non-negative");
  }
  if (resources.memory_mb < 0) {
    throw std::invalid_argument("resource memory must be non-negative");
  }
}

}  // namespace helios
