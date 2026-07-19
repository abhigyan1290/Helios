#include "helios/demo_workloads.hpp"

#include <stdexcept>
#include <vector>

namespace helios {

namespace {

Job make_job(JobId id, SimTime arrival_time, Duration duration, ResourceAmount gpu_count,
             ResourceAmount cpu_count, ResourceAmount memory_mb) {
  return Job{
      .id = id,
      .arrival_time = arrival_time,
      .duration = duration,
      .request =
          Resources{
              .gpu_count = gpu_count,
              .cpu_count = cpu_count,
              .memory_mb = memory_mb,
          },
  };
}

Workload light_overlap_workload() {
  return make_workload({
      make_job(1, 0, 5, 1, 4, 8192),
      make_job(2, 2, 4, 2, 8, 16384),
      make_job(3, 6, 3, 1, 4, 8192),
  });
}

Workload fifo_blocking_workload() {
  return make_workload({
      make_job(1, 0, 10, 4, 8, 16384),
      make_job(2, 0, 3, 1, 4, 8192),
      make_job(3, 0, 2, 1, 4, 8192),
  });
}

Workload staggered_arrivals_workload() {
  return make_workload({
      make_job(1, 5, 4, 1, 4, 8192),
      make_job(2, 8, 6, 2, 8, 16384),
      make_job(3, 9, 2, 1, 4, 8192),
      make_job(4, 12, 3, 1, 4, 8192),
  });
}

Workload gpu_contention_workload() {
  return make_workload({
      make_job(1, 0, 8, 2, 8, 16384),
      make_job(2, 0, 8, 2, 8, 16384),
      make_job(3, 1, 4, 2, 8, 16384),
      make_job(4, 2, 3, 1, 4, 8192),
  });
}

Workload large_varied_100_workload() {
  std::vector<Job> jobs;
  jobs.reserve(100);

  for (int wave = 0; wave < 10; ++wave) {
    const auto base_id = static_cast<JobId>(wave * 10 + 1);
    const auto base_time = static_cast<SimTime>(wave * 6);

    jobs.push_back(make_job(base_id, base_time, 18 + (wave % 3), 4, 16, 32768));
    jobs.push_back(make_job(base_id + 1, base_time, 2 + (wave % 2), 1, 4, 8192));
    jobs.push_back(make_job(base_id + 2, base_time, 3, 1, 4, 8192));
    jobs.push_back(make_job(base_id + 3, base_time + 1, 5, 2, 8, 16384));
    jobs.push_back(make_job(base_id + 4, base_time + 1, 1, 1, 2, 4096));
    jobs.push_back(make_job(base_id + 5, base_time + 2, 7, 2, 8, 16384));
    jobs.push_back(make_job(base_id + 6, base_time + 2, 2, 1, 4, 8192));
    jobs.push_back(make_job(base_id + 7, base_time + 3, 4, 1, 4, 8192));
    jobs.push_back(make_job(base_id + 8, base_time + 4, 9, 3, 12, 24576));
    jobs.push_back(make_job(base_id + 9, base_time + 5, 1, 1, 2, 4096));
  }

  return make_workload(std::move(jobs));
}

} // namespace

std::vector<DemoWorkloadSpec> demo_workload_specs() {
  return {
      DemoWorkloadSpec{
          .name = "light-overlap",
          .description = "small jobs with enough resources to overlap",
      },
      DemoWorkloadSpec{
          .name = "fifo-blocking",
          .description = "strict FIFO blocks smaller jobs behind a full-cluster job",
      },
      DemoWorkloadSpec{
          .name = "staggered-arrivals",
          .description = "jobs arrive after time zero and must not start early",
      },
      DemoWorkloadSpec{
          .name = "gpu-contention",
          .description = "multiple GPU-heavy jobs create resource contention",
      },
      DemoWorkloadSpec{
          .name = "large-varied-100",
          .description = "100 mixed jobs with staggered arrivals and varied resource requests",
      },
  };
}

Resources demo_cluster_resources() {
  return Resources{
      .gpu_count = 4,
      .cpu_count = 32,
      .memory_mb = 131072,
  };
}

Workload make_demo_workload(std::string_view name) {
  if (name == "light-overlap") {
    return light_overlap_workload();
  }
  if (name == "fifo-blocking") {
    return fifo_blocking_workload();
  }
  if (name == "staggered-arrivals") {
    return staggered_arrivals_workload();
  }
  if (name == "gpu-contention") {
    return gpu_contention_workload();
  }
  if (name == "large-varied-100") {
    return large_varied_100_workload();
  }

  throw std::invalid_argument("unknown demo workload");
}

} // namespace helios
