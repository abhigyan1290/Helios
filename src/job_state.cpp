#include "helios/job_state.hpp"

#include <stdexcept>

namespace helios {

void validate_running_job(const RunningJob& running_job) {
  validate_job(running_job.job);

  if (running_job.start_time < running_job.job.arrival_time) {
    throw std::invalid_argument("running job start time cannot be before arrival time");
  }
  if (running_job.completion_time != running_job.start_time + running_job.job.duration) {
    throw std::invalid_argument("running job completion time must equal start time plus duration");
  }
}

void validate_completed_job(const CompletedJob& completed_job) {
  validate_job(completed_job.job);

  if (completed_job.start_time < completed_job.job.arrival_time) {
    throw std::invalid_argument("completed job start time cannot be before arrival time");
  }
  if (completed_job.completion_time != completed_job.start_time + completed_job.job.duration) {
    throw std::invalid_argument(
        "completed job completion time must equal start time plus duration");
  }
}

RunningJob start_runtime_job(const Job& job, SimTime start_time) {
  validate_job(job);

  if (start_time < job.arrival_time) {
    throw std::runtime_error("job cannot start before arrival time");
  }

  return RunningJob{
      .job = job,
      .start_time = start_time,
      .completion_time = start_time + job.duration,
  };
}

CompletedJob complete_runtime_job(const RunningJob& running_job) {
  validate_running_job(running_job);

  return CompletedJob{
      .job = running_job.job,
      .start_time = running_job.start_time,
      .completion_time = running_job.completion_time,
  };
}

} // namespace helios
