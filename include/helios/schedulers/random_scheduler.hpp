#pragma once

#include "helios/observation.hpp"
#include "helios/schedule_decision.hpp"

#include <random>

namespace helios {

class RandomScheduler {
public:
  explicit RandomScheduler(unsigned int seed);
  ScheduleDecision operator()(const Observation& observation);
  void reset(unsigned int seed);

private:
  std::mt19937 engine_;
};

} // namespace helios
