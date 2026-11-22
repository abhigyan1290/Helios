#pragma once

#include "helios/job.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace helios {

struct PendingJobView {
  JobId id;
  SimTime arrival_time;
  std::optional<Duration> estimated_duration;
  Resources request;

  bool operator==(const PendingJobView&) const = default;
};

struct RunningJobView {
  PendingJobView job;
  SimTime start_time;
  Duration elapsed_time;

  bool operator==(const RunningJobView&) const = default;
};

struct Observation {
  SimTime current_time;
  Resources total_resources;
  Resources available_resources;
  std::vector<PendingJobView> pending_jobs;
  std::vector<RunningJobView> running_jobs;
  std::size_t completed_job_count;
  bool terminated;

  bool operator==(const Observation&) const = default;
};

} // namespace helios
