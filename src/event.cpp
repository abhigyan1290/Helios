#include "helios/event.hpp"

#include <stdexcept>

namespace helios {

namespace {

int event_type_rank(EventType type) {
  switch (type) {
  case EventType::JobCompletion:
    return 0;
  case EventType::JobArrival:
    return 1;
  }

  return 2;
}

} // namespace

void validate_event(const Event& event) {
  if (event.time < 0) {
    throw std::invalid_argument("event time must be non-negative");
  }
  if (event.job_id < 0) {
    throw std::invalid_argument("event job id must be non-negative");
  }
}

bool event_less(const Event& lhs, const Event& rhs) {
  validate_event(lhs);
  validate_event(rhs);

  if (lhs.time != rhs.time) {
    return lhs.time < rhs.time;
  }
  if (lhs.type != rhs.type) {
    return event_type_rank(lhs.type) < event_type_rank(rhs.type);
  }

  return lhs.job_id < rhs.job_id;
}

} // namespace helios
