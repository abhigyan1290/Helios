#include "helios/benchmark_suite.hpp"

#include "helios/demo_workloads.hpp"
#include "helios/simulation_runner.hpp"

#include <cassert>
#include <stdexcept>
#include <utility>

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
  assert(summary.p95_wait_time == 3);
  assert(summary.max_wait_time == 3);
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
    assert(row.p95_wait_time == 0);
    assert(row.max_wait_time == 0);
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

void seeded_comparisons_match_direct_runs() {
  const std::vector<unsigned int> seeds{0, 42};
  const auto rows = helios::compare_seeded_workloads({"generated-small"}, seeds);
  assert(rows.size() == 4);
  assert(rows == helios::compare_seeded_workloads({"generated-small"}, seeds));
  for (std::size_t i = 0; i < seeds.size(); ++i) {
    const auto workload = helios::make_generated_workload("generated-small", seeds[i]);
    const auto direct =
        helios::compare_fifo_and_sjf("generated-small", workload, helios::demo_cluster_resources());
    assert(rows[2 * i].seed == seeds[i]);
    assert(rows[2 * i + 1].seed == seeds[i]);
    assert(rows[2 * i].summary == direct[0]);
    assert(rows[2 * i + 1].summary == direct[1]);
  }
  assert(helios::make_generated_workload("generated-small", 0) !=
         helios::make_generated_workload("generated-small", 42));
}

void invalid_seeded_requests_are_rejected() {
  const auto rejected = [](const std::vector<std::string>& profiles,
                           const std::vector<unsigned int>& seeds) {
    try {
      (void)helios::compare_seeded_workloads(profiles, seeds);
    } catch (const std::invalid_argument&) {
      return;
    }
    assert(false && "expected invalid benchmark request to be rejected");
  };
  rejected({}, {0});
  rejected({"generated-small"}, {});
  rejected({"generated-small"}, {0, 0});
  rejected({"generated-small", "generated-small"}, {0});
  rejected({"unknown"}, {0});
  rejected({"fifo-blocking"}, {0});
}

void lower_average_and_p95_can_hide_a_longer_maximum_wait() {
  const auto capacity = helios::demo_cluster_resources();
  const auto job = [capacity](helios::JobId id, helios::SimTime arrival,
                              helios::Duration duration) {
    return helios::Job{
        .id = id, .arrival_time = arrival, .duration = duration, .request = capacity};
  };
  std::vector<helios::Job> jobs{job(1, 0, 10), job(2, 1, 2)};
  for (int arrival = 2; arrival <= 100; ++arrival) {
    jobs.push_back(job(arrival + 1, arrival, 1));
  }
  const auto rows = helios::compare_fifo_and_sjf("long-wait-example",
                                                 helios::make_workload(std::move(jobs)), capacity);
  const auto& fifo = rows[0];
  const auto& sjf = rows[1];
  assert(fifo.job_count == 101 && sjf.job_count == 101);
  assert(fifo.total_wait_time == 999 && sjf.total_wait_time == 900);
  assert(sjf.average_wait_time < fifo.average_wait_time);
  assert(fifo.p95_wait_time == 10 && sjf.p95_wait_time == 8);
  assert(fifo.max_wait_time == 10 && sjf.max_wait_time == 108);
  assert(fifo.makespan == 111 && sjf.makespan == 111);
}

} // namespace

int main() {
  summary_preserves_names_and_metrics();
  empty_comparison_returns_zero_metrics();
  comparison_matches_direct_runs_and_preserves_workload();
  generated_suite_covers_all_profiles_and_is_repeatable();
  seeded_comparisons_match_direct_runs();
  invalid_seeded_requests_are_rejected();
  lower_average_and_p95_can_hide_a_longer_maximum_wait();
}
