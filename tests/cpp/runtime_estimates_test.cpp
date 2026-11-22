#include "helios/job_state.hpp"
#include "helios/runtime_estimates.hpp"

#include <cassert>
#include <limits>
#include <stdexcept>

namespace {

template <class Error, class Function> void rejects(Function function) {
  try {
    function();
  } catch (const Error&) {
    return;
  }
  assert(false && "expected exception");
}

} // namespace

int main() {
  const auto workload = helios::make_workload({helios::Job{1, 0, 3, {1, 0, 0}}});
  assert(!workload.jobs[0].estimated_duration);
  for (const auto [percentage, expected] : {std::pair{50, 2}, {100, 3}, {150, 5}}) {
    const auto estimated = helios::with_runtime_estimates(workload, percentage);
    assert(estimated.jobs[0].estimated_duration == expected);
    assert(estimated.jobs[0].duration == 3);
    assert(helios::start_runtime_job(estimated.jobs[0], 0).completion_time == 3);
  }
  assert(!workload.jobs[0].estimated_duration);
  assert(helios::with_runtime_estimates(helios::make_workload({}), 100).jobs.empty());
  rejects<std::invalid_argument>([&] { helios::with_runtime_estimates(workload, 0); });
  rejects<std::invalid_argument>([&] { helios::with_runtime_estimates(workload, -10); });
  auto invalid = workload.jobs[0];
  invalid.estimated_duration = 0;
  rejects<std::invalid_argument>([&] { helios::validate_job(invalid); });
  invalid.estimated_duration = -1;
  rejects<std::invalid_argument>([&] { helios::validate_job(invalid); });
  auto huge = workload.jobs[0];
  huge.duration = std::numeric_limits<helios::Duration>::max();
  rejects<std::invalid_argument>(
      [&] { helios::with_runtime_estimates(helios::make_workload({huge}), 200); });
  rejects<std::runtime_error>([&] { helios::start_runtime_job(huge, 1); });
  rejects<std::invalid_argument>(
      [&] { helios::validate_running_job(helios::RunningJob{huge, 1, 1}); });
}
