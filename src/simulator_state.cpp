#include "helios/simulator_state.hpp"
#include "simulator_transitions.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace helios {

namespace {

bool job_less(const Job& lhs, const Job& rhs) {
  if (lhs.arrival_time != rhs.arrival_time) {
    return lhs.arrival_time < rhs.arrival_time;
  }

  return lhs.id < rhs.id;
}

Resources running_resource_usage(const std::vector<RunningJob>& running_jobs) {
  Resources used{
      .gpu_count = 0,
      .cpu_count = 0,
      .memory_mb = 0,
  };

  for (const auto& running_job : running_jobs) {
    used.gpu_count += running_job.job.request.gpu_count;
    used.cpu_count += running_job.job.request.cpu_count;
    used.memory_mb += running_job.job.request.memory_mb;
  }

  return used;
}

void reject_duplicate_id(std::unordered_set<JobId>& ids, JobId id) {
  if (!ids.insert(id).second) {
    throw std::invalid_argument("simulator state cannot contain duplicate job ids");
  }
}

void validate_job_ids_are_unique(const SimulatorState& state) {
  std::unordered_set<JobId> ids;

  for (const auto& job : state.pending_jobs) {
    reject_duplicate_id(ids, job.id);
  }
  for (const auto& running_job : state.running_jobs) {
    reject_duplicate_id(ids, running_job.job.id);
  }
  for (const auto& completed_job : state.completed_jobs) {
    reject_duplicate_id(ids, completed_job.job.id);
  }
}

bool contains_job_id(const SimulatorState& state, JobId job_id) {
  const auto pending_match = std::any_of(state.pending_jobs.begin(), state.pending_jobs.end(),
                                         [job_id](const Job& job) { return job.id == job_id; });
  const auto running_match =
      std::any_of(state.running_jobs.begin(), state.running_jobs.end(),
                  [job_id](const RunningJob& running_job) { return running_job.job.id == job_id; });
  const auto completed_match = std::any_of(
      state.completed_jobs.begin(), state.completed_jobs.end(),
      [job_id](const CompletedJob& completed_job) { return completed_job.job.id == job_id; });

  return pending_match || running_match || completed_match;
}

} // namespace

SimulatorState create_simulator_state(Resources total_resources) {
  validate_resources(total_resources);

  return SimulatorState{
      .current_time = 0,
      .total_resources = total_resources,
      .available_resources = total_resources,
      .pending_jobs = {},
      .running_jobs = {},
      .completed_jobs = {},
  };
}

void validate_simulator_state(const SimulatorState& state) {
  if (state.current_time < 0) {
    throw std::invalid_argument("simulator current time must be non-negative");
  }

  validate_resources(state.total_resources);
  validate_resources(state.available_resources);

  if (!less_equal(state.available_resources, state.total_resources)) {
    throw std::invalid_argument("available resources cannot exceed total resources");
  }

  for (const auto& job : state.pending_jobs) {
    validate_job(job);
  }
  for (const auto& running_job : state.running_jobs) {
    validate_running_job(running_job);
  }
  for (const auto& completed_job : state.completed_jobs) {
    validate_completed_job(completed_job);
  }

  validate_job_ids_are_unique(state);

  const auto used = running_resource_usage(state.running_jobs);
  const Resources accounted{
      .gpu_count = state.available_resources.gpu_count + used.gpu_count,
      .cpu_count = state.available_resources.cpu_count + used.cpu_count,
      .memory_mb = state.available_resources.memory_mb + used.memory_mb,
  };

  if (!less_equal(accounted, state.total_resources)) {
    throw std::invalid_argument("running and available resources cannot exceed total resources");
  }
}

SimulatorState add_pending_job(const SimulatorState& state, const Job& job) {
  validate_simulator_state(state);
  validate_job(job);

  if (contains_job_id(state, job.id)) {
    throw std::runtime_error("job id already exists in simulator state");
  }

  auto next = state;
  next.pending_jobs.push_back(job);
  std::sort(next.pending_jobs.begin(), next.pending_jobs.end(), job_less);
  return next;
}

SimulatorState advance_time(const SimulatorState& state, SimTime new_time) {
  validate_simulator_state(state);

  if (new_time < 0) {
    throw std::invalid_argument("new simulator time must be non-negative");
  }
  if (new_time < state.current_time) {
    throw std::runtime_error("simulator time cannot move backward");
  }

  auto next = state;
  next.current_time = new_time;
  return next;
}

SimulatorState start_pending_job(const SimulatorState& state, JobId job_id) {
  validate_simulator_state(state);

  if (job_id < 0) {
    throw std::invalid_argument("job id must be non-negative");
  }

  const auto job_it = std::find_if(state.pending_jobs.begin(), state.pending_jobs.end(),
                                   [job_id](const Job& job) { return job.id == job_id; });

  if (job_it == state.pending_jobs.end()) {
    throw std::runtime_error("pending job not found");
  }
  if (job_it->arrival_time > state.current_time) {
    throw std::runtime_error("pending job has not arrived");
  }
  if (!fits(state.available_resources, job_it->request)) {
    throw std::runtime_error("pending job does not fit available resources");
  }

  auto next = state;
  detail::start_pending_job_at(next, static_cast<std::size_t>(job_it - state.pending_jobs.begin()));
  return next;
}

SimulatorState complete_running_job(const SimulatorState& state, JobId job_id) {
  validate_simulator_state(state);

  if (job_id < 0) {
    throw std::invalid_argument("job id must be non-negative");
  }

  const auto running_it = std::find_if(
      state.running_jobs.begin(), state.running_jobs.end(),
      [job_id](const RunningJob& running_job) { return running_job.job.id == job_id; });

  if (running_it == state.running_jobs.end()) {
    throw std::runtime_error("running job not found");
  }
  if (running_it->completion_time > state.current_time) {
    throw std::runtime_error("running job has not reached completion time");
  }

  auto next = state;
  detail::complete_running_job_at(
      next, static_cast<std::size_t>(running_it - state.running_jobs.begin()));
  return next;
}

std::vector<Job> get_schedulable_jobs(const SimulatorState& state) {
  validate_simulator_state(state);

  std::vector<Job> schedulable_jobs;
  for (const auto& job : state.pending_jobs) {
    if (job.arrival_time <= state.current_time) {
      schedulable_jobs.push_back(job);
    }
  }

  if (!std::is_sorted(schedulable_jobs.begin(), schedulable_jobs.end(), job_less)) {
    std::sort(schedulable_jobs.begin(), schedulable_jobs.end(), job_less);
  }
  return schedulable_jobs;
}

namespace detail {

void start_pending_job_at(SimulatorState& state, std::size_t index) {
  const auto& job = state.pending_jobs.at(index);
  const auto running = start_runtime_job(job, state.current_time);
  const auto available = allocate(state.available_resources, job.request);
  state.running_jobs.push_back(running);
  state.available_resources = available;
  state.pending_jobs.erase(state.pending_jobs.begin() + index);
}

void complete_running_job_at(SimulatorState& state, std::size_t index) {
  const auto& running = state.running_jobs.at(index);
  if (running.completion_time > state.current_time) {
    throw std::runtime_error("running job has not reached completion time");
  }
  const auto completed = complete_runtime_job(running);
  const auto available =
      release(state.available_resources, state.total_resources, running.job.request);
  state.completed_jobs.push_back(completed);
  state.available_resources = available;
  state.running_jobs.erase(state.running_jobs.begin() + index);
}

} // namespace detail

} // namespace helios
