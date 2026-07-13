#include "helios/event.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <vector>

namespace {

helios::Event arrival_event() {
  return helios::Event{
      .time = 10,
      .type = helios::EventType::JobArrival,
      .job_id = 1,
  };
}

helios::Event completion_event() {
  return helios::Event{
      .time = 10,
      .type = helios::EventType::JobCompletion,
      .job_id = 1,
  };
}

template <typename Function> void expect_invalid_argument_from(Function function) {
  try {
    function();
  } catch (const std::invalid_argument&) {
    return;
  }

  assert(false && "expected std::invalid_argument");
}

void valid_arrival_event_passes_validation() {
  helios::validate_event(arrival_event());
}

void valid_completion_event_passes_validation() {
  helios::validate_event(completion_event());
}

void negative_event_time_is_rejected() {
  auto event = arrival_event();
  event.time = -1;

  expect_invalid_argument_from([&] { helios::validate_event(event); });
}

void negative_job_id_is_rejected() {
  auto event = arrival_event();
  event.job_id = -1;

  expect_invalid_argument_from([&] { helios::validate_event(event); });
}

void earlier_event_sorts_first() {
  auto earlier = arrival_event();
  earlier.time = 5;

  assert(helios::event_less(earlier, arrival_event()));
  assert(!helios::event_less(arrival_event(), earlier));
}

void completion_sorts_before_arrival_at_same_time() {
  assert(helios::event_less(completion_event(), arrival_event()));
  assert(!helios::event_less(arrival_event(), completion_event()));
}

void smaller_job_id_sorts_first_when_time_and_type_match() {
  auto smaller = arrival_event();
  smaller.job_id = 1;
  auto larger = arrival_event();
  larger.job_id = 2;

  assert(helios::event_less(smaller, larger));
  assert(!helios::event_less(larger, smaller));
}

void sorting_events_is_deterministic() {
  std::vector<helios::Event> events{
      helios::Event{.time = 10, .type = helios::EventType::JobArrival, .job_id = 2},
      helios::Event{.time = 5, .type = helios::EventType::JobArrival, .job_id = 3},
      helios::Event{.time = 10, .type = helios::EventType::JobCompletion, .job_id = 2},
      helios::Event{.time = 10, .type = helios::EventType::JobArrival, .job_id = 1},
  };
  auto sorted_once = events;
  auto sorted_twice = events;

  std::sort(sorted_once.begin(), sorted_once.end(), helios::event_less);
  std::sort(sorted_twice.begin(), sorted_twice.end(), helios::event_less);

  const std::vector<helios::Event> expected{
      helios::Event{.time = 5, .type = helios::EventType::JobArrival, .job_id = 3},
      helios::Event{.time = 10, .type = helios::EventType::JobCompletion, .job_id = 2},
      helios::Event{.time = 10, .type = helios::EventType::JobArrival, .job_id = 1},
      helios::Event{.time = 10, .type = helios::EventType::JobArrival, .job_id = 2},
  };

  assert(sorted_once == expected);
  assert(sorted_twice == expected);
}

} // namespace

int main() {
  valid_arrival_event_passes_validation();
  valid_completion_event_passes_validation();
  negative_event_time_is_rejected();
  negative_job_id_is_rejected();
  earlier_event_sorts_first();
  completion_sorts_before_arrival_at_same_time();
  smaller_job_id_sorts_first_when_time_and_type_match();
  sorting_events_is_deterministic();

  return 0;
}
