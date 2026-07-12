#include "helios/job.hpp"

#include <cassert>
#include <stdexcept>

namespace {

helios::Job valid_job() {
  return helios::Job{
      .id = 1,
      .arrival_time = 10,
      .duration = 5,
      .request =
          helios::Resources{
              .gpu_count = 2,
              .cpu_count = 8,
              .memory_mb = 16384,
          },
  };
}

void expect_invalid_argument(const helios::Job& job) {
  try {
    helios::validate_job(job);
  } catch (const std::invalid_argument&) {
    return;
  }

  assert(false && "expected std::invalid_argument");
}

void valid_job_passes_validation() {
  helios::validate_job(valid_job());
}

void negative_id_is_rejected() {
  auto job = valid_job();
  job.id = -1;

  expect_invalid_argument(job);
}

void negative_arrival_time_is_rejected() {
  auto job = valid_job();
  job.arrival_time = -1;

  expect_invalid_argument(job);
}

void zero_duration_is_rejected() {
  auto job = valid_job();
  job.duration = 0;

  expect_invalid_argument(job);
}

void negative_duration_is_rejected() {
  auto job = valid_job();
  job.duration = -1;

  expect_invalid_argument(job);
}

void zero_gpu_request_is_rejected() {
  auto job = valid_job();
  job.request.gpu_count = 0;

  expect_invalid_argument(job);
}

void negative_gpu_request_is_rejected() {
  auto job = valid_job();
  job.request.gpu_count = -1;

  expect_invalid_argument(job);
}

void negative_cpu_request_is_rejected() {
  auto job = valid_job();
  job.request.cpu_count = -1;

  expect_invalid_argument(job);
}

void negative_memory_request_is_rejected() {
  auto job = valid_job();
  job.request.memory_mb = -1;

  expect_invalid_argument(job);
}

void minimal_valid_job_is_accepted() {
  const helios::Job job{
      .id = 0,
      .arrival_time = 0,
      .duration = 1,
      .request =
          helios::Resources{
              .gpu_count = 1,
              .cpu_count = 0,
              .memory_mb = 0,
          },
  };

  helios::validate_job(job);
}

void equal_jobs_compare_equal() {
  assert(valid_job() == valid_job());
}

void different_jobs_compare_unequal() {
  auto first = valid_job();
  auto second = valid_job();
  second.id = 2;

  assert(!(first == second));
}

} // namespace

int main() {
  valid_job_passes_validation();
  negative_id_is_rejected();
  negative_arrival_time_is_rejected();
  zero_duration_is_rejected();
  negative_duration_is_rejected();
  zero_gpu_request_is_rejected();
  negative_gpu_request_is_rejected();
  negative_cpu_request_is_rejected();
  negative_memory_request_is_rejected();
  minimal_valid_job_is_accepted();
  equal_jobs_compare_equal();
  different_jobs_compare_unequal();

  return 0;
}
