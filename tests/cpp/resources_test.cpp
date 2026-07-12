#include "helios/resources.hpp"

#include <cassert>
#include <stdexcept>

namespace {

helios::Resources valid_resources() {
  return helios::Resources{
      .gpu_count = 8,
      .cpu_count = 64,
      .memory_mb = 262144,
  };
}

void expect_invalid_argument(const helios::Resources& resources) {
  try {
    helios::validate_resources(resources);
  } catch (const std::invalid_argument&) {
    return;
  }

  assert(false && "expected std::invalid_argument");
}

void valid_resources_pass_validation() {
  helios::validate_resources(valid_resources());
}

void zero_capacity_resources_pass_validation() {
  const helios::Resources resources{
      .gpu_count = 0,
      .cpu_count = 0,
      .memory_mb = 0,
  };

  helios::validate_resources(resources);
}

void negative_gpu_count_is_rejected() {
  auto resources = valid_resources();
  resources.gpu_count = -1;

  expect_invalid_argument(resources);
}

void negative_cpu_count_is_rejected() {
  auto resources = valid_resources();
  resources.cpu_count = -1;

  expect_invalid_argument(resources);
}

void negative_memory_is_rejected() {
  auto resources = valid_resources();
  resources.memory_mb = -1;

  expect_invalid_argument(resources);
}

void equal_resources_compare_equal() {
  assert(valid_resources() == valid_resources());
}

void different_gpu_values_compare_unequal() {
  auto first = valid_resources();
  auto second = valid_resources();
  second.gpu_count = 4;

  assert(!(first == second));
}

void different_cpu_values_compare_unequal() {
  auto first = valid_resources();
  auto second = valid_resources();
  second.cpu_count = 32;

  assert(!(first == second));
}

void different_memory_values_compare_unequal() {
  auto first = valid_resources();
  auto second = valid_resources();
  second.memory_mb = 131072;

  assert(!(first == second));
}

}  // namespace

int main() {
  valid_resources_pass_validation();
  zero_capacity_resources_pass_validation();
  negative_gpu_count_is_rejected();
  negative_cpu_count_is_rejected();
  negative_memory_is_rejected();
  equal_resources_compare_equal();
  different_gpu_values_compare_unequal();
  different_cpu_values_compare_unequal();
  different_memory_values_compare_unequal();

  return 0;
}
