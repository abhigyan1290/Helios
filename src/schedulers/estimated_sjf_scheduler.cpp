#include "helios/schedulers/estimated_sjf_scheduler.hpp"

#include "policy_helpers.hpp"

#include <optional>
#include <tuple>

namespace helios {

ScheduleDecision estimated_sjf_schedule(const Observation& observation) {
  std::optional<PendingJobView> best;
  for (const auto& job : observation.pending_jobs) {
    const auto estimate = detail::require_estimate(job);
    if (!fits(observation.available_resources, job.request)) {
      continue;
    }
    if (!best || std::tie(estimate, job.arrival_time, job.id) <
                     std::tie(*best->estimated_duration, best->arrival_time, best->id)) {
      best = job;
    }
  }
  return best ? select_job(best->id) : no_schedule_decision();
}

} // namespace helios
