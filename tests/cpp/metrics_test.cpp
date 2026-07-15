#include "helios/metrics.hpp"

#include <cassert>
#include <stdexcept>

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

  return 0;
}
