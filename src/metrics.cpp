#include "helios/metrics.hpp"

#include <algorithm>

namespace helios {

namespace {

bool completed_job_less(const CompletedJob& lhs, const CompletedJob& rhs) {
  if (lhs.completion_time != rhs.completion_time) {
    return lhs.completion_time < rhs.completion_time;
  }

  return lhs.job.id < rhs.job.id;
}

} // namespace

JobMetrics make_job_metrics(const CompletedJob& completed_job) {
  validate_completed_job(completed_job);

  return JobMetrics{
      .job_id = completed_job.job.id,
      .arrival_time = completed_job.job.arrival_time,
      .start_time = completed_job.start_time,
      .completion_time = completed_job.completion_time,
      .duration = completed_job.job.duration,
      .wait_time = completed_job.start_time - completed_job.job.arrival_time,
      .turnaround_time = completed_job.completion_time - completed_job.job.arrival_time,
  };
}

SimulationResult make_simulation_result(std::vector<CompletedJob> completed_jobs) {
  std::sort(completed_jobs.begin(), completed_jobs.end(), completed_job_less);

  std::vector<JobMetrics> job_metrics;
  job_metrics.reserve(completed_jobs.size());

  SimTime makespan = 0;
  for (const auto& completed_job : completed_jobs) {
    job_metrics.push_back(make_job_metrics(completed_job));
    makespan = std::max(makespan, completed_job.completion_time);
  }

  return SimulationResult{
      .jobs = std::move(job_metrics),
      .makespan = makespan,
  };
}

} // namespace helios
