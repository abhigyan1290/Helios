#pragma once

#include "helios/job.hpp"

namespace helios {

struct RunningJob {
  Job job;
  SimTime start_time;
  SimTime completion_time;

  bool operator==(const RunningJob&) const = default;
};

struct CompletedJob {
  Job job;
  SimTime start_time;
  SimTime completion_time;

  bool operator==(const CompletedJob&) const = default;
};

void validate_running_job(const RunningJob& running_job);
void validate_completed_job(const CompletedJob& completed_job);
RunningJob start_runtime_job(const Job& job, SimTime start_time);
CompletedJob complete_runtime_job(const RunningJob& running_job);

} // namespace helios
