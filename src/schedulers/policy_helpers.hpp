#pragma once

#include "helios/observation.hpp"

#include <stdexcept>
#include <tuple>

namespace helios::detail {

inline Duration require_estimate(const PendingJobView& job) {
  if (!job.estimated_duration || *job.estimated_duration <= 0) {
    throw std::invalid_argument("policy requires positive runtime estimates for visible jobs");
  }
  return *job.estimated_duration;
}

inline bool arrival_less(const PendingJobView& lhs, const PendingJobView& rhs) {
  return std::tie(lhs.arrival_time, lhs.id) < std::tie(rhs.arrival_time, rhs.id);
}

} // namespace helios::detail
