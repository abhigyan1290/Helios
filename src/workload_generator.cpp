#include "helios/workload_generator.hpp"

#include <random>
#include <stdexcept>
#include <vector>

namespace helios {

namespace {

void reject_invalid_range(int min_value, int max_value, const char* message) {
  if (max_value < min_value) {
    throw std::invalid_argument(message);
  }
}

int sample_inclusive(std::mt19937& rng, int min_value, int max_value) {
  std::uniform_int_distribution<int> distribution(min_value, max_value);
  return distribution(rng);
}

} // namespace

void validate_workload_generator_config(const WorkloadGeneratorConfig& config,
                                        Resources cluster_resources) {
  validate_resources(cluster_resources);

  if (config.job_count < 0) {
    throw std::invalid_argument("workload generator job count must be non-negative");
  }
  if (config.max_arrival_time < 0) {
    throw std::invalid_argument("workload generator max arrival time must be non-negative");
  }
  if (config.min_duration <= 0) {
    throw std::invalid_argument("workload generator minimum duration must be positive");
  }
  reject_invalid_range(config.min_duration, config.max_duration,
                       "workload generator duration range is invalid");

  if (config.min_gpu_count <= 0) {
    throw std::invalid_argument("workload generator minimum GPU count must be positive");
  }
  reject_invalid_range(config.min_gpu_count, config.max_gpu_count,
                       "workload generator GPU range is invalid");

  if (config.min_cpu_count < 0) {
    throw std::invalid_argument("workload generator minimum CPU count must be non-negative");
  }
  reject_invalid_range(config.min_cpu_count, config.max_cpu_count,
                       "workload generator CPU range is invalid");

  if (config.min_memory_mb < 0) {
    throw std::invalid_argument("workload generator minimum memory must be non-negative");
  }
  reject_invalid_range(config.min_memory_mb, config.max_memory_mb,
                       "workload generator memory range is invalid");

  if (config.max_gpu_count > cluster_resources.gpu_count) {
    throw std::invalid_argument("workload generator GPU maximum exceeds cluster resources");
  }
  if (config.max_cpu_count > cluster_resources.cpu_count) {
    throw std::invalid_argument("workload generator CPU maximum exceeds cluster resources");
  }
  if (config.max_memory_mb > cluster_resources.memory_mb) {
    throw std::invalid_argument("workload generator memory maximum exceeds cluster resources");
  }
}

Workload generate_workload(const WorkloadGeneratorConfig& config, Resources cluster_resources) {
  validate_workload_generator_config(config, cluster_resources);

  std::mt19937 rng(config.seed);
  std::vector<Job> jobs;
  jobs.reserve(static_cast<std::size_t>(config.job_count));

  for (int index = 0; index < config.job_count; ++index) {
    const Resources request{
        .gpu_count = sample_inclusive(rng, config.min_gpu_count, config.max_gpu_count),
        .cpu_count = sample_inclusive(rng, config.min_cpu_count, config.max_cpu_count),
        .memory_mb = sample_inclusive(rng, config.min_memory_mb, config.max_memory_mb),
    };

    jobs.push_back(Job{
        .id = index + 1,
        .arrival_time = sample_inclusive(rng, 0, config.max_arrival_time),
        .duration = sample_inclusive(rng, config.min_duration, config.max_duration),
        .request = request,
    });
  }

  return make_workload(std::move(jobs));
}

} // namespace helios
