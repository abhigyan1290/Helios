#pragma once

#include "helios/job.hpp"

#include <vector>

namespace helios {

struct Workload {
  std::vector<Job> jobs;

  bool operator==(const Workload&) const = default;
};

void validate_workload(const Workload& workload);
Workload make_workload(std::vector<Job> jobs);

} // namespace helios
