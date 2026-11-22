#include "helios/simulation_runner.hpp"

#include "helios/schedulers/easy_backfill_scheduler.hpp"
#include "helios/schedulers/estimated_sjf_scheduler.hpp"
#include "helios/schedulers/fifo_scheduler.hpp"
#include "helios/schedulers/random_scheduler.hpp"
#include "helios/simulator.hpp"

#include <stdexcept>

namespace helios {

namespace {

template <class Select>
SimulationResult drive(const Workload& workload, Resources resources, Select select) {
  Simulator simulator(workload, resources);
  while (!simulator.done()) {
    simulator.step(select(simulator));
  }
  return simulator.result();
}

} // namespace

SimulationResult run_simulation(const Workload& workload, Resources total_resources,
                                SchedulerFn scheduler) {
  if (!scheduler) {
    throw std::invalid_argument("scheduler function must be provided");
  }
  return drive(workload, total_resources,
               [&](const Simulator& simulator) { return scheduler(simulator.diagnostic_state()); });
}

SimulationResult run_observed_simulation(const Workload& workload, Resources total_resources,
                                         ObservationPolicy policy) {
  if (!policy) {
    throw std::invalid_argument("observation policy must be provided");
  }
  return drive(workload, total_resources,
               [&](const Simulator& simulator) { return policy(simulator.observe()); });
}

SimulationResult run_fifo_simulation(const Workload& workload, Resources total_resources) {
  return run_observed_simulation(workload, total_resources, fifo_policy);
}

SimulationResult run_sjf_simulation(const Workload& workload, Resources total_resources) {
  return drive(workload, total_resources, [](const Simulator& simulator) {
    return estimated_sjf_schedule(simulator.observe_oracle());
  });
}

SimulationResult run_estimated_sjf_simulation(const Workload& workload, Resources total_resources) {
  return run_observed_simulation(workload, total_resources, estimated_sjf_schedule);
}

SimulationResult run_random_simulation(const Workload& workload, Resources total_resources,
                                       unsigned int seed) {
  return run_observed_simulation(workload, total_resources, RandomScheduler(seed));
}

SimulationResult run_easy_backfill_simulation(const Workload& workload, Resources total_resources) {
  return run_observed_simulation(workload, total_resources, easy_backfill_schedule);
}

} // namespace helios
