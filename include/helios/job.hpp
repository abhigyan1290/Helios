#pragma once

#include "helios/resources.hpp"

namespace helios {

using JobId = int;
using SimTime = int;
using Duration = int;

struct Job {
  JobId id;
  SimTime arrival_time;
  Duration duration;
  Resources request;

  bool operator==(const Job&) const = default;
};

void validate_job(const Job& job);

} // namespace helios
