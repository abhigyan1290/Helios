#include "helios/simulator.hpp"

#include <cassert>
#include <limits>
#include <stdexcept>

namespace {

const helios::Resources capacity{2, 8, 32};

helios::Job job(int id, int arrival, int duration, int gpus = 1,
                std::optional<helios::Duration> estimate = std::nullopt) {
  return helios::Job{id, arrival, duration, {gpus, 0, 0}, estimate};
}

template <class Error, class Function> void rejects(Function function) {
  try {
    function();
  } catch (const Error&) {
    return;
  }
  assert(false && "expected exception");
}

void empty_episode_is_terminal() {
  helios::Simulator sim(helios::make_workload({}), capacity);
  assert(sim.done() && sim.observe().terminated);
  assert(sim.result().jobs.empty());
  rejects<std::runtime_error>([&] { sim.step(helios::no_schedule_decision()); });
}

void arrivals_completions_and_same_time_starts_are_explicit() {
  helios::Simulator sim(helios::make_workload({job(2, 0, 5), job(1, 0, 5), job(3, 5, 2)}),
                        capacity);
  assert(sim.observe().pending_jobs.size() == 2);
  assert(sim.step(helios::select_job(2)).elapsed_time == 0);
  assert(sim.step(helios::select_job(1)).started_job_id == 1);
  auto event = sim.step(helios::no_schedule_decision());
  assert(event.elapsed_time == 5);
  assert((event.completed_job_ids == std::vector<helios::JobId>{1, 2}));
  assert(!event.terminated);
  const auto observation = sim.observe();
  assert(observation.available_resources == capacity);
  assert(observation.pending_jobs.size() == 1 && observation.pending_jobs[0].id == 3);
  const auto after_completion = sim.diagnostic_state();
  rejects<std::runtime_error>([&] { sim.step(helios::select_job(1)); });
  assert(sim.diagnostic_state() == after_completion);
  sim.step(helios::select_job(3));
  assert(sim.observe().current_time == 5);
  event = sim.step(helios::no_schedule_decision());
  assert(event.elapsed_time == 2 && event.terminated);
  assert(sim.result().makespan == 7);
  sim.validate();
}

void invalid_actions_and_failed_resets_preserve_state() {
  const auto workload = helios::make_workload({job(1, 0, 5), job(2, 0, 2, 2), job(3, 10, 1)});
  helios::Simulator sim(workload, capacity);
  const auto original = sim.diagnostic_state();
  rejects<std::runtime_error>([&] { sim.step(helios::select_job(3)); });
  rejects<std::runtime_error>([&] { sim.step(helios::select_job(99)); });
  rejects<std::invalid_argument>([&] { sim.step(helios::ScheduleDecision{-1}); });
  rejects<std::runtime_error>([&] { sim.result(); });
  assert(sim.diagnostic_state() == original);
  sim.step(helios::select_job(1));
  const auto running = sim.diagnostic_state();
  rejects<std::runtime_error>([&] { sim.step(helios::select_job(1)); });
  rejects<std::runtime_error>([&] { sim.step(helios::select_job(2)); });
  rejects<std::runtime_error>(
      [&] { sim.reset(helios::make_workload({job(9, 0, 1, 3)}), capacity); });
  rejects<std::invalid_argument>(
      [&] { sim.reset(helios::Workload{{job(9, 0, 1), job(9, 0, 2)}}, capacity); });
  assert(sim.diagnostic_state() == running);
  sim.reset(workload, capacity);
  assert(sim.diagnostic_state() == original);
}

void idle_future_arrivals_and_no_progress_are_distinct() {
  helios::Simulator future(helios::make_workload({job(1, 10, 2)}), capacity);
  assert(future.observe().pending_jobs.empty());
  assert(!future.done());
  assert(future.step(helios::no_schedule_decision()).elapsed_time == 10);
  const auto original = future.diagnostic_state();
  rejects<std::runtime_error>([&] { future.step(helios::no_schedule_decision()); });
  assert(future.diagnostic_state() == original);
}

void observations_do_not_reveal_actual_durations_or_future_jobs() {
  helios::Simulator first(helios::make_workload({job(1, 0, 10, 1, 5), job(2, 100, 3)}), capacity);
  helios::Simulator second(helios::make_workload({job(1, 0, 90, 1, 5), job(99, 200, 300)}),
                           capacity);
  assert(first.observe() == second.observe());
  assert(first.observe().pending_jobs.size() == 1);
  first.step(helios::select_job(1));
  second.step(helios::select_job(1));
  assert(first.observe() == second.observe());
  auto view = first.observe();
  view.running_jobs[0].job.id = 777;
  assert(first.observe().running_jobs[0].job.id == 1);
  helios::Simulator unknown(helios::make_workload({job(1, 0, 20)}), capacity);
  assert(!unknown.observe().pending_jobs[0].estimated_duration);
  assert(unknown.observe_oracle().pending_jobs[0].estimated_duration == 20);
}

void overflowing_starts_are_rejected_without_mutation() {
  helios::Simulator sim(helios::make_workload({job(1, std::numeric_limits<int>::max(), 1)}),
                        capacity);
  sim.step(helios::no_schedule_decision());
  const auto original = sim.diagnostic_state();
  rejects<std::runtime_error>([&] { sim.step(helios::select_job(1)); });
  assert(sim.diagnostic_state() == original);
}

} // namespace

int main() {
  empty_episode_is_terminal();
  arrivals_completions_and_same_time_starts_are_explicit();
  invalid_actions_and_failed_resets_preserve_state();
  idle_future_arrivals_and_no_progress_are_distinct();
  observations_do_not_reveal_actual_durations_or_future_jobs();
  overflowing_starts_are_rejected_without_mutation();
}
