#include "helios/demo_workloads.hpp"
#include "helios/runtime_estimates.hpp"
#include "helios/schedulers/easy_backfill_scheduler.hpp"
#include "helios/schedulers/estimated_sjf_scheduler.hpp"
#include "helios/schedulers/fifo_scheduler.hpp"
#include "helios/schedulers/random_scheduler.hpp"
#include "helios/schedulers/sjf_scheduler.hpp"
#include "helios/simulation_runner.hpp"
#include "helios/simulator.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <vector>

namespace {

const helios::Resources capacity{4, 8, 32};

helios::Job job(int id, int duration, helios::Resources request, int estimate, int arrival = 0) {
  return {id, arrival, duration, request, estimate};
}

template <class Error, class Function> void rejects(Function function) {
  try {
    function();
  } catch (const Error&) {
    return;
  }
  assert(false && "expected exception");
}

helios::SimTime start(const helios::SimulationResult& result, int id) {
  const auto found = std::find_if(result.jobs.begin(), result.jobs.end(),
                                  [id](const auto& metrics) { return metrics.job_id == id; });
  assert(found != result.jobs.end());
  return found->start_time;
}

void estimates_control_selection_not_execution() {
  const auto workload = helios::make_workload({job(1, 1, capacity, 20), job(2, 20, capacity, 1)});
  const auto estimated = helios::run_estimated_sjf_simulation(workload, capacity);
  const auto oracle = helios::run_sjf_simulation(workload, capacity);
  assert(start(estimated, 2) == 0 && start(estimated, 1) == 20);
  assert(start(oracle, 1) == 0 && start(oracle, 2) == 1);
  assert(estimated.makespan == 21 && oracle.makespan == 21);
  auto missing = workload;
  missing.jobs[0].estimated_duration.reset();
  rejects<std::invalid_argument>([&] { helios::run_estimated_sjf_simulation(missing, capacity); });
  rejects<std::invalid_argument>(
      [&] { helios::run_observed_simulation(workload, capacity, helios::ObservationPolicy{}); });
}

void estimated_sjf_ties_fit_and_arrivals_are_deterministic() {
  helios::Simulator sim(
      helios::make_workload({job(4, 10, {2, 0, 0}, 10), job(3, 1, capacity, 1, 0),
                             job(2, 4, {1, 0, 0}, 4, 1), job(1, 4, {1, 0, 0}, 4, 1),
                             job(5, 1, {1, 0, 0}, 1, 20)}),
      capacity);
  sim.step(helios::select_job(4));
  sim.step(helios::no_schedule_decision());
  assert(helios::estimated_sjf_schedule(sim.observe()).job_id == 1);
  assert(!helios::fifo_policy(sim.observe()).job_id);
  helios::Simulator future(helios::make_workload({{1, 10, 1, {1, 0, 0}}}), capacity);
  assert(!helios::estimated_sjf_schedule(future.observe()).job_id);
}

void random_policy_is_repeatable_resettable_and_resource_aware() {
  helios::Simulator sim(helios::make_workload({job(1, 10, {2, 0, 0}, 10), job(2, 5, capacity, 5),
                                               job(3, 5, {1, 0, 0}, 5), job(4, 5, {1, 0, 0}, 5),
                                               job(5, 1, {1, 0, 0}, 1, 20)}),
                        capacity);
  sim.step(helios::select_job(1));
  const auto observation = sim.observe();
  helios::RandomScheduler first(42), second(42), other(7);
  std::vector<helios::ScheduleDecision> sequence, alternative;
  for (int i = 0; i < 20; ++i) {
    const auto decision = first(observation);
    assert(decision == second(observation));
    assert(decision.job_id == 3 || decision.job_id == 4);
    sequence.push_back(decision);
    alternative.push_back(other(observation));
  }
  assert(sequence != alternative);
  first.reset(42);
  for (const auto& expected : sequence) {
    assert(first(observation) == expected);
  }
  assert(sim.observe() == observation);
}

helios::Observation blocked_head(int candidate_duration, int candidate_estimate,
                                 helios::Resources head_request = capacity) {
  helios::Simulator sim(
      helios::make_workload({job(1, 10, {2, 0, 0}, 10), job(2, 5, head_request, 5),
                             job(3, candidate_duration, {1, 1, 1}, candidate_estimate)}),
      capacity);
  sim.step(helios::select_job(1));
  return sim.observe();
}

void backfill_preserves_estimated_head_reservation() {
  auto safe = blocked_head(3, 3);
  assert(!helios::fifo_policy(safe).job_id);
  assert(helios::easy_backfill_schedule(safe).job_id == 3);
  auto boundary = blocked_head(10, 10);
  assert(helios::easy_backfill_schedule(boundary).job_id == 3);
  auto unsafe = blocked_head(11, 11);
  assert(!helios::easy_backfill_schedule(unsafe).job_id);
  auto overlapping = blocked_head(20, 20, {3, 0, 0});
  assert(helios::easy_backfill_schedule(overlapping).job_id == 3);
  assert(!helios::easy_backfill_schedule(blocked_head(20, 20, {3, 8, 0})).job_id);
  assert(!helios::easy_backfill_schedule(blocked_head(20, 20, {3, 0, 32})).job_id);
  safe.running_jobs[0].job.estimated_duration.reset();
  rejects<std::invalid_argument>([&] { helios::easy_backfill_schedule(safe); });
}

void underestimation_can_delay_reservations_without_changing_actual_duration() {
  auto workload = helios::make_workload(
      {job(1, 10, {2, 0, 0}, 10), job(2, 5, capacity, 5), job(3, 11, {1, 0, 0}, 2)});
  const auto underestimated = helios::run_easy_backfill_simulation(workload, capacity);
  assert(start(underestimated, 2) == 11);
  assert(underestimated.makespan == 16);
  workload.jobs[2].estimated_duration = 11;
  const auto accurate = helios::run_easy_backfill_simulation(workload, capacity);
  assert(start(accurate, 2) == 10);
  assert(start(accurate, 3) == 15);
  assert(accurate.makespan == 26);
}

void simultaneous_estimated_releases_are_combined_before_testing_overlap() {
  helios::Simulator sim(
      helios::make_workload({job(1, 10, {2, 0, 0}, 10), job(2, 5, {3, 0, 0}, 5),
                             job(3, 20, {1, 0, 0}, 20), job(4, 10, {1, 0, 0}, 10)}),
      capacity);
  sim.step(helios::select_job(4));
  sim.step(helios::select_job(1));
  const auto before = sim.observe();
  assert(helios::easy_backfill_schedule(before).job_id == 3);
  assert(sim.observe() == before);
}

void batch_and_interactive_paths_match_for_all_policies() {
  for (const auto& spec : helios::demo_workload_specs()) {
    const auto workload =
        helios::with_runtime_estimates(helios::make_demo_workload(spec.name), 100);
    const auto resources = helios::demo_cluster_resources();
    assert(helios::run_fifo_simulation(workload, resources) ==
           helios::run_simulation(workload, resources, helios::fifo_schedule));
    assert(helios::run_sjf_simulation(workload, resources) ==
           helios::run_simulation(workload, resources, helios::sjf_schedule));
    assert(helios::run_sjf_simulation(workload, resources) ==
           helios::run_estimated_sjf_simulation(workload, resources));
    for (helios::ObservationPolicy policy :
         {helios::ObservationPolicy(helios::fifo_policy),
          helios::ObservationPolicy(helios::estimated_sjf_schedule),
          helios::ObservationPolicy(helios::easy_backfill_schedule),
          helios::ObservationPolicy(helios::RandomScheduler(42))}) {
      helios::Simulator sim(workload, resources);
      auto stepping_policy = policy;
      while (!sim.done()) {
        sim.step(stepping_policy(sim.observe()));
        sim.validate();
      }
      assert(sim.result() == helios::run_observed_simulation(workload, resources, policy));
      assert(sim.result().jobs.size() == workload.jobs.size());
    }
  }
  const auto empty = helios::make_workload({});
  assert(helios::run_random_simulation(empty, capacity, 0).jobs.empty());
  assert(helios::run_easy_backfill_simulation(empty, capacity).jobs.empty());
  assert(helios::run_estimated_sjf_simulation(empty, capacity).jobs.empty());
}

} // namespace

int main() {
  estimates_control_selection_not_execution();
  estimated_sjf_ties_fit_and_arrivals_are_deterministic();
  random_policy_is_repeatable_resettable_and_resource_aware();
  backfill_preserves_estimated_head_reservation();
  underestimation_can_delay_reservations_without_changing_actual_duration();
  simultaneous_estimated_releases_are_combined_before_testing_overlap();
  batch_and_interactive_paths_match_for_all_policies();
}
