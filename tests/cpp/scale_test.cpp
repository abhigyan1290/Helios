#include "helios/demo_workloads.hpp"
#include "helios/runtime_estimates.hpp"
#include "helios/simulation_runner.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

void verify_result(const helios::Workload& workload, const helios::SimulationResult& result,
                   helios::Resources capacity) {
  assert(result.jobs.size() == workload.jobs.size());
  std::unordered_map<helios::JobId, const helios::Job*> input;
  for (const auto& job : workload.jobs) {
    assert(input.emplace(job.id, &job).second);
  }
  struct ResourceChange {
    helios::SimTime time;
    int direction;
    helios::Resources resources;
  };
  std::vector<ResourceChange> events;
  std::unordered_set<helios::JobId> seen;
  helios::SimTime last_completion = 0;
  for (const auto& metrics : result.jobs) {
    assert(seen.insert(metrics.job_id).second);
    assert(input.contains(metrics.job_id));
    const auto& job = *input.at(metrics.job_id);
    assert(metrics.arrival_time == job.arrival_time);
    assert(metrics.duration == job.duration);
    assert(metrics.start_time >= job.arrival_time);
    assert(metrics.completion_time == metrics.start_time + job.duration);
    assert(metrics.wait_time == metrics.start_time - job.arrival_time);
    assert(metrics.turnaround_time == metrics.completion_time - job.arrival_time);
    events.push_back({metrics.start_time, 1, job.request});
    events.push_back({metrics.completion_time, -1, job.request});
    last_completion = std::max(last_completion, metrics.completion_time);
  }
  assert(result.makespan == last_completion);
  // Reconstruct resource usage independently, releasing before starts at equal times.
  std::sort(events.begin(), events.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.time != rhs.time ? lhs.time < rhs.time : lhs.direction < rhs.direction;
  });
  helios::Resources used{0, 0, 0};
  for (const auto& event : events) {
    used.gpu_count += event.direction * event.resources.gpu_count;
    used.cpu_count += event.direction * event.resources.cpu_count;
    used.memory_mb += event.direction * event.resources.memory_mb;
    assert(used.gpu_count >= 0 && used.gpu_count <= capacity.gpu_count);
    assert(used.cpu_count >= 0 && used.cpu_count <= capacity.cpu_count);
    assert(used.memory_mb >= 0 && used.memory_mb <= capacity.memory_mb);
  }
  assert((used == helios::Resources{0, 0, 0}));
}

} // namespace

int main() {
  for (const auto& spec : helios::scale_workload_specs()) {
    const auto workload = helios::make_generated_workload(spec.name, 0);
    const auto original = workload;
    assert(workload == helios::make_generated_workload(spec.name, 0));
    const auto resources = helios::demo_cluster_resources();
    verify_result(workload, helios::run_fifo_simulation(workload, resources), resources);
    verify_result(workload, helios::run_sjf_simulation(workload, resources), resources);
    const auto estimated = helios::with_runtime_estimates(workload, 100);
    verify_result(estimated, helios::run_estimated_sjf_simulation(estimated, resources), resources);
    verify_result(estimated, helios::run_easy_backfill_simulation(estimated, resources), resources);
    verify_result(workload, helios::run_random_simulation(workload, resources, 42), resources);
    assert(workload == original);
    std::cout << spec.name << ": verified " << workload.jobs.size()
              << " jobs per scheduler, lifecycle and resource limits\n";
  }
}
