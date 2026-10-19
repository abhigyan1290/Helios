#pragma once

#include "helios/resources.hpp"
#include "helios/workload.hpp"

namespace helios {

struct WorkloadGeneratorConfig {
  int job_count;
  unsigned int seed;
  SimTime max_arrival_time;
  Duration min_duration;
  Duration max_duration;
  ResourceAmount min_gpu_count;
  ResourceAmount max_gpu_count;
  ResourceAmount min_cpu_count;
  ResourceAmount max_cpu_count;
  ResourceAmount min_memory_mb;
  ResourceAmount max_memory_mb;

  bool operator==(const WorkloadGeneratorConfig&) const = default;
};

void validate_workload_generator_config(const WorkloadGeneratorConfig& config,
                                        Resources cluster_resources);
Workload generate_workload(const WorkloadGeneratorConfig& config, Resources cluster_resources);

} // namespace helios
