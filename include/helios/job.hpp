#pragma once

#include "helios/resources.hpp"

#include <optional>

namespace helios {

using JobId = int;
using SimTime = int;
using Duration = int;

struct Job {
  JobId id;
  SimTime arrival_time;
  Duration duration;
  Resources request;
  std::optional<Duration> estimated_duration = std::nullopt;

  bool operator==(const Job&) const = default;
};

void validate_job(const Job& job);

} // namespace helios
