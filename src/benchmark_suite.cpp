#include "helios/benchmark_suite.hpp"

#include "helios/demo_workloads.hpp"
#include "helios/simulation_runner.hpp"

#include <stdexcept>
#include <unordered_set>
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
      .p95_wait_time = p95_wait_time(result),
      .max_wait_time = max_wait_time(result),
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

std::vector<SchedulerSummary> compare_baselines(std::string_view workload_name,
                                                const Workload& workload,
                                                Resources cluster_resources,
                                                unsigned int random_seed) {
  const std::string name(workload_name);
  return {
      summarize_result("fifo", name, run_fifo_simulation(workload, cluster_resources)),
      summarize_result("oracle_sjf", name, run_sjf_simulation(workload, cluster_resources)),
      summarize_result("estimated_sjf", name,
                       run_estimated_sjf_simulation(workload, cluster_resources)),
      summarize_result("random", name,
                       run_random_simulation(workload, cluster_resources, random_seed)),
      summarize_result("easy_backfill", name,
                       run_easy_backfill_simulation(workload, cluster_resources)),
  };
}

std::vector<SeededSummary> compare_seeded_workloads(const std::vector<std::string>& profile_names,
                                                    const std::vector<unsigned int>& seeds) {
  if (profile_names.empty() || seeds.empty()) {
    throw std::invalid_argument("benchmark requires profiles and seeds");
  }
  std::unordered_set<unsigned int> unique_seeds;
  for (const auto seed : seeds) {
    if (!unique_seeds.insert(seed).second) {
      throw std::invalid_argument("benchmark seeds must be unique");
    }
  }
  std::unordered_set<std::string> unique_profiles;
  for (const auto& name : profile_names) {
    if (!unique_profiles.insert(name).second) {
      throw std::invalid_argument("benchmark profiles must be unique");
    }
    (void)generated_workload_config(name, seeds.front());
  }

  std::vector<SeededSummary> rows;
  for (const auto& name : profile_names) {
    for (const auto seed : seeds) {
      const auto workload = make_generated_workload(name, seed);
      for (auto& summary : compare_fifo_and_sjf(name, workload, demo_cluster_resources())) {
        rows.push_back(SeededSummary{seed, std::move(summary)});
      }
    }
  }
  return rows;
}

} // namespace helios
