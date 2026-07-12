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

bool less_equal(const Resources& lhs, const Resources& rhs) {
  validate_resources(lhs);
  validate_resources(rhs);

  return lhs.gpu_count <= rhs.gpu_count && lhs.cpu_count <= rhs.cpu_count &&
         lhs.memory_mb <= rhs.memory_mb;
}

bool fits(const Resources& available, const Resources& requested) {
  return less_equal(requested, available);
}

Resources allocate(const Resources& available, const Resources& requested) {
  validate_resources(available);
  validate_resources(requested);

  if (!fits(available, requested)) {
    throw std::runtime_error("requested resources do not fit available resources");
  }

  return Resources{
      .gpu_count = available.gpu_count - requested.gpu_count,
      .cpu_count = available.cpu_count - requested.cpu_count,
      .memory_mb = available.memory_mb - requested.memory_mb,
  };
}

Resources release(const Resources& available, const Resources& total, const Resources& released) {
  validate_resources(available);
  validate_resources(total);
  validate_resources(released);

  if (!less_equal(available, total)) {
    throw std::invalid_argument("available resources cannot exceed total resources");
  }

  const Resources after_release{
      .gpu_count = available.gpu_count + released.gpu_count,
      .cpu_count = available.cpu_count + released.cpu_count,
      .memory_mb = available.memory_mb + released.memory_mb,
  };

  if (!less_equal(after_release, total)) {
    throw std::runtime_error("released resources would exceed total resources");
  }

  return after_release;
}

} // namespace helios
