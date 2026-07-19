#pragma once

#include "helios/schedule_decision.hpp"
#include "helios/simulator_state.hpp"

namespace helios {

ScheduleDecision sjf_schedule(const SimulatorState& state);

} // namespace helios
