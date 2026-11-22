#include "helios/schedulers/easy_backfill_scheduler.hpp"

#include "policy_helpers.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace helios {

ScheduleDecision easy_backfill_schedule(const Observation& observation) {
  if (observation.pending_jobs.empty()) {
    return no_schedule_decision();
  }
  auto queue = observation.pending_jobs;
  for (const auto& job : queue) {
    detail::require_estimate(job);
  }
  if (!std::is_sorted(queue.begin(), queue.end(), detail::arrival_less)) {
    std::sort(queue.begin(), queue.end(), detail::arrival_less);
  }
  const auto& head = queue.front();
  if (fits(observation.available_resources, head.request)) {
    return select_job(head.id);
  }

  struct Release {
    std::int64_t time;
    JobId id;
    Resources request;
  };
  std::vector<Release> releases;
  for (const auto& running : observation.running_jobs) {
    const auto estimated_end =
        static_cast<std::int64_t>(running.start_time) + detail::require_estimate(running.job);
    // An overrun is still running: do not promise its resources at the current time.
    const auto release_time =
        std::max(estimated_end, static_cast<std::int64_t>(observation.current_time) + 1);
    releases.push_back({release_time, running.job.id, running.job.request});
  }
  std::sort(releases.begin(), releases.end(), [](const Release& lhs, const Release& rhs) {
    return std::tie(lhs.time, lhs.id) < std::tie(rhs.time, rhs.id);
  });
  auto projected = observation.available_resources;
  for (std::size_t i = 0; i < releases.size();) {
    const auto reservation = releases[i].time;
    do {
      projected = release(projected, observation.total_resources, releases[i].request);
      ++i;
    } while (i < releases.size() && releases[i].time == reservation);
    if (!fits(projected, head.request)) {
      continue;
    }
    for (std::size_t candidate = 1; candidate < queue.size(); ++candidate) {
      const auto& job = queue[candidate];
      if (!fits(observation.available_resources, job.request)) {
        continue;
      }
      const auto end =
          static_cast<std::int64_t>(observation.current_time) + *job.estimated_duration;
      if (end <= reservation || fits(allocate(projected, job.request), head.request)) {
        return select_job(job.id);
      }
    }
    return no_schedule_decision();
  }
  return no_schedule_decision();
}

} // namespace helios
