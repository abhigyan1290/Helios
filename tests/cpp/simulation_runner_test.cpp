#include "helios/simulation_runner.hpp"

#include <cassert>
#include <stdexcept>

namespace {

helios::Resources total_resources() {
  return helios::Resources{
      .gpu_count = 4,
      .cpu_count = 32,
      .memory_mb = 131072,
  };
}

helios::Job make_job(helios::JobId id, helios::SimTime arrival_time, helios::Duration duration,
                     helios::Resources request) {
  return helios::Job{
      .id = id,
      .arrival_time = arrival_time,
      .duration = duration,
      .request = request,
  };
}

helios::Job job_with_resources(helios::JobId id, helios::SimTime arrival_time,
                               helios::Duration duration, helios::ResourceAmount gpu_count,
                               helios::ResourceAmount cpu_count, helios::ResourceAmount memory_mb) {
  return make_job(id, arrival_time, duration,
                  helios::Resources{
                      .gpu_count = gpu_count,
                      .cpu_count = cpu_count,
                      .memory_mb = memory_mb,
                  });
}

helios::Job small_job(helios::JobId id, helios::SimTime arrival_time = 0,
                      helios::Duration duration = 5) {
  return job_with_resources(id, arrival_time, duration, 1, 4, 8192);
}

helios::Job two_gpu_job(helios::JobId id, helios::SimTime arrival_time = 0,
                        helios::Duration duration = 10) {
  return job_with_resources(id, arrival_time, duration, 2, 4, 8192);
}

helios::Job full_cluster_job(helios::JobId id, helios::SimTime arrival_time = 0,
                             helios::Duration duration = 10) {
  return job_with_resources(id, arrival_time, duration, 4, 4, 8192);
}

template <typename Function> void expect_runtime_error_from(Function function) {
  try {
    function();
  } catch (const std::runtime_error&) {
    return;
  }

  assert(false && "expected std::runtime_error");
}

void empty_workload_completes_with_empty_result() {
  const auto result = helios::run_fifo_simulation(helios::make_workload({}), total_resources());

  assert(result.jobs.empty());
  assert(result.makespan == 0);
}

void single_job_starts_at_arrival_time() {
  const auto result = helios::run_fifo_simulation(helios::make_workload({
                                                      small_job(1, 3, 5),
                                                  }),
                                                  total_resources());

  assert(result.jobs.size() == 1);
  assert(result.jobs.front().start_time == 3);
}

void single_job_completes_at_arrival_plus_duration() {
  const auto result = helios::run_fifo_simulation(helios::make_workload({
                                                      small_job(1, 3, 5),
                                                  }),
                                                  total_resources());

  assert(result.jobs.front().completion_time == 8);
}

void two_jobs_that_fit_can_run_concurrently() {
  const auto result = helios::run_fifo_simulation(helios::make_workload({
                                                      two_gpu_job(1, 0, 10),
                                                      two_gpu_job(2, 0, 10),
                                                  }),
                                                  total_resources());

  assert(result.jobs.size() == 2);
  assert(result.jobs[0].start_time == 0);
  assert(result.jobs[1].start_time == 0);
  assert(result.jobs[0].completion_time == 10);
  assert(result.jobs[1].completion_time == 10);
}

void strict_fifo_blocks_later_jobs_behind_full_cluster_job() {
  const auto result = helios::run_fifo_simulation(helios::make_workload({
                                                      full_cluster_job(1, 0, 10),
                                                      small_job(2, 0, 5),
                                                  }),
                                                  total_resources());

  assert(result.jobs.size() == 2);
  assert(result.jobs[0].job_id == 1);
  assert(result.jobs[0].start_time == 0);
  assert(result.jobs[0].completion_time == 10);
  assert(result.jobs[1].job_id == 2);
  assert(result.jobs[1].start_time == 10);
}

void blocked_fifo_advances_to_completion_not_irrelevant_arrival() {
  const auto result = helios::run_fifo_simulation(helios::make_workload({
                                                      full_cluster_job(1, 0, 10),
                                                      full_cluster_job(2, 0, 5),
                                                      small_job(3, 5, 1),
                                                  }),
                                                  total_resources());

  assert(result.jobs.size() == 3);
  assert(result.jobs[0].job_id == 1);
  assert(result.jobs[0].completion_time == 10);
  assert(result.jobs[1].job_id == 2);
  assert(result.jobs[1].start_time == 10);
  assert(result.jobs[2].job_id == 3);
  assert(result.jobs[2].start_time == 15);
}

void unschedulable_gpu_request_is_rejected() {
  expect_runtime_error_from([] {
    helios::run_fifo_simulation(helios::make_workload({
                                    job_with_resources(1, 0, 5, 5, 4, 8192),
                                }),
                                total_resources());
  });
}

void unschedulable_cpu_request_is_rejected() {
  expect_runtime_error_from([] {
    helios::run_fifo_simulation(helios::make_workload({
                                    job_with_resources(1, 0, 5, 1, 33, 8192),
                                }),
                                total_resources());
  });
}

void unschedulable_memory_request_is_rejected() {
  expect_runtime_error_from([] {
    helios::run_fifo_simulation(helios::make_workload({
                                    job_with_resources(1, 0, 5, 1, 4, 131073),
                                }),
                                total_resources());
  });
}

void later_jobs_are_not_started_before_arrival() {
  const auto result = helios::run_fifo_simulation(helios::make_workload({
                                                      small_job(1, 10, 5),
                                                  }),
                                                  total_resources());

  assert(result.jobs.front().start_time == 10);
}

void same_time_arrivals_start_in_job_id_order() {
  const auto result = helios::run_fifo_simulation(helios::make_workload({
                                                      small_job(2, 0, 5),
                                                      small_job(1, 0, 5),
                                                  }),
                                                  total_resources());

  assert(result.jobs.size() == 2);
  assert(result.jobs[0].job_id == 1);
  assert(result.jobs[1].job_id == 2);
  assert(result.jobs[0].start_time == 0);
  assert(result.jobs[1].start_time == 0);
}

void runner_is_deterministic() {
  const auto workload = helios::make_workload({
      small_job(2, 3, 4),
      full_cluster_job(1, 0, 5),
  });

  assert(helios::run_fifo_simulation(workload, total_resources()) ==
         helios::run_fifo_simulation(workload, total_resources()));
}

void metrics_order_by_completion_time_then_job_id() {
  const auto result = helios::run_fifo_simulation(helios::make_workload({
                                                      small_job(2, 0, 5),
                                                      small_job(1, 0, 5),
                                                  }),
                                                  total_resources());

  assert(result.jobs[0].job_id == 1);
  assert(result.jobs[1].job_id == 2);
}

} // namespace

int main() {
  empty_workload_completes_with_empty_result();
  single_job_starts_at_arrival_time();
  single_job_completes_at_arrival_plus_duration();
  two_jobs_that_fit_can_run_concurrently();
  strict_fifo_blocks_later_jobs_behind_full_cluster_job();
  blocked_fifo_advances_to_completion_not_irrelevant_arrival();
  unschedulable_gpu_request_is_rejected();
  unschedulable_cpu_request_is_rejected();
  unschedulable_memory_request_is_rejected();
  later_jobs_are_not_started_before_arrival();
  same_time_arrivals_start_in_job_id_order();
  runner_is_deterministic();
  metrics_order_by_completion_time_then_job_id();

  return 0;
}
