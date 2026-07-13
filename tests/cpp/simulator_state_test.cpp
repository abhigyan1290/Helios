#include "helios/simulator_state.hpp"

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

helios::Job small_job(helios::JobId id, helios::SimTime arrival_time = 0) {
  return make_job(id, arrival_time, 5,
                  helios::Resources{
                      .gpu_count = 1,
                      .cpu_count = 4,
                      .memory_mb = 8192,
                  });
}

helios::Job large_job(helios::JobId id) {
  return make_job(id, 0, 5,
                  helios::Resources{
                      .gpu_count = 5,
                      .cpu_count = 4,
                      .memory_mb = 8192,
                  });
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

void empty_simulator_has_current_time_zero() {
  assert(helios::create_simulator_state(total_resources()).current_time == 0);
}

void empty_simulator_has_available_resources_equal_to_total() {
  const auto state = helios::create_simulator_state(total_resources());

  assert(state.available_resources == state.total_resources);
}

void empty_simulator_has_no_jobs() {
  const auto state = helios::create_simulator_state(total_resources());

  assert(state.pending_jobs.empty());
  assert(state.running_jobs.empty());
  assert(state.completed_jobs.empty());
}

void adding_one_job_puts_it_in_pending() {
  const auto state =
      helios::add_pending_job(helios::create_simulator_state(total_resources()), small_job(1));

  assert(state.pending_jobs.size() == 1);
  assert(state.pending_jobs.front() == small_job(1));
}

void adding_jobs_orders_by_arrival_time() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(2, 10));
  state = helios::add_pending_job(state, small_job(1, 5));

  assert(state.pending_jobs[0].id == 1);
  assert(state.pending_jobs[1].id == 2);
}

void adding_jobs_with_equal_arrival_orders_by_id() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(2, 5));
  state = helios::add_pending_job(state, small_job(1, 5));

  assert(state.pending_jobs[0].id == 1);
  assert(state.pending_jobs[1].id == 2);
}

void duplicate_job_ids_are_rejected() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1));
  const auto original = state;

  expect_runtime_error_from([&] { helios::add_pending_job(state, small_job(1, 10)); });
  assert(state == original);
}

void schedulable_jobs_exclude_future_pending_jobs() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1, 5));

  assert(helios::get_schedulable_jobs(state).empty());
}

void schedulable_jobs_include_arrived_pending_jobs() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1, 5));
  state = helios::advance_time(state, 5);

  const auto schedulable_jobs = helios::get_schedulable_jobs(state);

  assert(schedulable_jobs.size() == 1);
  assert(schedulable_jobs.front().id == 1);
}

void advancing_time_forward_succeeds() {
  const auto state = helios::advance_time(helios::create_simulator_state(total_resources()), 10);

  assert(state.current_time == 10);
}

void advancing_time_to_same_time_succeeds() {
  const auto state = helios::create_simulator_state(total_resources());

  assert(helios::advance_time(state, 0) == state);
}

void moving_time_backward_is_rejected() {
  const auto state = helios::advance_time(helios::create_simulator_state(total_resources()), 10);
  const auto original = state;

  expect_runtime_error_from([&] { helios::advance_time(state, 9); });
  assert(state == original);
}

void advancing_time_does_not_change_resources() {
  const auto state = helios::create_simulator_state(total_resources());
  const auto advanced = helios::advance_time(state, 10);

  assert(advanced.available_resources == state.available_resources);
  assert(advanced.total_resources == state.total_resources);
}

void advancing_time_does_not_move_pending_jobs() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1, 5));
  const auto advanced = helios::advance_time(state, 10);

  assert(advanced.pending_jobs == state.pending_jobs);
  assert(advanced.running_jobs.empty());
  assert(advanced.completed_jobs.empty());
}

void starting_missing_job_is_rejected() {
  const auto state = helios::create_simulator_state(total_resources());
  const auto original = state;

  expect_runtime_error_from([&] { helios::start_pending_job(state, 1); });
  assert(state == original);
}

void starting_job_before_arrival_is_rejected() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1, 5));
  const auto original = state;

  expect_runtime_error_from([&] { helios::start_pending_job(state, 1); });
  assert(state == original);
}

void starting_job_that_does_not_fit_is_rejected() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, large_job(1));
  const auto original = state;

  expect_runtime_error_from([&] { helios::start_pending_job(state, 1); });
  assert(state == original);
}

void starting_fitting_job_moves_it_from_pending_to_running() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1));
  state = helios::start_pending_job(state, 1);

  assert(state.pending_jobs.empty());
  assert(state.running_jobs.size() == 1);
  assert(state.running_jobs.front().job.id == 1);
}

