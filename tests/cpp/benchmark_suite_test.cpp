#include "helios/benchmark_suite.hpp"

#include "helios/demo_workloads.hpp"
#include "helios/simulation_runner.hpp"

#include <cassert>

namespace {

void summary_preserves_names_and_metrics() {
  const helios::SimulationResult result{
      .jobs = {{1, 0, 2, 5, 3, 2, 5}, {2, 1, 4, 6, 2, 3, 5}},
      .makespan = 6,
  };
  const auto summary = helios::summarize_result("fifo", "example", result);
  assert(summary.scheduler_name == "fifo");
  assert(summary.workload_name == "example");
  assert(summary.job_count == 2);
  assert(summary.makespan == 6);
  assert(summary.total_wait_time == 5);
  assert(summary.average_wait_time == 2.5);
}

void empty_comparison_returns_zero_metrics() {
  const auto rows = helios::compare_fifo_and_sjf("empty", helios::make_workload({}),
                                                 helios::demo_cluster_resources());
  assert(rows.size() == 2);
  assert(rows[0].scheduler_name == "fifo");
  assert(rows[1].scheduler_name == "sjf");
  for (const auto& row : rows) {
    assert(row.workload_name == "empty");
    assert(row.job_count == 0);
    assert(row.makespan == 0);
    assert(row.total_wait_time == 0);
    assert(row.average_wait_time == 0.0);
  }
}

void comparison_matches_direct_runs_and_preserves_workload() {
  const auto workload = helios::make_demo_workload("fifo-blocking");
  const auto original = workload;
  const auto resources = helios::demo_cluster_resources();
  const auto rows = helios::compare_fifo_and_sjf("fifo-blocking", workload, resources);
  assert(rows.size() == 2);
  assert(rows[0] == helios::summarize_result("fifo", "fifo-blocking",
                                             helios::run_fifo_simulation(workload, resources)));
  assert(rows[1] == helios::summarize_result("sjf", "fifo-blocking",
                                             helios::run_sjf_simulation(workload, resources)));
  assert(workload == original);
}

void generated_suite_covers_all_profiles_and_is_repeatable() {
  const auto rows = helios::compare_generated_suite();
  assert(rows == helios::compare_generated_suite());
  std::size_t index = 0;
  for (const auto& spec : helios::demo_workload_specs()) {
    if (!spec.name.starts_with("generated-")) {
      continue;
    }
    const auto workload = helios::make_demo_workload(spec.name);
    assert(index + 1 < rows.size());
    assert(rows[index].scheduler_name == "fifo");
    assert(rows[index + 1].scheduler_name == "sjf");
    for (int scheduler = 0; scheduler < 2; ++scheduler) {
      const auto& row = rows[index++];
      assert(row.workload_name == spec.name);
      assert(row.job_count == workload.jobs.size());
      assert(row.makespan > 0);
      assert(row.total_wait_time >= 0);
    }
  }
  assert(index == rows.size());
  assert(rows.size() == 10);
}

} // namespace

int main() {
  summary_preserves_names_and_metrics();
  empty_comparison_returns_zero_metrics();
  comparison_matches_direct_runs_and_preserves_workload();
  generated_suite_covers_all_profiles_and_is_repeatable();
}
