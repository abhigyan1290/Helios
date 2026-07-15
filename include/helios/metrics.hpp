#pragma once

#include "helios/job_state.hpp"

#include <vector>

namespace helios {

struct JobMetrics {
  JobId job_id;
  SimTime arrival_time;
  SimTime start_time;
  SimTime completion_time;
  Duration duration;
  SimTime wait_time;
  SimTime turnaround_time;

  bool operator==(const JobMetrics&) const = default;
};

struct SimulationResult {
  std::vector<JobMetrics> jobs;
  SimTime makespan;

  bool operator==(const SimulationResult&) const = default;
};

JobMetrics make_job_metrics(const CompletedJob& completed_job);
SimulationResult make_simulation_result(std::vector<CompletedJob> completed_jobs);

} // namespace helios
