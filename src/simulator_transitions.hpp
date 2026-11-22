#pragma once

#include "helios/simulator_state.hpp"

#include <cstddef>

namespace helios::detail {

// Caller owns a validated state. Selected-job checks still run before mutation.
void start_pending_job_at(SimulatorState& state, std::size_t index);
void complete_running_job_at(SimulatorState& state, std::size_t index);

} // namespace helios::detail
