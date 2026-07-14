#include "helios/schedule_decision.hpp"

#include <stdexcept>

namespace helios {

ScheduleDecision no_schedule_decision() {
  return ScheduleDecision{
      .job_id = std::nullopt,
  };
}

ScheduleDecision select_job(JobId job_id) {
  if (job_id < 0) {
    throw std::invalid_argument("selected job id must be non-negative");
  }

  return ScheduleDecision{
      .job_id = job_id,
  };
}

SimulatorState apply_schedule_decision(const SimulatorState& state,
                                       const ScheduleDecision& decision) {
  if (!decision.job_id.has_value()) {
    return state;
  }

  return start_pending_job(state, *decision.job_id);
}

} // namespace helios
