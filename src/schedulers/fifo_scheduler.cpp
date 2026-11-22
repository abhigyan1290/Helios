#include "helios/schedulers/fifo_scheduler.hpp"

#include <algorithm>
#include <tuple>

namespace helios {

ScheduleDecision fifo_schedule(const SimulatorState& state) {
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

ScheduleDecision fifo_policy(const Observation& observation) {
  const auto head = std::min_element(
      observation.pending_jobs.begin(), observation.pending_jobs.end(),
      [](const PendingJobView& lhs, const PendingJobView& rhs) {
        return std::tie(lhs.arrival_time, lhs.id) < std::tie(rhs.arrival_time, rhs.id);
      });
  if (head == observation.pending_jobs.end() ||
      !fits(observation.available_resources, head->request)) {
    return no_schedule_decision();
  }
  return select_job(head->id);
}

} // namespace helios
