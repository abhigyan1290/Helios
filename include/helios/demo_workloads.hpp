#pragma once

#include "helios/resources.hpp"
#include "helios/workload.hpp"

#include <string_view>
#include <vector>

namespace helios {

struct DemoWorkloadSpec {
  std::string_view name;
  std::string_view description;
};

std::vector<DemoWorkloadSpec> demo_workload_specs();
Resources demo_cluster_resources();
Workload make_demo_workload(std::string_view name);

} // namespace helios
