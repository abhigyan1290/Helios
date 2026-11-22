#include "helios/runtime_estimates.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace helios {

Workload with_runtime_estimates(const Workload& workload, int percentage) {
  validate_workload(workload);
  if (percentage <= 0) {
    throw std::invalid_argument("estimate percentage must be positive");
  }
  auto result = workload;
  for (auto& job : result.jobs) {
    const auto estimate = (static_cast<std::int64_t>(job.duration) * percentage + 99) / 100;
    if (estimate > std::numeric_limits<Duration>::max()) {
      throw std::invalid_argument("estimated duration exceeds simulation time range");
    }
    job.estimated_duration = static_cast<Duration>(estimate);
  }
  return result;
}

} // namespace helios
