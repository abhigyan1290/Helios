#include "helios/workload.hpp"

#include <cassert>
#include <stdexcept>

namespace {

helios::Job make_job(helios::JobId id, helios::SimTime arrival_time = 0) {
  return helios::Job{
      .id = id,
      .arrival_time = arrival_time,
      .duration = 5,
      .request =
          helios::Resources{
              .gpu_count = 1,
              .cpu_count = 4,
              .memory_mb = 8192,
          },
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

void empty_workload_is_valid() {
  helios::validate_workload(helios::Workload{});
}

void valid_workload_is_accepted() {
  const auto workload = helios::make_workload({
      make_job(1, 0),
      make_job(2, 5),
  });

  helios::validate_workload(workload);
}

void invalid_job_is_rejected() {
  auto job = make_job(1);
  job.duration = 0;

  expect_invalid_argument_from([&] { helios::make_workload({job}); });
}

void duplicate_job_ids_are_rejected() {
  expect_invalid_argument_from([&] {
    helios::make_workload({
        make_job(1, 0),
        make_job(1, 5),
    });
  });
}

void workload_jobs_are_ordered_by_arrival_then_id() {
  const auto workload = helios::make_workload({
      make_job(3, 10),
      make_job(2, 5),
      make_job(1, 5),
  });

  assert(workload.jobs.size() == 3);
  assert(workload.jobs[0].id == 1);
  assert(workload.jobs[1].id == 2);
  assert(workload.jobs[2].id == 3);
}

void make_workload_preserves_empty_workload() {
  const auto workload = helios::make_workload({});

  assert(workload.jobs.empty());
}

} // namespace

int main() {
  empty_workload_is_valid();
  valid_workload_is_accepted();
  invalid_job_is_rejected();
  duplicate_job_ids_are_rejected();
  workload_jobs_are_ordered_by_arrival_then_id();
  make_workload_preserves_empty_workload();

  return 0;
}
