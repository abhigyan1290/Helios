#include "helios/schedulers/sjf_scheduler.hpp"

#include "helios/demo_workloads.hpp"
#include "helios/simulation_runner.hpp"

#include <cassert>
#include <stdexcept>

namespace {

helios::Resources total_resources() {
  return helios::Resources{
      .gpu_count = 2,
      .cpu_count = 16,
      .memory_mb = 65536,
  };
}

helios::Job job_with_resources(helios::JobId id, helios::SimTime arrival_time,
                               helios::Duration duration, helios::ResourceAmount gpu_count) {
  return helios::Job{
      .id = id,
      .arrival_time = arrival_time,
      .duration = duration,
      .request =
          helios::Resources{
              .gpu_count = gpu_count,
              .cpu_count = 4,
              .memory_mb = 8192,
          },
  };
}

helios::Job one_gpu_job(helios::JobId id, helios::SimTime arrival_time, helios::Duration duration) {
  return job_with_resources(id, arrival_time, duration, 1);
}

helios::Job two_gpu_job(helios::JobId id, helios::SimTime arrival_time, helios::Duration duration) {
  return job_with_resources(id, arrival_time, duration, 2);
}

bool has_no_job(const helios::ScheduleDecision& decision) {
  return !decision.job_id.has_value();
}

helios::JobId selected_job_id(const helios::ScheduleDecision& decision) {
  assert(decision.job_id.has_value());
  return *decision.job_id;
}

helios::SimulatorState state_with_jobs(std::initializer_list<helios::Job> jobs,
                                       helios::SimTime current_time = 0) {
  auto state = helios::create_simulator_state(total_resources());
  for (const auto& job : jobs) {
    state = helios::add_pending_job(state, job);
  }

  return helios::advance_time(state, current_time);
}

template <typename Function> void expect_invalid_argument_from(Function function) {
  try {
    function();
  } catch (const std::invalid_argument&) {
    return;
  }

  assert(false && "expected std::invalid_argument");
}

helios::SimTime wait_time_for(const helios::SimulationResult& result, helios::JobId job_id) {
  for (const auto& metrics : result.jobs) {
    if (metrics.job_id == job_id) {
      return metrics.wait_time;
    }
  }

  assert(false && "expected job metrics");
  return 0;
}

void empty_simulator_returns_no_decision() {
  assert(has_no_job(helios::sjf_schedule(helios::create_simulator_state(total_resources()))));
}

void future_jobs_return_no_decision() {
  const auto state = state_with_jobs({
      one_gpu_job(1, 5, 1),
  });

  assert(has_no_job(helios::sjf_schedule(state)));
}

void single_arrived_fitting_job_is_selected() {
  const auto state = state_with_jobs({
      one_gpu_job(1, 0, 5),
  });

  assert(selected_job_id(helios::sjf_schedule(state)) == 1);
}

void single_arrived_non_fitting_job_returns_no_decision() {
  auto state = state_with_jobs({
      two_gpu_job(1, 0, 5),
      one_gpu_job(2, 0, 5),
  });
  state = helios::start_pending_job(state, 2);

  assert(has_no_job(helios::sjf_schedule(state)));
}

void shorter_duration_wins_among_schedulable_jobs() {
  const auto state = state_with_jobs({
      one_gpu_job(1, 0, 10),
      one_gpu_job(2, 0, 3),
  });

  assert(selected_job_id(helios::sjf_schedule(state)) == 2);
}

void future_short_job_does_not_win_before_arrival() {
  const auto state = state_with_jobs({
      one_gpu_job(1, 0, 10),
      one_gpu_job(2, 5, 1),
  });

  assert(selected_job_id(helios::sjf_schedule(state)) == 1);
}

void equal_duration_tie_breaks_by_arrival_time() {
  const auto state = state_with_jobs(
      {
          one_gpu_job(1, 3, 5),
          one_gpu_job(2, 1, 5),
      },
      3);

  assert(selected_job_id(helios::sjf_schedule(state)) == 2);
}

void equal_duration_and_arrival_tie_breaks_by_job_id() {
  const auto state = state_with_jobs({
      one_gpu_job(2, 0, 5),
      one_gpu_job(1, 0, 5),
  });

  assert(selected_job_id(helios::sjf_schedule(state)) == 1);
}

void skips_non_fitting_jobs_and_selects_shortest_fitting_job() {
  auto state = state_with_jobs({
      one_gpu_job(1, 0, 20),
      two_gpu_job(2, 0, 1),
      one_gpu_job(3, 0, 5),
  });
  state = helios::start_pending_job(state, 1);

  assert(selected_job_id(helios::sjf_schedule(state)) == 3);
}

void schedulable_jobs_with_none_fitting_return_no_decision() {
  auto state = state_with_jobs({
      two_gpu_job(1, 0, 5),
      two_gpu_job(2, 0, 5),
  });
  state = helios::start_pending_job(state, 1);

  assert(has_no_job(helios::sjf_schedule(state)));
}

void scheduler_does_not_mutate_state() {
  const auto state = state_with_jobs({
      one_gpu_job(1, 0, 10),
      one_gpu_job(2, 0, 3),
  });

  const auto original = state;
  assert(selected_job_id(helios::sjf_schedule(state)) == 2);
  assert(state == original);
}

void invalid_simulator_state_is_rejected() {
  auto state = helios::create_simulator_state(total_resources());
  state.current_time = -1;

  expect_invalid_argument_from([&] { helios::sjf_schedule(state); });
}

void fifo_and_sjf_match_when_jobs_arrive_one_at_a_time() {
  const auto workload = helios::make_workload({
      one_gpu_job(1, 0, 2),
      one_gpu_job(2, 3, 1),
      one_gpu_job(3, 5, 4),
  });

  assert(helios::run_fifo_simulation(workload, total_resources()) ==
         helios::run_sjf_simulation(workload, total_resources()));
}

void fifo_and_sjf_differ_on_compare_workload() {
  const auto workload = helios::make_workload({
      two_gpu_job(1, 0, 10),
      one_gpu_job(2, 0, 1),
      one_gpu_job(3, 0, 1),
  });

  const auto fifo = helios::run_fifo_simulation(workload, total_resources());
  const auto sjf = helios::run_sjf_simulation(workload, total_resources());

  assert(wait_time_for(fifo, 1) == 0);
  assert(wait_time_for(fifo, 2) == 10);
  assert(wait_time_for(fifo, 3) == 10);

  assert(wait_time_for(sjf, 1) == 1);
  assert(wait_time_for(sjf, 2) == 0);
  assert(wait_time_for(sjf, 3) == 0);
}

void sjf_runner_is_deterministic() {
  const auto workload = helios::make_workload({
      one_gpu_job(1, 0, 10),
      one_gpu_job(2, 0, 3),
      one_gpu_job(3, 2, 1),
  });

  assert(helios::run_sjf_simulation(workload, total_resources()) ==
         helios::run_sjf_simulation(workload, total_resources()));
}

void fifo_and_sjf_compare_on_large_varied_workload() {
  const auto workload = helios::make_demo_workload("large-varied-100");

  assert(workload.jobs.size() == 100);

  const auto fifo = helios::run_fifo_simulation(workload, helios::demo_cluster_resources());
  const auto sjf = helios::run_sjf_simulation(workload, helios::demo_cluster_resources());

  assert(fifo.jobs.size() == 100);
  assert(sjf.jobs.size() == 100);
  assert(helios::run_fifo_simulation(workload, helios::demo_cluster_resources()) == fifo);
  assert(helios::run_sjf_simulation(workload, helios::demo_cluster_resources()) == sjf);
  assert(helios::total_wait_time(sjf) < helios::total_wait_time(fifo));
  assert(helios::average_wait_time(sjf) < helios::average_wait_time(fifo));
}

} // namespace

int main() {
  empty_simulator_returns_no_decision();
  future_jobs_return_no_decision();
  single_arrived_fitting_job_is_selected();
  single_arrived_non_fitting_job_returns_no_decision();
  shorter_duration_wins_among_schedulable_jobs();
  future_short_job_does_not_win_before_arrival();
  equal_duration_tie_breaks_by_arrival_time();
  equal_duration_and_arrival_tie_breaks_by_job_id();
  skips_non_fitting_jobs_and_selects_shortest_fitting_job();
  schedulable_jobs_with_none_fitting_return_no_decision();
  scheduler_does_not_mutate_state();
  invalid_simulator_state_is_rejected();
  fifo_and_sjf_match_when_jobs_arrive_one_at_a_time();
  fifo_and_sjf_differ_on_compare_workload();
  sjf_runner_is_deterministic();
  fifo_and_sjf_compare_on_large_varied_workload();

  return 0;
}
