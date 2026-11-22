#pragma once

#include "helios/observation.hpp"
#include "helios/schedule_decision.hpp"

namespace helios {

// One FIFO head reservation, based solely on visible runtime estimates.
ScheduleDecision easy_backfill_schedule(const Observation& observation);

} // namespace helios
