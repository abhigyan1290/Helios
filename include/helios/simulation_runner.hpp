#pragma once

#include "helios/metrics.hpp"
#include "helios/resources.hpp"
#include "helios/workload.hpp"

namespace helios {

SimulationResult run_fifo_simulation(const Workload& workload, Resources total_resources);

} // namespace helios
