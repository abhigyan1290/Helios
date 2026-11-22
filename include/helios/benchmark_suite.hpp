#pragma once

#include "helios/metrics.hpp"
#include "helios/workload.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace helios {

struct SchedulerSummary {
  std::string scheduler_name;
  std::string workload_name;
  std::size_t job_count;
  SimTime makespan;
  AggregateTime total_wait_time;
  double average_wait_time;
  SimTime p95_wait_time;
  SimTime max_wait_time;

  bool operator==(const SchedulerSummary&) const = default;
};

SchedulerSummary summarize_result(std::string scheduler_name, std::string workload_name,
                                  const SimulationResult& result);
std::vector<SchedulerSummary> compare_fifo_and_sjf(std::string_view workload_name,
                                                   const Workload& workload,
                                                   Resources cluster_resources);
std::vector<SchedulerSummary> compare_generated_suite();
std::vector<SchedulerSummary> compare_baselines(std::string_view workload_name,
                                                const Workload& workload,
                                                Resources cluster_resources,
                                                unsigned int random_seed);

struct SeededSummary {
  unsigned int seed;
  SchedulerSummary summary;

  bool operator==(const SeededSummary&) const = default;
};

std::vector<SeededSummary> compare_seeded_workloads(const std::vector<std::string>& profile_names,
                                                    const std::vector<unsigned int>& seeds);

} // namespace helios
