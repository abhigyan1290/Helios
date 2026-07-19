#include "helios/demo_workloads.hpp"

#include <cassert>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

namespace {

void demo_specs_have_unique_names() {
  std::unordered_set<std::string_view> names;

  for (const auto& spec : helios::demo_workload_specs()) {
    assert(!spec.name.empty());
    assert(!spec.description.empty());
    assert(names.insert(spec.name).second);
  }
}

void every_demo_spec_builds_valid_workload() {
  for (const auto& spec : helios::demo_workload_specs()) {
    const auto workload = helios::make_demo_workload(spec.name);

    helios::validate_workload(workload);
    assert(!workload.jobs.empty());
  }
}

void demo_cluster_resources_are_valid() {
  helios::validate_resources(helios::demo_cluster_resources());
}

void unknown_demo_workload_is_rejected() {
  try {
    (void)helios::make_demo_workload("missing");
  } catch (const std::invalid_argument&) {
    return;
  }

  assert(false && "expected std::invalid_argument");
}

void fifo_blocking_workload_has_full_cluster_head_job() {
  const auto workload = helios::make_demo_workload("fifo-blocking");

  assert(workload.jobs.front().id == 1);
  assert(workload.jobs.front().request.gpu_count == helios::demo_cluster_resources().gpu_count);
}

void staggered_arrivals_start_after_time_zero() {
  const auto workload = helios::make_demo_workload("staggered-arrivals");

  assert(!workload.jobs.empty());
  assert(workload.jobs.front().arrival_time > 0);
}

void large_varied_workload_has_mixed_job_shapes() {
  const auto workload = helios::make_demo_workload("large-varied-100");

  bool has_later_arrival = false;
  bool has_one_gpu_job = false;
  bool has_full_cluster_job = false;

  assert(workload.jobs.size() == 100);

  for (const auto& job : workload.jobs) {
    has_later_arrival = has_later_arrival || job.arrival_time > 0;
    has_one_gpu_job = has_one_gpu_job || job.request.gpu_count == 1;
    has_full_cluster_job =
        has_full_cluster_job || job.request.gpu_count == helios::demo_cluster_resources().gpu_count;
  }

  assert(has_later_arrival);
  assert(has_one_gpu_job);
  assert(has_full_cluster_job);
}

} // namespace

int main() {
  demo_specs_have_unique_names();
  every_demo_spec_builds_valid_workload();
  demo_cluster_resources_are_valid();
  unknown_demo_workload_is_rejected();
  fifo_blocking_workload_has_full_cluster_head_job();
  staggered_arrivals_start_after_time_zero();
  large_varied_workload_has_mixed_job_shapes();

  return 0;
}
