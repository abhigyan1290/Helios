#include "helios/job_state.hpp"

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

helios::RunningJob valid_running_job() {
  return helios::RunningJob{
      .job = valid_job(),
      .start_time = 10,
      .completion_time = 15,
  };
}

helios::CompletedJob valid_completed_job() {
  return helios::CompletedJob{
      .job = valid_job(),
      .start_time = 10,
      .completion_time = 15,
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

template <typename Function> void expect_runtime_error_from(Function function) {
  try {
    function();
  } catch (const std::runtime_error&) {
    return;
  }

  assert(false && "expected std::runtime_error");
}

void starting_valid_job_creates_running_job() {
  const auto running_job = helios::start_runtime_job(valid_job(), 10);

  assert(running_job.job == valid_job());
  assert(running_job.start_time == 10);
  assert(running_job.completion_time == 15);
}

void starting_at_arrival_time_is_allowed() {
  helios::validate_running_job(helios::start_runtime_job(valid_job(), 10));
}

void starting_after_arrival_time_is_allowed() {
  const auto running_job = helios::start_runtime_job(valid_job(), 12);

  assert(running_job.start_time == 12);
  assert(running_job.completion_time == 17);
}

void starting_before_arrival_time_is_rejected() {
  expect_runtime_error_from([] { helios::start_runtime_job(valid_job(), 9); });
}

void completion_time_equals_start_time_plus_duration() {
  const auto running_job = helios::start_runtime_job(valid_job(), 12);

  assert(running_job.completion_time == running_job.start_time + running_job.job.duration);
}

void completing_running_job_creates_completed_job() {
  const auto running_job = helios::start_runtime_job(valid_job(), 10);
  const auto completed_job = helios::complete_runtime_job(running_job);

  assert(completed_job.job == running_job.job);
  assert(completed_job.start_time == running_job.start_time);
  assert(completed_job.completion_time == running_job.completion_time);
}

void equal_running_jobs_compare_equal() {
  assert(valid_running_job() == valid_running_job());
}

void equal_completed_jobs_compare_equal() {
  assert(valid_completed_job() == valid_completed_job());
}

void validate_running_job_rejects_invalid_embedded_job() {
  auto running_job = valid_running_job();
  running_job.job.duration = 0;

  expect_invalid_argument_from([&] { helios::validate_running_job(running_job); });
}

void validate_running_job_rejects_wrong_completion_time() {
  auto running_job = valid_running_job();
  running_job.completion_time = 16;

  expect_invalid_argument_from([&] { helios::validate_running_job(running_job); });
}

void validate_running_job_rejects_start_before_arrival() {
  auto running_job = valid_running_job();
  running_job.start_time = 9;
  running_job.completion_time = 14;

  expect_invalid_argument_from([&] { helios::validate_running_job(running_job); });
}

void validate_completed_job_rejects_invalid_embedded_job() {
  auto completed_job = valid_completed_job();
  completed_job.job.request.gpu_count = 0;

  expect_invalid_argument_from([&] { helios::validate_completed_job(completed_job); });
}

void validate_completed_job_rejects_wrong_completion_time() {
  auto completed_job = valid_completed_job();
  completed_job.completion_time = 16;

  expect_invalid_argument_from([&] { helios::validate_completed_job(completed_job); });
}

void validate_completed_job_rejects_start_before_arrival() {
  auto completed_job = valid_completed_job();
  completed_job.start_time = 9;
  completed_job.completion_time = 14;

  expect_invalid_argument_from([&] { helios::validate_completed_job(completed_job); });
}

} // namespace

int main() {
  starting_valid_job_creates_running_job();
  starting_at_arrival_time_is_allowed();
  starting_after_arrival_time_is_allowed();
  starting_before_arrival_time_is_rejected();
  completion_time_equals_start_time_plus_duration();
  completing_running_job_creates_completed_job();
  equal_running_jobs_compare_equal();
  equal_completed_jobs_compare_equal();
  validate_running_job_rejects_invalid_embedded_job();
  validate_running_job_rejects_wrong_completion_time();
  validate_running_job_rejects_start_before_arrival();
  validate_completed_job_rejects_invalid_embedded_job();
  validate_completed_job_rejects_wrong_completion_time();
  validate_completed_job_rejects_start_before_arrival();

  return 0;
}
