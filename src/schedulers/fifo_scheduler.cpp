#include "helios/schedulers/fifo_scheduler.hpp"

namespace helios {

ScheduleDecision fifo_schedule(const SimulatorState& state) {
  validate_simulator_state(state);

  const auto schedulable_jobs = get_schedulable_jobs(state);
  if (schedulable_jobs.empty()) {
    return no_schedule_decision();
  }

  const auto& head_job = schedulable_jobs.front();
  if (!fits(state.available_resources, head_job.request)) {
    return no_schedule_decision();
  }

  return select_job(head_job.id);
}

} // namespace helios
