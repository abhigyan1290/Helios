#include "helios/schedulers/sjf_scheduler.hpp"

#include <algorithm>

namespace helios {

namespace {

bool sjf_job_less(const Job& lhs, const Job& rhs) {
  if (lhs.duration != rhs.duration) {
    return lhs.duration < rhs.duration;
  }
  if (lhs.arrival_time != rhs.arrival_time) {
    return lhs.arrival_time < rhs.arrival_time;
  }

  return lhs.id < rhs.id;
}

} // namespace

ScheduleDecision sjf_schedule(const SimulatorState& state) {
  validate_simulator_state(state);

  auto schedulable_jobs = get_schedulable_jobs(state);
  schedulable_jobs.erase(std::remove_if(schedulable_jobs.begin(), schedulable_jobs.end(),
                                        [&state](const Job& job) {
                                          return !fits(state.available_resources, job.request);
                                        }),
                         schedulable_jobs.end());

  if (schedulable_jobs.empty()) {
    return no_schedule_decision();
  }

  const auto best_job =
      std::min_element(schedulable_jobs.begin(), schedulable_jobs.end(), sjf_job_less);
  return select_job(best_job->id);
}

} // namespace helios
