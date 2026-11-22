#pragma once

#include "helios/resources.hpp"
#include "helios/workload.hpp"
#include "helios/workload_generator.hpp"

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
WorkloadGeneratorConfig generated_workload_config(std::string_view name, unsigned int seed);
Workload make_generated_workload(std::string_view name, unsigned int seed);
std::vector<DemoWorkloadSpec> scale_workload_specs();

} // namespace helios
