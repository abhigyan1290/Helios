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

template <typename Function> void expect_invalid_argument_from(Function function) {
  try {
    function();
  } catch (const std::invalid_argument&) {
    return;
  }

  assert(false && "expected std::invalid_argument");
}

template <typename Function> void expect_runtime_error_from(Function function) {
  try {
    function();
  } catch (const std::runtime_error&) {
    return;
  }

  assert(false && "expected std::runtime_error");
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

void less_equal_accepts_exact_match() {
  assert(helios::less_equal(valid_resources(), valid_resources()));
}

void less_equal_accepts_smaller_values() {
  const helios::Resources smaller{
      .gpu_count = 4,
      .cpu_count = 32,
      .memory_mb = 131072,
  };

  assert(helios::less_equal(smaller, valid_resources()));
}

void less_equal_rejects_excess_gpu_count() {
  auto larger = valid_resources();
  larger.gpu_count = 9;

  assert(!helios::less_equal(larger, valid_resources()));
}

void less_equal_rejects_excess_cpu_count() {
  auto larger = valid_resources();
  larger.cpu_count = 65;

  assert(!helios::less_equal(larger, valid_resources()));
}

void less_equal_rejects_excess_memory() {
  auto larger = valid_resources();
  larger.memory_mb = 262145;

  assert(!helios::less_equal(larger, valid_resources()));
}

void fits_accepts_available_resources() {
  const helios::Resources requested{
      .gpu_count = 2,
      .cpu_count = 8,
      .memory_mb = 16384,
  };

  assert(helios::fits(valid_resources(), requested));
}

void fits_rejects_insufficient_gpu_count() {
  const helios::Resources requested{
      .gpu_count = 9,
      .cpu_count = 8,
      .memory_mb = 16384,
  };

  assert(!helios::fits(valid_resources(), requested));
}

void fits_rejects_insufficient_cpu_count() {
  const helios::Resources requested{
      .gpu_count = 2,
      .cpu_count = 65,
      .memory_mb = 16384,
  };

  assert(!helios::fits(valid_resources(), requested));
}

void fits_rejects_insufficient_memory() {
  const helios::Resources requested{
      .gpu_count = 2,
      .cpu_count = 8,
      .memory_mb = 262145,
  };

  assert(!helios::fits(valid_resources(), requested));
}

void fits_accepts_exact_match() {
  assert(helios::fits(valid_resources(), valid_resources()));
}

void empty_cluster_does_not_fit_gpu_request() {
  const helios::Resources empty{
      .gpu_count = 0,
      .cpu_count = 0,
      .memory_mb = 0,
  };
  const helios::Resources requested{
      .gpu_count = 1,
      .cpu_count = 0,
      .memory_mb = 0,
  };

  assert(!helios::fits(empty, requested));
}

void allocation_subtracts_requested_resources() {
  const helios::Resources requested{
      .gpu_count = 2,
      .cpu_count = 8,
      .memory_mb = 16384,
  };
  const helios::Resources expected{
      .gpu_count = 6,
      .cpu_count = 56,
      .memory_mb = 245760,
  };

  assert(helios::allocate(valid_resources(), requested) == expected);
}

void exact_fit_allocation_leaves_zero_resources() {
  const helios::Resources expected{
      .gpu_count = 0,
      .cpu_count = 0,
      .memory_mb = 0,
  };

  assert(helios::allocate(valid_resources(), valid_resources()) == expected);
}

void allocation_rejects_request_that_does_not_fit() {
  const auto available = valid_resources();
  const helios::Resources requested{
      .gpu_count = 9,
      .cpu_count = 64,
      .memory_mb = 262144,
  };

  expect_runtime_error_from([&] { helios::allocate(available, requested); });
  assert(available == valid_resources());
}

void release_adds_released_resources() {
  const helios::Resources available{
      .gpu_count = 6,
      .cpu_count = 56,
      .memory_mb = 245760,
  };
  const helios::Resources released{
      .gpu_count = 2,
      .cpu_count = 8,
      .memory_mb = 16384,
  };

  assert(helios::release(available, valid_resources(), released) == valid_resources());
}

void allocate_then_release_returns_to_original_resources() {
  const auto total = valid_resources();
  const helios::Resources requested{
      .gpu_count = 2,
      .cpu_count = 8,
      .memory_mb = 16384,
  };

  const auto after_allocation = helios::allocate(total, requested);
  const auto after_release = helios::release(after_allocation, total, requested);

  assert(after_release == total);
}

void release_rejects_available_gpu_count_above_total() {
  auto available = valid_resources();
  available.gpu_count = 9;

  expect_invalid_argument_from(
      [&] { helios::release(available, valid_resources(), helios::Resources{0, 0, 0}); });
}

void release_rejects_available_cpu_count_above_total() {
  auto available = valid_resources();
  available.cpu_count = 65;

  expect_invalid_argument_from(
      [&] { helios::release(available, valid_resources(), helios::Resources{0, 0, 0}); });
}

void release_rejects_available_memory_above_total() {
  auto available = valid_resources();
  available.memory_mb = 262145;

  expect_invalid_argument_from(
      [&] { helios::release(available, valid_resources(), helios::Resources{0, 0, 0}); });
}

void release_rejects_over_release() {
  const helios::Resources available{
      .gpu_count = 7,
      .cpu_count = 64,
      .memory_mb = 262144,
  };
  const helios::Resources released{
      .gpu_count = 2,
      .cpu_count = 0,
      .memory_mb = 0,
  };

  expect_runtime_error_from([&] { helios::release(available, valid_resources(), released); });
  assert(available.gpu_count == 7);
}

void release_allows_zero_resources() {
  const helios::Resources released{
      .gpu_count = 0,
      .cpu_count = 0,
      .memory_mb = 0,
  };

  assert(helios::release(valid_resources(), valid_resources(), released) == valid_resources());
}

} // namespace

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
  less_equal_accepts_exact_match();
  less_equal_accepts_smaller_values();
  less_equal_rejects_excess_gpu_count();
  less_equal_rejects_excess_cpu_count();
  less_equal_rejects_excess_memory();
  fits_accepts_available_resources();
  fits_rejects_insufficient_gpu_count();
  fits_rejects_insufficient_cpu_count();
  fits_rejects_insufficient_memory();
  fits_accepts_exact_match();
  empty_cluster_does_not_fit_gpu_request();
  allocation_subtracts_requested_resources();
  exact_fit_allocation_leaves_zero_resources();
  allocation_rejects_request_that_does_not_fit();
  release_adds_released_resources();
  allocate_then_release_returns_to_original_resources();
  release_rejects_available_gpu_count_above_total();
  release_rejects_available_cpu_count_above_total();
  release_rejects_available_memory_above_total();
  release_rejects_over_release();
  release_allows_zero_resources();

  return 0;
}
