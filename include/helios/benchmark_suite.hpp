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
  SimTime total_wait_time;
  double average_wait_time;

  bool operator==(const SchedulerSummary&) const = default;
};

SchedulerSummary summarize_result(std::string scheduler_name, std::string workload_name,
                                  const SimulationResult& result);
std::vector<SchedulerSummary> compare_fifo_and_sjf(std::string_view workload_name,
                                                   const Workload& workload,
                                                   Resources cluster_resources);
std::vector<SchedulerSummary> compare_generated_suite();

} // namespace helios
