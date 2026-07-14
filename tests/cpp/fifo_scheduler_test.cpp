#include "helios/schedulers/fifo_scheduler.hpp"

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

helios::Job make_job(helios::JobId id, helios::SimTime arrival_time,
                     helios::ResourceAmount gpu_count) {
  return helios::Job{
      .id = id,
      .arrival_time = arrival_time,
      .duration = 5,
      .request =
          helios::Resources{
              .gpu_count = gpu_count,
              .cpu_count = 4,
              .memory_mb = 8192,
          },
  };
}

helios::Job small_job(helios::JobId id, helios::SimTime arrival_time = 0) {
  return make_job(id, arrival_time, 1);
}

helios::Job full_cluster_job(helios::JobId id, helios::SimTime arrival_time = 0) {
  return make_job(id, arrival_time, 4);
}

helios::Job too_large_job(helios::JobId id, helios::SimTime arrival_time = 0) {
  return make_job(id, arrival_time, 5);
}

template <typename Function> void expect_invalid_argument_from(Function function) {
  try {
    function();
  } catch (const std::invalid_argument&) {
    return;
  }

  assert(false && "expected std::invalid_argument");
}

bool has_no_job(const helios::ScheduleDecision& decision) {
  return !decision.job_id.has_value();
}

helios::JobId selected_job_id(const helios::ScheduleDecision& decision) {
  assert(decision.job_id.has_value());
  return *decision.job_id;
}

void empty_simulator_returns_no_decision() {
  const auto state = helios::create_simulator_state(total_resources());

  assert(has_no_job(helios::fifo_schedule(state)));
}

void future_job_returns_no_decision() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1, 10));

  assert(has_no_job(helios::fifo_schedule(state)));
}

void single_arrived_fitting_job_is_selected() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1));

  assert(selected_job_id(helios::fifo_schedule(state)) == 1);
}

void single_arrived_non_fitting_job_returns_no_decision() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, too_large_job(1));

  assert(has_no_job(helios::fifo_schedule(state)));
}

void multiple_arrived_jobs_select_earliest_arrival() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(2, 10));
  state = helios::add_pending_job(state, small_job(1, 5));
  state = helios::advance_time(state, 10);

  assert(selected_job_id(helios::fifo_schedule(state)) == 1);
}

void equal_arrival_times_select_smaller_job_id() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(2));
  state = helios::add_pending_job(state, small_job(1));

  assert(selected_job_id(helios::fifo_schedule(state)) == 1);
}

void out_of_order_insertions_still_select_fifo_head() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(3, 10));
  state = helios::add_pending_job(state, small_job(1, 5));
  state = helios::add_pending_job(state, small_job(2, 5));
  state = helios::advance_time(state, 10);

  assert(selected_job_id(helios::fifo_schedule(state)) == 1);
}

void fifo_does_not_skip_unfitting_head_job() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, too_large_job(1));
  state = helios::add_pending_job(state, small_job(2));

  assert(has_no_job(helios::fifo_schedule(state)));
}

void fifo_does_not_mutate_state_when_returning_decision() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1));
  const auto original = state;

  assert(selected_job_id(helios::fifo_schedule(state)) == 1);
  assert(state == original);
}

void fifo_does_not_mutate_state_when_head_job_does_not_fit() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, too_large_job(1));
  const auto original = state;

  assert(has_no_job(helios::fifo_schedule(state)));
  assert(state == original);
}

void invalid_simulator_state_is_rejected() {
  auto state = helios::create_simulator_state(total_resources());
  state.current_time = -1;

  expect_invalid_argument_from([&] { helios::fifo_schedule(state); });
}

void fifo_decision_starts_one_pending_job() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1));

  const auto next = helios::apply_schedule_decision(state, helios::fifo_schedule(state));

  assert(next.pending_jobs.empty());
  assert(next.running_jobs.size() == 1);
  assert(next.running_jobs.front().job.id == 1);
}

void applying_fifo_subtracts_resources() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1));

  const auto next = helios::apply_schedule_decision(state, helios::fifo_schedule(state));
  const helios::Resources expected{
      .gpu_count = 3,
      .cpu_count = 28,
      .memory_mb = 122880,
  };

  assert(next.available_resources == expected);
}

void applying_fifo_does_not_complete_jobs_automatically() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1));
  state = helios::apply_schedule_decision(state, helios::fifo_schedule(state));
  state = helios::advance_time(state, 5);

  assert(state.running_jobs.size() == 1);
  assert(state.completed_jobs.empty());
}

void empty_fifo_decision_leaves_state_unchanged() {
  const auto state = helios::create_simulator_state(total_resources());

  assert(helios::apply_schedule_decision(state, helios::fifo_schedule(state)) == state);
}

void repeated_fifo_application_starts_multiple_jobs_when_resources_allow() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1));
  state = helios::add_pending_job(state, small_job(2));
  state = helios::add_pending_job(state, small_job(3));

  state = helios::apply_schedule_decision(state, helios::fifo_schedule(state));
  assert(state.running_jobs.back().job.id == 1);
  state = helios::apply_schedule_decision(state, helios::fifo_schedule(state));
  assert(state.running_jobs.back().job.id == 2);
  state = helios::apply_schedule_decision(state, helios::fifo_schedule(state));
  assert(state.running_jobs.back().job.id == 3);

  assert(state.pending_jobs.empty());
  assert(state.running_jobs.size() == 3);
}

void repeated_fifo_application_stops_when_head_job_does_not_fit() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, full_cluster_job(1));
  state = helios::add_pending_job(state, small_job(2));
  state = helios::apply_schedule_decision(state, helios::fifo_schedule(state));
  const auto original = state;

  assert(has_no_job(helios::fifo_schedule(state)));
  assert(helios::apply_schedule_decision(state, helios::fifo_schedule(state)) == original);
  assert(state.pending_jobs.size() == 1);
  assert(state.pending_jobs.front().id == 2);
}

} // namespace

int main() {
  empty_simulator_returns_no_decision();
  future_job_returns_no_decision();
  single_arrived_fitting_job_is_selected();
  single_arrived_non_fitting_job_returns_no_decision();
  multiple_arrived_jobs_select_earliest_arrival();
  equal_arrival_times_select_smaller_job_id();
  out_of_order_insertions_still_select_fifo_head();
  fifo_does_not_skip_unfitting_head_job();
  fifo_does_not_mutate_state_when_returning_decision();
  fifo_does_not_mutate_state_when_head_job_does_not_fit();
  invalid_simulator_state_is_rejected();
  fifo_decision_starts_one_pending_job();
  applying_fifo_subtracts_resources();
  applying_fifo_does_not_complete_jobs_automatically();
  empty_fifo_decision_leaves_state_unchanged();
  repeated_fifo_application_starts_multiple_jobs_when_resources_allow();
  repeated_fifo_application_stops_when_head_job_does_not_fit();

  return 0;
}
