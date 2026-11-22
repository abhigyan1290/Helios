#include "helios/simulator.hpp"

#include "simulator_transitions.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace helios {

Simulator::Simulator(const Workload& workload, Resources total_resources) {
  reset(workload, total_resources);
}

void Simulator::reset(const Workload& workload, Resources total_resources) {
  auto next = create_simulator_state(total_resources);
  next.pending_jobs = make_workload(workload.jobs).jobs;
  for (const auto& job : next.pending_jobs) {
    if (!fits(total_resources, job.request)) {
      throw std::runtime_error("workload contains a job that cannot fit total resources");
    }
  }
  next.running_jobs.reserve(
      std::min(next.pending_jobs.size(), static_cast<std::size_t>(total_resources.gpu_count)));
  next.completed_jobs.reserve(next.pending_jobs.size());
  state_ = std::move(next);
}

bool Simulator::done() const noexcept {
  return state_.pending_jobs.empty() && state_.running_jobs.empty();
}

Observation Simulator::observation(bool oracle) const {
  const auto view = [oracle](const Job& job) {
    return PendingJobView{job.id, job.arrival_time,
                          oracle ? std::optional<Duration>{job.duration} : job.estimated_duration,
                          job.request};
  };
  Observation result{state_.current_time,
                     state_.total_resources,
                     state_.available_resources,
                     {},
                     {},
                     state_.completed_jobs.size(),
                     done()};
  for (const auto& job : state_.pending_jobs) {
    if (job.arrival_time > state_.current_time) {
      break;
    }
    result.pending_jobs.push_back(view(job));
  }
  for (const auto& running : state_.running_jobs) {
    result.running_jobs.push_back(
        {view(running.job), running.start_time, state_.current_time - running.start_time});
  }
  return result;
}

Observation Simulator::observe() const {
  return observation(false);
}

Observation Simulator::observe_oracle() const {
  return observation(true);
}

const SimulatorState& Simulator::diagnostic_state() const noexcept {
  return state_;
}

void Simulator::validate() const {
  validate_simulator_state(state_);
}

SimulationResult Simulator::result() const {
  if (!done()) {
    throw std::runtime_error("simulation results are only available after termination");
  }
  return make_simulation_result(state_.completed_jobs);
}

StepResult Simulator::step(const ScheduleDecision& decision) {
  if (decision.job_id && *decision.job_id < 0) {
    throw std::invalid_argument("selected job id must be non-negative");
  }
  if (done()) {
    throw std::runtime_error("cannot step a terminated simulator");
  }
  StepResult result{0, std::nullopt, {}, false};
  if (decision.job_id) {
    const auto it = std::find_if(state_.pending_jobs.begin(), state_.pending_jobs.end(),
                                 [&](const Job& job) { return job.id == *decision.job_id; });
    if (it == state_.pending_jobs.end()) {
      throw std::runtime_error("pending job not found");
    }
    detail::start_pending_job_at(state_,
                                 static_cast<std::size_t>(it - state_.pending_jobs.begin()));
    result.started_job_id = decision.job_id;
    return result;
  }

  std::optional<SimTime> next_time;
  const auto arrival =
      std::upper_bound(state_.pending_jobs.begin(), state_.pending_jobs.end(), state_.current_time,
                       [](SimTime time, const Job& job) { return time < job.arrival_time; });
  if (arrival != state_.pending_jobs.end()) {
    next_time = arrival->arrival_time;
  }
  for (const auto& running : state_.running_jobs) {
    if (!next_time || running.completion_time < *next_time) {
      next_time = running.completion_time;
    }
  }
  if (!next_time) {
    throw std::runtime_error("simulation cannot make progress: select an arrived fitting job");
  }
  for (const auto& running : state_.running_jobs) {
    if (running.completion_time == *next_time) {
      result.completed_job_ids.push_back(running.job.id);
    }
  }
  std::sort(result.completed_job_ids.begin(), result.completed_job_ids.end());
  result.elapsed_time = *next_time - state_.current_time;
  state_.current_time = *next_time;
  for (const auto id : result.completed_job_ids) {
    const auto running = std::find_if(state_.running_jobs.begin(), state_.running_jobs.end(),
                                      [id](const RunningJob& job) { return job.job.id == id; });
    detail::complete_running_job_at(
        state_, static_cast<std::size_t>(running - state_.running_jobs.begin()));
  }
  result.terminated = done();
  return result;
}

} // namespace helios