void starting_fitting_job_subtracts_resources() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1));
  state = helios::start_pending_job(state, 1);
  const helios::Resources expected{
      .gpu_count = 3,
      .cpu_count = 28,
      .memory_mb = 122880,
  };

  assert(state.available_resources == expected);
}

void completing_missing_running_job_is_rejected() {
  const auto state = helios::create_simulator_state(total_resources());
  const auto original = state;

  expect_runtime_error_from([&] { helios::complete_running_job(state, 1); });
  assert(state == original);
}

void completing_running_job_too_early_is_rejected() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1));
  state = helios::start_pending_job(state, 1);
  const auto original = state;

  expect_runtime_error_from([&] { helios::complete_running_job(state, 1); });
  assert(state == original);
}

void completing_running_job_moves_it_to_completed() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1));
  state = helios::start_pending_job(state, 1);
  state = helios::advance_time(state, 5);
  state = helios::complete_running_job(state, 1);

  assert(state.running_jobs.empty());
  assert(state.completed_jobs.size() == 1);
  assert(state.completed_jobs.front().job.id == 1);
}

void completing_running_job_releases_resources() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1));
  state = helios::start_pending_job(state, 1);
  state = helios::advance_time(state, 5);
  state = helios::complete_running_job(state, 1);

  assert(state.available_resources == state.total_resources);
}

void single_job_lifecycle_returns_available_resources_to_total() {
  auto state = helios::create_simulator_state(total_resources());
  state = helios::add_pending_job(state, small_job(1));
  state = helios::start_pending_job(state, 1);
  state = helios::advance_time(state, 5);
  state = helios::complete_running_job(state, 1);

  assert(state.pending_jobs.empty());
  assert(state.running_jobs.empty());
  assert(state.completed_jobs.size() == 1);
  assert(state.available_resources == state.total_resources);
}

void validation_rejects_negative_current_time() {
  auto state = helios::create_simulator_state(total_resources());
  state.current_time = -1;

  expect_invalid_argument_from([&] { helios::validate_simulator_state(state); });
}

void validation_rejects_available_resources_above_total() {
  auto state = helios::create_simulator_state(total_resources());
  state.available_resources.gpu_count = 5;

  expect_invalid_argument_from([&] { helios::validate_simulator_state(state); });
}

void validation_rejects_duplicate_ids_across_containers() {
  auto state = helios::create_simulator_state(total_resources());
  state.pending_jobs.push_back(small_job(1));
  state.completed_jobs.push_back(helios::CompletedJob{
      .job = small_job(1),
      .start_time = 0,
      .completion_time = 5,
  });

  expect_invalid_argument_from([&] { helios::validate_simulator_state(state); });
}

void validation_rejects_inconsistent_running_resource_accounting() {
  auto state = helios::create_simulator_state(total_resources());
  state.available_resources = helios::Resources{
      .gpu_count = 3,
      .cpu_count = 32,
      .memory_mb = 131072,
  };
  state.running_jobs.push_back(helios::RunningJob{
      .job = small_job(1),
      .start_time = 0,
      .completion_time = 5,
  });

  expect_invalid_argument_from([&] { helios::validate_simulator_state(state); });
}

} // namespace

int main() {
  empty_simulator_has_current_time_zero();
  empty_simulator_has_available_resources_equal_to_total();
  empty_simulator_has_no_jobs();
  adding_one_job_puts_it_in_pending();
  adding_jobs_orders_by_arrival_time();
  adding_jobs_with_equal_arrival_orders_by_id();
  duplicate_job_ids_are_rejected();
  schedulable_jobs_exclude_future_pending_jobs();
  schedulable_jobs_include_arrived_pending_jobs();
  advancing_time_forward_succeeds();
  advancing_time_to_same_time_succeeds();
  moving_time_backward_is_rejected();
  advancing_time_does_not_change_resources();
  advancing_time_does_not_move_pending_jobs();
  starting_missing_job_is_rejected();
  starting_job_before_arrival_is_rejected();
  starting_job_that_does_not_fit_is_rejected();
  starting_fitting_job_moves_it_from_pending_to_running();
  starting_fitting_job_subtracts_resources();
  completing_missing_running_job_is_rejected();
  completing_running_job_too_early_is_rejected();
  completing_running_job_moves_it_to_completed();
  completing_running_job_releases_resources();
  single_job_lifecycle_returns_available_resources_to_total();
  validation_rejects_negative_current_time();
  validation_rejects_available_resources_above_total();
  validation_rejects_duplicate_ids_across_containers();
  validation_rejects_inconsistent_running_resource_accounting();

  return 0;
}
