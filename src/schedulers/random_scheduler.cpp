#include "helios/schedulers/random_scheduler.hpp"

#include <vector>

namespace helios {

RandomScheduler::RandomScheduler(unsigned int seed) : engine_(seed) {}

void RandomScheduler::reset(unsigned int seed) {
  engine_.seed(seed);
}

ScheduleDecision RandomScheduler::operator()(const Observation& observation) {
  std::vector<JobId> fitting;
  for (const auto& job : observation.pending_jobs) {
    if (fits(observation.available_resources, job.request)) {
      fitting.push_back(job.id);
    }
  }
  if (fitting.empty()) {
    return no_schedule_decision();
  }
  std::uniform_int_distribution<std::size_t> choose(0, fitting.size() - 1);
  return select_job(fitting[choose(engine_)]);
}

} // namespace helios
