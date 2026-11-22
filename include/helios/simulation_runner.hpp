#pragma once

#include "helios/metrics.hpp"
#include "helios/observation.hpp"
#include "helios/resources.hpp"
#include "helios/schedule_decision.hpp"
#include "helios/simulator_state.hpp"
#include "helios/workload.hpp"

#include <functional>

namespace helios {

using SchedulerFn = std::function<ScheduleDecision(const SimulatorState&)>;
using ObservationPolicy = std::function<ScheduleDecision(const Observation&)>;

SimulationResult run_simulation(const Workload& workload, Resources total_resources,
                                SchedulerFn scheduler);
SimulationResult run_fifo_simulation(const Workload& workload, Resources total_resources);
SimulationResult run_sjf_simulation(const Workload& workload, Resources total_resources);
SimulationResult run_observed_simulation(const Workload& workload, Resources total_resources,
                                         ObservationPolicy policy);
SimulationResult run_estimated_sjf_simulation(const Workload& workload, Resources total_resources);
SimulationResult run_random_simulation(const Workload& workload, Resources total_resources,
                                       unsigned int seed);
SimulationResult run_easy_backfill_simulation(const Workload& workload, Resources total_resources);

} // namespace helios
