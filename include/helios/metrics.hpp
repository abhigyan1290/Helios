#pragma once

#include "helios/job_state.hpp"

#include <cstdint>
#include <vector>

namespace helios {

using AggregateTime = std::int64_t;

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
AggregateTime total_wait_time(const SimulationResult& result);
double average_wait_time(const SimulationResult& result);
SimTime max_wait_time(const SimulationResult& result);
// Nearest rank: ceil(0.95 * job count), one-based. Empty results return zero.
SimTime p95_wait_time(const SimulationResult& result);

} // namespace helios
