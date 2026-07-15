#include "helios/simulation_runner.hpp"

#include "helios/schedule_decision.hpp"
#include "helios/schedulers/fifo_scheduler.hpp"
#include "helios/simulator_state.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>

namespace helios {

namespace {

void reject_unschedulable_jobs(const Workload& workload, const Resources& total_resources) {
  for (const auto& job : workload.jobs) {
    if (!fits(total_resources, job.request)) {
      throw std::runtime_error("workload contains a job that cannot fit total resources");
    }
  }
}

std::optional<JobId> due_running_job_id(const SimulatorState& state) {
  std::optional<RunningJob> due_job;

  for (const auto& running_job : state.running_jobs) {
    if (running_job.completion_time > state.current_time) {
      continue;
    }

    if (!due_job.has_value() || running_job.completion_time < due_job->completion_time ||
        (running_job.completion_time == due_job->completion_time &&
         running_job.job.id < due_job->job.id)) {
      due_job = running_job;
    }
  }

  if (!due_job.has_value()) {
    return std::nullopt;
  }

  return due_job->job.id;
}

std::optional<SimTime> next_running_completion_time(const SimulatorState& state) {
  std::optional<SimTime> next_time;

  for (const auto& running_job : state.running_jobs) {
    if (running_job.completion_time <= state.current_time) {
      continue;
    }

    if (!next_time.has_value() || running_job.completion_time < *next_time) {
      next_time = running_job.completion_time;
    }
  }

  return next_time;
}

std::optional<SimTime> next_pending_arrival_time(const SimulatorState& state) {
  std::optional<SimTime> next_time;

  for (const auto& job : state.pending_jobs) {
    if (job.arrival_time <= state.current_time) {
      continue;
    }

    if (!next_time.has_value() || job.arrival_time < *next_time) {
      next_time = job.arrival_time;
    }
  }

  return next_time;
}

std::optional<SimTime> min_time(std::optional<SimTime> lhs, std::optional<SimTime> rhs) {
  if (!lhs.has_value()) {
    return rhs;
  }
  if (!rhs.has_value()) {
    return lhs;
  }

  return std::min(*lhs, *rhs);
}

std::optional<SimTime> next_relevant_future_time(const SimulatorState& state) {
  const auto next_completion = next_running_completion_time(state);
  const auto schedulable_jobs = get_schedulable_jobs(state);

  if (!schedulable_jobs.empty()) {
    const auto& head_job = schedulable_jobs.front();
    if (!fits(state.available_resources, head_job.request)) {
      return next_completion;
    }
  }

  return min_time(next_completion, next_pending_arrival_time(state));
}

SimulatorState complete_due_jobs(SimulatorState state) {
  while (true) {
    const auto job_id = due_running_job_id(state);
    if (!job_id.has_value()) {
      return state;
    }

    state = complete_running_job(state, *job_id);
  }
}

SimulatorState start_fifo_jobs(SimulatorState state, bool& started_any) {
  started_any = false;

  while (true) {
    const auto decision = fifo_schedule(state);
    if (!decision.job_id.has_value()) {
      return state;
    }

    state = apply_schedule_decision(state, decision);
    started_any = true;
  }
}

bool is_finished(const SimulatorState& state) {
  return state.pending_jobs.empty() && state.running_jobs.empty();
}

} // namespace

SimulationResult run_fifo_simulation(const Workload& workload, Resources total_resources) {
  validate_workload(workload);
  validate_resources(total_resources);
  reject_unschedulable_jobs(workload, total_resources);

  auto state = create_simulator_state(total_resources);
  for (const auto& job : workload.jobs) {
    state = add_pending_job(state, job);
  }

  while (!is_finished(state)) {
    state = complete_due_jobs(state);

    bool started_any = false;
    state = start_fifo_jobs(state, started_any);

    if (is_finished(state)) {
      break;
    }

    if (started_any) {
      continue;
    }

    const auto next_time = next_relevant_future_time(state);
    if (!next_time.has_value()) {
      throw std::runtime_error("simulation cannot make progress");
    }

    state = advance_time(state, *next_time);
  }

  return make_simulation_result(state.completed_jobs);
}

} // namespace helios
