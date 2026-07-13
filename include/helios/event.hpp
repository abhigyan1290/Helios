#pragma once

#include "helios/job.hpp"

namespace helios {

enum class EventType {
  JobArrival,
  JobCompletion,
};

struct Event {
  SimTime time;
  EventType type;
  JobId job_id;

  bool operator==(const Event&) const = default;
};

void validate_event(const Event& event);
bool event_less(const Event& lhs, const Event& rhs);

} // namespace helios
