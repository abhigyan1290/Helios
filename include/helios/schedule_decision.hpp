#pragma once

#include "helios/job.hpp"
#include "helios/simulator_state.hpp"

#include <optional>

namespace helios {

struct ScheduleDecision {
  std::optional<JobId> job_id;

  bool operator==(const ScheduleDecision&) const = default;
};

ScheduleDecision no_schedule_decision();
ScheduleDecision select_job(JobId job_id);
SimulatorState apply_schedule_decision(const SimulatorState& state,
                                       const ScheduleDecision& decision);

} // namespace helios
