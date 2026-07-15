#include "helios/workload.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace helios {

namespace {

bool job_less(const Job& lhs, const Job& rhs) {
  if (lhs.arrival_time != rhs.arrival_time) {
    return lhs.arrival_time < rhs.arrival_time;
  }

  return lhs.id < rhs.id;
}

} // namespace

void validate_workload(const Workload& workload) {
  std::unordered_set<JobId> ids;

  for (const auto& job : workload.jobs) {
    validate_job(job);
    if (!ids.insert(job.id).second) {
      throw std::invalid_argument("workload job ids must be unique");
    }
  }
}

Workload make_workload(std::vector<Job> jobs) {
  Workload workload{
      .jobs = std::move(jobs),
  };

  validate_workload(workload);
  std::sort(workload.jobs.begin(), workload.jobs.end(), job_less);
  return workload;
}

} // namespace helios
