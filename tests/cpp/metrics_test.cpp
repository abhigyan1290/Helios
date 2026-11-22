#include "helios/metrics.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <utility>

namespace {

helios::Job make_job(helios::JobId id, helios::SimTime arrival_time, helios::Duration duration) {
  return helios::Job{
      .id = id,
      .arrival_time = arrival_time,
      .duration = duration,
      .request =
          helios::Resources{
              .gpu_count = 1,
              .cpu_count = 4,
              .memory_mb = 8192,
          },
  };
}

helios::CompletedJob completed_job(helios::JobId id, helios::SimTime arrival_time,
                                   helios::SimTime start_time, helios::Duration duration) {
  return helios::CompletedJob{
      .job = make_job(id, arrival_time, duration),
      .start_time = start_time,
      .completion_time = start_time + duration,
  };
}

template <typename Function> void expect_invalid_argument_from(Function function) {
  try {
    function();
  } catch (const std::invalid_argument&) {
    return;
  }

  assert(false && "expected std::invalid_argument");
}

void job_metrics_compute_wait_time() {
  const auto metrics = helios::make_job_metrics(completed_job(1, 5, 9, 4));

  assert(metrics.wait_time == 4);
}

void job_metrics_compute_turnaround_time() {
  const auto metrics = helios::make_job_metrics(completed_job(1, 5, 9, 4));

  assert(metrics.turnaround_time == 8);
}

void job_metrics_copy_core_job_times() {
  const auto metrics = helios::make_job_metrics(completed_job(7, 5, 9, 4));

  assert(metrics.job_id == 7);
  assert(metrics.arrival_time == 5);
  assert(metrics.start_time == 9);
  assert(metrics.completion_time == 13);
  assert(metrics.duration == 4);
}

void invalid_completed_job_is_rejected() {
  auto job = completed_job(1, 5, 9, 4);
  job.completion_time = 12;

  expect_invalid_argument_from([&] { helios::make_job_metrics(job); });
}

void simulation_result_computes_makespan() {
  const auto result = helios::make_simulation_result({
      completed_job(1, 0, 0, 5),
      completed_job(2, 10, 10, 3),
  });

  assert(result.makespan == 13);
}

void empty_simulation_result_has_makespan_zero() {
  const auto result = helios::make_simulation_result({});

  assert(result.jobs.empty());
  assert(result.makespan == 0);
}

void makespan_is_absolute_simulation_end_time() {
  const auto result = helios::make_simulation_result({
      completed_job(1, 100, 100, 10),
  });

  assert(result.makespan == 110);
}

void completed_jobs_are_ordered_by_completion_time_then_id() {
  const auto result = helios::make_simulation_result({
      completed_job(3, 0, 0, 10),
      completed_job(2, 0, 0, 5),
      completed_job(1, 0, 0, 5),
  });

  assert(result.jobs.size() == 3);
  assert(result.jobs[0].job_id == 1);
  assert(result.jobs[1].job_id == 2);
  assert(result.jobs[2].job_id == 3);
}

void total_wait_time_sums_job_wait_times() {
  const auto result = helios::make_simulation_result({
      completed_job(1, 0, 0, 5),
      completed_job(2, 0, 5, 3),
      completed_job(3, 2, 8, 1),
  });

  assert(helios::total_wait_time(result) == 11);
}

void average_wait_time_divides_by_job_count() {
  const auto result = helios::make_simulation_result({
      completed_job(1, 0, 0, 5),
      completed_job(2, 0, 5, 3),
      completed_job(3, 2, 8, 1),
  });

  assert(helios::average_wait_time(result) == 11.0 / 3.0);
}

void empty_average_wait_time_is_zero() {
  const auto result = helios::make_simulation_result({});

  assert(helios::total_wait_time(result) == 0);
  assert(helios::average_wait_time(result) == 0.0);
}

void empty_tail_waits_are_zero() {
  const auto result = helios::make_simulation_result({});
  assert(helios::max_wait_time(result) == 0);
  assert(helios::p95_wait_time(result) == 0);
}

void single_job_tail_waits_equal_its_wait() {
  const auto result = helios::make_simulation_result({completed_job(1, 3, 10, 2)});
  assert(helios::max_wait_time(result) == 7);
  assert(helios::p95_wait_time(result) == 7);
}

void tail_waits_handle_ties_and_zero_waits() {
  const auto tied = helios::make_simulation_result(
      {completed_job(1, 0, 4, 2), completed_job(2, 1, 5, 2), completed_job(3, 0, 0, 2)});
  assert(helios::max_wait_time(tied) == 4);
  assert(helios::p95_wait_time(tied) == 4);
  const auto zero =
      helios::make_simulation_result({completed_job(1, 0, 0, 2), completed_job(2, 3, 3, 2)});
  assert(helios::max_wait_time(zero) == 0);
  assert(helios::p95_wait_time(zero) == 0);
}

void p95_uses_nearest_rank_at_integer_boundaries_without_mutation() {
  struct Example {
    int count;
    helios::SimTime expected;
  };
  for (const auto [count, expected] :
       {Example{19, 19}, Example{20, 19}, Example{21, 20}, Example{100, 95}, Example{101, 96}}) {
    std::vector<helios::CompletedJob> jobs;
    for (int id = 1; id <= count; ++id) {
      jobs.push_back(completed_job(id, 0, id, 1));
    }
    auto result = helios::make_simulation_result(std::move(jobs));
    std::reverse(result.jobs.begin(), result.jobs.end());
    const auto original = result;
    assert(helios::p95_wait_time(result) == expected);
    assert(helios::max_wait_time(result) == count);
    assert(result == original);
  }
}

void p95_can_exclude_an_outlier_and_be_lower_than_average() {
  std::vector<helios::CompletedJob> jobs;
  for (int id = 1; id < 20; ++id) {
    jobs.push_back(completed_job(id, 0, 0, 1));
  }
  jobs.push_back(completed_job(20, 0, 1000, 1));
  const auto result = helios::make_simulation_result(std::move(jobs));
  assert(helios::p95_wait_time(result) == 0);
  assert(helios::average_wait_time(result) == 50.0);
  assert(helios::max_wait_time(result) == 1000);
}

void aggregate_wait_can_exceed_the_timestamp_range() {
  const auto result = helios::make_simulation_result(
      {completed_job(1, 0, 1500000000, 1), completed_job(2, 0, 1500000000, 1)});
  assert(helios::total_wait_time(result) == 3000000000LL);
  assert(helios::average_wait_time(result) == 1500000000.0);
}

} // namespace

int main() {
  job_metrics_compute_wait_time();
  job_metrics_compute_turnaround_time();
  job_metrics_copy_core_job_times();
  invalid_completed_job_is_rejected();
  simulation_result_computes_makespan();
  empty_simulation_result_has_makespan_zero();
  makespan_is_absolute_simulation_end_time();
  completed_jobs_are_ordered_by_completion_time_then_id();
  total_wait_time_sums_job_wait_times();
  average_wait_time_divides_by_job_count();
  empty_average_wait_time_is_zero();
  empty_tail_waits_are_zero();
  single_job_tail_waits_equal_its_wait();
  tail_waits_handle_ties_and_zero_waits();
  p95_uses_nearest_rank_at_integer_boundaries_without_mutation();
  p95_can_exclude_an_outlier_and_be_lower_than_average();
  aggregate_wait_can_exceed_the_timestamp_range();

  return 0;
}
