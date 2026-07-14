#include "helios/schedule_decision.hpp"

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

helios::Job small_job(helios::JobId id, helios::SimTime arrival_time = 0) {
  return helios::Job{
      .id = id,
      .arrival_time = arrival_time,
      .duration = 5,
      .request =
          helios::Resources{
              .gpu_count = 1,
              .cpu_count = 4,
              .memory_mb = 8192,
          },
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

template <typename Function> void expect_runtime_error_from(Function function) {
  try {
    function();
  } catch (const std::runtime_error&) {
    return;
  }

  assert(false && "expected std::runtime_error");
}

void no_schedule_decision_has_no_job_id() {
  assert(!helios::no_schedule_decision().job_id.has_value());
}

void select_job_stores_job_id() {
  const auto decision = helios::select_job(7);

  assert(decision.job_id.has_value());
  assert(*decision.job_id == 7);
}

void select_job_rejects_negative_job_id() {
  expect_invalid_argument_from([] { helios::select_job(-1); });
}

void applying_empty_decision_returns_unchanged_state() {
  const auto state = helios::create_simulator_state(total_resources());

  assert(helios::apply_schedule_decision(state, helios::no_schedule_decision()) == state);
}

void applying_job_decision_starts_pending_job() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1));

  const auto next = helios::apply_schedule_decision(state, helios::select_job(1));

  assert(next.pending_jobs.empty());
  assert(next.running_jobs.size() == 1);
  assert(next.running_jobs.front().job.id == 1);
}

void applying_future_job_decision_throws() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1, 10));
  const auto original = state;

  expect_runtime_error_from([&] { helios::apply_schedule_decision(state, helios::select_job(1)); });
  assert(state == original);
}

void applying_running_job_decision_throws() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1));
  state = helios::start_pending_job(state, 1);
  const auto original = state;

  expect_runtime_error_from([&] { helios::apply_schedule_decision(state, helios::select_job(1)); });
  assert(state == original);
}

void applying_completed_job_decision_throws() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1));
  state = helios::start_pending_job(state, 1);
  state = helios::advance_time(state, 5);
  state = helios::complete_running_job(state, 1);
  const auto original = state;

  expect_runtime_error_from([&] { helios::apply_schedule_decision(state, helios::select_job(1)); });
  assert(state == original);
}

void applying_decision_propagates_invalid_state_errors() {
  auto state = helios::create_simulator_state(total_resources());
  state.current_time = -1;

  expect_invalid_argument_from(
      [&] { helios::apply_schedule_decision(state, helios::select_job(1)); });
}

} // namespace

int main() {
  no_schedule_decision_has_no_job_id();
  select_job_stores_job_id();
  select_job_rejects_negative_job_id();
  applying_empty_decision_returns_unchanged_state();
  applying_job_decision_starts_pending_job();
  applying_future_job_decision_throws();
  applying_running_job_decision_throws();
  applying_completed_job_decision_throws();
  applying_decision_propagates_invalid_state_errors();

  return 0;
}
