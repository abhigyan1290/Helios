#include "helios/workload_generator.hpp"

#include <cassert>
#include <stdexcept>
#include <unordered_set>

namespace {

helios::Resources cluster_resources() {
  return helios::Resources{
      .gpu_count = 4,
      .cpu_count = 32,
      .memory_mb = 131072,
  };
}

helios::WorkloadGeneratorConfig valid_config() {
  return helios::WorkloadGeneratorConfig{
      .job_count = 25,
      .seed = 123,
      .max_arrival_time = 50,
      .min_duration = 1,
      .max_duration = 20,
      .min_gpu_count = 1,
      .max_gpu_count = 4,
      .min_cpu_count = 0,
      .max_cpu_count = 16,
      .min_memory_mb = 0,
      .max_memory_mb = 65536,
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

void valid_config_passes_validation() {
  helios::validate_workload_generator_config(valid_config(), cluster_resources());
}

void negative_job_count_is_rejected() {
  auto config = valid_config();
  config.job_count = -1;

  expect_invalid_argument_from(
      [&] { helios::validate_workload_generator_config(config, cluster_resources()); });
}

void negative_arrival_bound_is_rejected() {
  auto config = valid_config();
  config.max_arrival_time = -1;

  expect_invalid_argument_from(
      [&] { helios::validate_workload_generator_config(config, cluster_resources()); });
}

void zero_min_duration_is_rejected() {
  auto config = valid_config();
  config.min_duration = 0;

  expect_invalid_argument_from(
      [&] { helios::validate_workload_generator_config(config, cluster_resources()); });
}

void invalid_duration_range_is_rejected() {
  auto config = valid_config();
  config.min_duration = 10;
  config.max_duration = 9;

  expect_invalid_argument_from(
      [&] { helios::validate_workload_generator_config(config, cluster_resources()); });
}

void zero_min_gpu_count_is_rejected() {
  auto config = valid_config();
  config.min_gpu_count = 0;

  expect_invalid_argument_from(
      [&] { helios::validate_workload_generator_config(config, cluster_resources()); });
}

void invalid_gpu_range_is_rejected() {
  auto config = valid_config();
  config.min_gpu_count = 3;
  config.max_gpu_count = 2;

  expect_invalid_argument_from(
      [&] { helios::validate_workload_generator_config(config, cluster_resources()); });
}

void gpu_max_above_cluster_is_rejected() {
  auto config = valid_config();
  config.max_gpu_count = 5;

  expect_invalid_argument_from(
      [&] { helios::validate_workload_generator_config(config, cluster_resources()); });
}

void cpu_max_above_cluster_is_rejected() {
  auto config = valid_config();
  config.max_cpu_count = 33;

  expect_invalid_argument_from(
      [&] { helios::validate_workload_generator_config(config, cluster_resources()); });
}

void memory_max_above_cluster_is_rejected() {
  auto config = valid_config();
  config.max_memory_mb = 131073;

  expect_invalid_argument_from(
      [&] { helios::validate_workload_generator_config(config, cluster_resources()); });
}

void negative_cpu_min_is_rejected() {
  auto config = valid_config();
  config.min_cpu_count = -1;

  expect_invalid_argument_from(
      [&] { helios::validate_workload_generator_config(config, cluster_resources()); });
}

void negative_memory_min_is_rejected() {
  auto config = valid_config();
  config.min_memory_mb = -1;

  expect_invalid_argument_from(
      [&] { helios::validate_workload_generator_config(config, cluster_resources()); });
}

void generated_workload_has_requested_job_count() {
  const auto workload = helios::generate_workload(valid_config(), cluster_resources());

  assert(workload.jobs.size() == 25);
}

void generated_job_ids_are_unique_and_stable() {
  const auto workload = helios::generate_workload(valid_config(), cluster_resources());
  std::unordered_set<helios::JobId> ids;

  for (const auto& job : workload.jobs) {
    assert(job.id >= 1);
    assert(job.id <= 25);
    assert(ids.insert(job.id).second);
  }

  assert(ids.size() == 25);
}

void generated_jobs_stay_within_configured_ranges() {
  const auto config = valid_config();
  const auto workload = helios::generate_workload(config, cluster_resources());

  for (const auto& job : workload.jobs) {
    assert(job.arrival_time >= 0);
    assert(job.arrival_time <= config.max_arrival_time);
    assert(job.duration >= config.min_duration);
    assert(job.duration <= config.max_duration);
    assert(job.request.gpu_count >= config.min_gpu_count);
    assert(job.request.gpu_count <= config.max_gpu_count);
    assert(job.request.cpu_count >= config.min_cpu_count);
    assert(job.request.cpu_count <= config.max_cpu_count);
    assert(job.request.memory_mb >= config.min_memory_mb);
    assert(job.request.memory_mb <= config.max_memory_mb);
  }
}

void every_generated_job_fits_cluster_resources() {
  const auto workload = helios::generate_workload(valid_config(), cluster_resources());

  for (const auto& job : workload.jobs) {
    assert(helios::fits(cluster_resources(), job.request));
  }
}

void same_config_and_seed_generate_identical_workloads() {
  const auto config = valid_config();

  assert(helios::generate_workload(config, cluster_resources()) ==
         helios::generate_workload(config, cluster_resources()));
}

void different_fixed_seeds_generate_different_workloads() {
  auto first = valid_config();
  auto second = valid_config();
  second.seed = 456;

  assert(helios::generate_workload(first, cluster_resources()) !=
         helios::generate_workload(second, cluster_resources()));
}

void empty_generated_workload_is_valid() {
  auto config = valid_config();
  config.job_count = 0;

  const auto workload = helios::generate_workload(config, cluster_resources());

  assert(workload.jobs.empty());
  helios::validate_workload(workload);
}

} // namespace

int main() {
  valid_config_passes_validation();
  negative_job_count_is_rejected();
  negative_arrival_bound_is_rejected();
  zero_min_duration_is_rejected();
  invalid_duration_range_is_rejected();
  zero_min_gpu_count_is_rejected();
  invalid_gpu_range_is_rejected();
  gpu_max_above_cluster_is_rejected();
  cpu_max_above_cluster_is_rejected();
  memory_max_above_cluster_is_rejected();
  negative_cpu_min_is_rejected();
  negative_memory_min_is_rejected();
  generated_workload_has_requested_job_count();
  generated_job_ids_are_unique_and_stable();
  generated_jobs_stay_within_configured_ranges();
  every_generated_job_fits_cluster_resources();
  same_config_and_seed_generate_identical_workloads();
  different_fixed_seeds_generate_different_workloads();
  empty_generated_workload_is_valid();

  return 0;
}
