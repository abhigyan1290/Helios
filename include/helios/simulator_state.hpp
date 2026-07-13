#pragma once

#include "helios/job_state.hpp"

#include <vector>

namespace helios {

struct SimulatorState {
  SimTime current_time;
  Resources total_resources;
  Resources available_resources;
  std::vector<Job> pending_jobs;
  std::vector<RunningJob> running_jobs;
  std::vector<CompletedJob> completed_jobs;

  bool operator==(const SimulatorState&) const = default;
};

SimulatorState create_simulator_state(Resources total_resources);
void validate_simulator_state(const SimulatorState& state);

SimulatorState add_pending_job(const SimulatorState& state, const Job& job);
SimulatorState advance_time(const SimulatorState& state, SimTime new_time);
SimulatorState start_pending_job(const SimulatorState& state, JobId job_id);
SimulatorState complete_running_job(const SimulatorState& state, JobId job_id);
std::vector<Job> get_schedulable_jobs(const SimulatorState& state);

} // namespace helios
