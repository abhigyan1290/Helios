#pragma once

#include "helios/observation.hpp"
#include "helios/schedule_decision.hpp"

namespace helios {

ScheduleDecision estimated_sjf_schedule(const Observation& observation);

} // namespace helios
