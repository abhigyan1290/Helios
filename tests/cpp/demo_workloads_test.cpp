#include "helios/demo_workloads.hpp"
#include "helios/simulation_runner.hpp"

#include <algorithm>
#include <array>
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

void generated_profiles_are_repeatable_and_complete() {
  struct ExpectedProfile {
    std::string_view name;
    std::size_t job_count;
    helios::SimTime max_arrival_time;
    helios::ResourceAmount min_gpu_count;
  };
  const std::array profiles{
      ExpectedProfile{"generated-small", 25, 50, 1},
      ExpectedProfile{"generated-medium", 100, 100, 1},
      ExpectedProfile{"generated-large", 500, 500, 1},
      ExpectedProfile{"generated-bursty", 100, 3, 1},
      ExpectedProfile{"generated-gpu-heavy", 100, 100, 2},
  };
  const auto specs = helios::demo_workload_specs();
  const auto resources = helios::demo_cluster_resources();

  for (const auto& profile : profiles) {
    assert(std::any_of(specs.begin(), specs.end(),
                       [&](const auto& spec) { return spec.name == profile.name; }));
    const auto workload = helios::make_demo_workload(profile.name);
    assert(workload.jobs.size() == profile.job_count);
    assert(workload == helios::make_demo_workload(profile.name));
    helios::validate_workload(workload);
    for (const auto& job : workload.jobs) {
      assert(job.arrival_time >= 0 && job.arrival_time <= profile.max_arrival_time);
      assert(job.duration >= 1 && job.duration <= 20);
      assert(job.request.gpu_count >= profile.min_gpu_count);
      assert(helios::fits(resources, job.request));
    }

    const auto fifo = helios::run_fifo_simulation(workload, resources);
    const auto sjf = helios::run_sjf_simulation(workload, resources);
    for (const auto* result : {&fifo, &sjf}) {
      assert(result->jobs.size() == profile.job_count);
      std::unordered_set<helios::JobId> completed_ids;
      for (const auto& metrics : result->jobs) {
        assert(completed_ids.insert(metrics.job_id).second);
        const auto job =
            std::find_if(workload.jobs.begin(), workload.jobs.end(),
                         [&](const auto& input) { return input.id == metrics.job_id; });
        assert(job != workload.jobs.end());
        assert(metrics.arrival_time == job->arrival_time);
        assert(metrics.duration == job->duration);
        assert(metrics.start_time >= job->arrival_time);
        assert(metrics.completion_time == metrics.start_time + job->duration);
      }
    }
  }
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
  generated_profiles_are_repeatable_and_complete();

  return 0;
}
