#pragma once

#include "helios/observation.hpp"
#include "helios/schedule_decision.hpp"
#include "helios/simulator_state.hpp"

namespace helios {

ScheduleDecision fifo_schedule(const SimulatorState& state);
ScheduleDecision fifo_policy(const Observation& observation);

} // namespace helios
