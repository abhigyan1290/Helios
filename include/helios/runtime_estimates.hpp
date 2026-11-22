#pragma once

#include "helios/workload.hpp"

namespace helios {

// Synthetic estimate model: ceil(actual duration * percentage / 100).
Workload with_runtime_estimates(const Workload& workload, int percentage);

} // namespace helios
