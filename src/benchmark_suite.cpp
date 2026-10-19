#include "helios/benchmark_suite.hpp"

#include "helios/demo_workloads.hpp"
#include "helios/simulation_runner.hpp"

#include <utility>

namespace helios {

SchedulerSummary summarize_result(std::string scheduler_name, std::string workload_name,
                                  const SimulationResult& result) {
  return SchedulerSummary{
      .scheduler_name = std::move(scheduler_name),
      .workload_name = std::move(workload_name),
      .job_count = result.jobs.size(),
      .makespan = result.makespan,
      .total_wait_time = total_wait_time(result),
      .average_wait_time = average_wait_time(result),
  };
}

std::vector<SchedulerSummary> compare_fifo_and_sjf(std::string_view workload_name,
                                                   const Workload& workload,
                                                   Resources cluster_resources) {
  return {
      summarize_result("fifo", std::string(workload_name),
                       run_fifo_simulation(workload, cluster_resources)),
      summarize_result("sjf", std::string(workload_name),
                       run_sjf_simulation(workload, cluster_resources)),
  };
}

std::vector<SchedulerSummary> compare_generated_suite() {
  std::vector<SchedulerSummary> summaries;
  for (const auto& spec : demo_workload_specs()) {
    if (!spec.name.starts_with("generated-")) {
      continue;
    }
    const auto workload = make_demo_workload(spec.name);
    for (auto& summary : compare_fifo_and_sjf(spec.name, workload, demo_cluster_resources())) {
      summaries.push_back(std::move(summary));
    }
  }
  return summaries;
}

} // namespace helios
