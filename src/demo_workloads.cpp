#include "helios/demo_workloads.hpp"
#include "helios/workload_generator.hpp"

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

Workload generated_profile(int job_count, unsigned int seed, SimTime max_arrival_time,
                           ResourceAmount min_gpu_count = 1) {
  return generate_workload(
      WorkloadGeneratorConfig{
          .job_count = job_count,
          .seed = seed,
          .max_arrival_time = max_arrival_time,
          .min_duration = 1,
          .max_duration = 20,
          .min_gpu_count = min_gpu_count,
          .max_gpu_count = 4,
          .min_cpu_count = 2,
          .max_cpu_count = 16,
          .min_memory_mb = 4096,
          .max_memory_mb = 32768,
      },
      demo_cluster_resources());
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
      DemoWorkloadSpec{
          .name = "generated-small",
          .description = "25 seeded jobs arriving between times 0 and 50",
      },
      DemoWorkloadSpec{
          .name = "generated-medium",
          .description = "100 seeded jobs arriving between times 0 and 100",
      },
      DemoWorkloadSpec{
          .name = "generated-large",
          .description = "500 seeded jobs arriving between times 0 and 500",
      },
      DemoWorkloadSpec{
          .name = "generated-bursty",
          .description = "100 seeded jobs concentrated between times 0 and 3",
      },
      DemoWorkloadSpec{
          .name = "generated-gpu-heavy",
          .description = "100 seeded jobs each requesting 2 to 4 GPUs",
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
  if (name == "generated-small") {
    return generated_profile(25, 701, 50);
  }
  if (name == "generated-medium") {
    return generated_profile(100, 702, 100);
  }
  if (name == "generated-large") {
    return generated_profile(500, 703, 500);
  }
  if (name == "generated-bursty") {
    return generated_profile(100, 704, 3);
  }
  if (name == "generated-gpu-heavy") {
    return generated_profile(100, 705, 100, 2);
  }

  throw std::invalid_argument("unknown demo workload");
}

} // namespace helios
