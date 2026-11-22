#pragma once

#include "helios/metrics.hpp"
#include "helios/observation.hpp"
#include "helios/schedule_decision.hpp"
#include "helios/workload.hpp"

namespace helios {

struct StepResult {
  SimTime elapsed_time;
  std::optional<JobId> started_job_id;
  std::vector<JobId> completed_job_ids;
  bool terminated;

  bool operator==(const StepResult&) const = default;
};

class Simulator {
public:
  Simulator(const Workload& workload, Resources total_resources);
  void reset(const Workload& workload, Resources total_resources);
  Observation observe() const;
  StepResult step(const ScheduleDecision& decision);
  bool done() const noexcept;
  SimulationResult result() const;
  void validate() const;

  // Explicit oracle/diagnostic access for legacy baselines, not external policies.
  Observation observe_oracle() const;
  const SimulatorState& diagnostic_state() const noexcept;

private:
  SimulatorState state_;
  Observation observation(bool oracle) const;
};

} // namespace helios
