# Phase 5 Plan

## Purpose

Phase 5 should make Helios capable of running a complete deterministic
scheduling experiment with the strict FIFO baseline and saving results as CSV.

The goal is not to add reinforcement learning yet. The goal is to connect the
pieces we already have:

```text
workload jobs
simulator state
FIFO scheduler
explicit simulator transitions
metrics
CSV output
```

By the end of this phase, Helios should be able to run a small synthetic
workload through FIFO and produce benchmark rows that can be compared later
against SJF, EDF, backfilling, and RL.

## What Is Already Done

### Milestone 1: Project Scaffold

Completed:

1. C++20 CMake project scaffold.
2. `helios_core` library.
3. Placeholder `helios_sim` CLI.
4. CTest smoke test.
5. Formatting scripts and GitHub Actions CI.

### Phase 2: Model Primitives

Completed:

1. `Resources`.
2. `Job`.
3. Resource validation.
4. Job validation.
5. Resource fit, allocation, and release helpers.
6. C++ tests for model invariants.

### Phase 3: Runtime Simulator Core

Completed:

1. `RunningJob`.
2. `CompletedJob`.
3. `Event`.
4. `SimulatorState`.
5. `advance_time`.
6. `get_schedulable_jobs`.
7. Explicit start and complete transitions.
8. C++ tests for lifecycle behavior.

### Phase 4: First Scheduler Baseline

Completed or in review:

1. `ScheduleDecision`.
2. `apply_schedule_decision`.
3. Strict `fifo_schedule`.
4. Scheduler tests.
5. FIFO policy tests.

Important design decisions already made:

1. Schedulers return decisions and do not mutate state.
2. Simulator transitions execute decisions.
3. FIFO is strict and does not skip an unfitting head job.
4. Future jobs are hidden from schedulers through `get_schedulable_jobs`.

## High-Level Goals

Phase 5 should add:

1. A deterministic workload representation for benchmark inputs.
2. A small workload validation layer.
3. A simulation runner that repeatedly applies a scheduler until all jobs finish.
4. Basic documented metrics.
5. CSV output for completed benchmark runs.
6. Tests for deterministic workload execution and metrics.

Phase 5 should not add:

1. Python bindings.
2. Gymnasium.
3. PPO.
4. Reward functions.
5. RL observations/actions.
6. Plotting.
7. Dashboards.
8. Large workload generators.
9. Advanced schedulers beyond FIFO unless Phase 5 is later split.

## Branch Strategy

Start after Phase 4 merges:

```bash
git switch main
git pull
git switch -c feature/m5-fifo-benchmark-runner
```

If the work grows, split into:

```bash
feature/m5-workload-model
feature/m5-simulation-runner
feature/m5-csv-output
```

Recommended split:

1. PR 1: workload and metrics types.
2. PR 2: FIFO simulation runner.
3. PR 3: CSV output and CLI integration.

## Phase 5 Deliverables

By the end of Phase 5, the repository should contain:

1. A `Workload` value type.
2. Workload validation tests.
3. A `SimulationResult` or `BenchmarkResult` type.
4. Documented metrics.
5. A FIFO simulation runner.
6. CSV serialization for benchmark rows.
7. A basic CLI path to run a small built-in FIFO benchmark.
8. Tests proving deterministic benchmark output.

## Core Design Decisions

### Keep Workloads Simple

Use a simple workload wrapper around jobs:

```cpp
struct Workload {
  std::vector<Job> jobs;

  bool operator==(const Workload&) const = default;
};
```

Workload semantics:

1. Jobs are validated with `validate_job`.
2. Job IDs must be unique.
3. Workload order should be deterministic by `arrival_time`, then `id`.
4. Empty workloads are valid.

Do not add random generation in the first PR. Random synthetic workload
generation can come later after deterministic execution is tested.

### Add Metrics Before More Schedulers

Metrics should be boring, explicit, and documented.

Recommended first metrics:

```cpp
struct JobMetrics {
  JobId job_id;
  SimTime arrival_time;
  SimTime start_time;
  SimTime completion_time;
  Duration duration;
  SimTime wait_time;
  SimTime turnaround_time;
};

struct SimulationResult {
  std::vector<JobMetrics> jobs;
  SimTime makespan;
};
```

Definitions:

```text
wait_time = start_time - arrival_time
turnaround_time = completion_time - arrival_time
makespan = max(completion_time), or 0 for an empty workload
```

For Phase 5, `makespan` means absolute simulation end time. Example: if the
first job arrives at time 100 and completes at time 110, `makespan` is 110, not
10. A later phase may add `total_elapsed_time = max(completion_time) -
min(arrival_time)`.

Do not add utilization, slowdown, reward, fairness, or cost metrics yet unless
they are documented in the same PR.

Future metric note: GPU utilization is important, but it requires interval math
over time:

```text
sum(used_gpus * interval_duration) / (total_gpus * total_time)
```

Add it later after the runner is stable and the definition is documented.

### Runner Owns The Loop

The simulator state currently supports explicit transitions. Phase 5 should add
a runner that automates those transitions for a whole workload.

For FIFO:

```cpp
SimulationResult run_fifo_simulation(
    const Workload& workload,
    Resources total_resources
);
```

The runner may use:

1. `create_simulator_state`.
2. `add_pending_job`.
3. `advance_time`.
4. `fifo_schedule`.
5. `apply_schedule_decision`.
6. `complete_running_job`.

The runner should not duplicate resource accounting. It should use existing
simulator transitions.

### Deterministic Time Advancement

Use event-style deterministic stepping:

```cpp
while (!state.pending_jobs.empty() || !state.running_jobs.empty()) {
  while (exists running job with completion_time <= state.current_time) {
    state = complete_running_job(state, job_id);
  }

  bool started_any = false;
  while (true) {
    const auto decision = fifo_schedule(state);
    if (!decision.job_id.has_value()) {
      break;
    }

    state = apply_schedule_decision(state, decision);
    started_any = true;
  }

  if (state.pending_jobs.empty() && state.running_jobs.empty()) {
    break;
  }

  if (started_any) {
    continue;
  }

  const auto next_time = next_relevant_future_time(state);
  if (!next_time.has_value()) {
    throw std::runtime_error("simulation cannot make progress");
  }

  state = advance_time(state, *next_time);
}
```

Loop rules:

1. Add all workload jobs to simulator state as pending jobs before the loop.
2. Complete all running jobs due at `current_time`, ordered by completion time
   then job ID.
3. Repeatedly apply FIFO while it returns a decision.
4. If jobs started, loop again at the same time before advancing.
5. If nothing can start, advance to the next useful future time.
6. Stop when pending and running jobs are both empty.

Important: if strict FIFO is blocked by an unfitting head job and no running job
can complete, the runner must detect that the workload cannot finish.

### Next Relevant Future Time

The runner should choose time advancement carefully.

`next_relevant_future_time(state)` considers:

1. The next running job completion time greater than `current_time`.
2. The next pending job arrival time greater than `current_time`.

However, strict FIFO has one important wrinkle:

```text
current_time = 0
available = 0 GPUs
running job completes at time 10

pending:
Job B arrived at time 0, needs 4 GPUs
Job C arrives at time 5, needs 1 GPU
```

FIFO is blocked by Job B. Advancing to Job C's arrival at time 5 does not help
because FIFO cannot skip Job B. The next useful time is the running completion at
time 10.

Required behavior:

1. If the head schedulable job has arrived and does not fit because resources are
   currently busy, prefer the next running completion over unrelated future
   arrivals.
2. If no job has arrived yet, advance to the next pending arrival.
3. If there are running jobs but no schedulable job can start, advance to the
   next running completion.
4. If no future arrival or completion exists, throw `std::runtime_error`.

### Handle Unschedulable Jobs Explicitly

A job may request more resources than the cluster has.

Recommended behavior:

1. Detect unschedulable jobs before the run starts.
2. Throw `std::runtime_error` if any job request cannot fit total resources.

Reason:

```text
If a job can never fit, the simulation loop would otherwise wait forever.
```

Test this for each resource dimension:

1. Job GPU request exceeds total GPUs.
2. Job CPU request exceeds total CPUs.
3. Job memory request exceeds total memory.

### CSV Output Is A Serialization Layer

CSV output should serialize `SimulationResult`. It should not run simulation.

Recommended function:

```cpp
std::string simulation_result_to_csv(const SimulationResult& result);
```

Recommended columns:

```text
job_id,arrival_time,start_time,completion_time,duration,wait_time,turnaround_time
```

Keep aggregate metrics out of the first CSV unless needed. If adding makespan,
either add a separate summary row format or a separate summary CSV later.

CSV newline convention:

1. Include a header row.
2. End the header with a newline.
3. End every data row with a newline.
4. Empty results emit the header plus one trailing newline.

This makes exact CSV tests stable.

## PR 1: Workload And Metrics Types

### Goal

Create deterministic workload and metric value types.

### Proposed Files

```text
include/helios/workload.hpp
src/workload.cpp
include/helios/metrics.hpp
src/metrics.cpp
tests/cpp/workload_test.cpp
tests/cpp/metrics_test.cpp
CMakeLists.txt
```

### Proposed Functions

```cpp
void validate_workload(const Workload& workload);
Workload make_workload(std::vector<Job> jobs);

JobMetrics make_job_metrics(const CompletedJob& completed_job);
SimulationResult make_simulation_result(std::vector<CompletedJob> completed_jobs);
```

### Required Tests

1. Empty workload is valid.
2. Valid workload is accepted.
3. Invalid job is rejected.
4. Duplicate job IDs are rejected.
5. Workload jobs are ordered by `arrival_time`, then `id`.
6. Job metrics compute wait time.
7. Job metrics compute turnaround time.
8. Simulation result computes makespan.
9. Empty simulation result has makespan zero.
10. Makespan is documented as absolute simulation end time.
11. Two jobs completing at the same time are ordered by completion time, then job
    ID in metrics.

## PR 2: FIFO Simulation Runner

### Goal

Run a full deterministic workload through strict FIFO.

### Proposed Files

```text
include/helios/simulation_runner.hpp
src/simulation_runner.cpp
tests/cpp/simulation_runner_test.cpp
CMakeLists.txt
```

### Proposed Function

```cpp
SimulationResult run_fifo_simulation(
    const Workload& workload,
    Resources total_resources
);
```

### Required Tests

1. Empty workload completes with no job metrics and makespan zero.
2. Single job starts at arrival time when resources are available.
3. Single job completes at `arrival_time + duration`.
4. Two jobs that both fit can run concurrently if FIFO repeatedly starts them.
5. Strict FIFO blocks later jobs behind an unfitting head job.
6. If strict FIFO is blocked by an arrived head job, the runner advances to the
   next running completion instead of an irrelevant future arrival.
7. Unschedulable job with GPU request larger than total GPUs is rejected.
8. Unschedulable job with CPU request larger than total CPUs is rejected.
9. Unschedulable job with memory request larger than total memory is rejected.
10. Jobs arriving later are not started before arrival.
11. A workload whose first job arrives after time zero starts that job at its
    arrival time.
12. Multiple jobs arriving at the same time are started in JobId order under
    FIFO.
13. Runner throws if no possible future progress exists.
14. Runner is deterministic when called twice on the same workload.
15. Completed job metrics are ordered deterministically by completion time, then
   job ID.

Concurrency test guidance:

```text
Cluster: 4 GPUs
Job 1: arrival 0, duration 10, request 2 GPUs
Job 2: arrival 0, duration 10, request 2 GPUs
```

Both jobs should start at time 0 and complete at time 10.

Separate blocking test:

```text
Cluster: 4 GPUs
Job 1: arrival 0, duration 10, request 4 GPUs
Job 2: arrival 0, duration 10, request 1 GPU
```

Job 2 should wait while Job 1 uses the full cluster.

## PR 3: CSV Output And CLI Hook

### Goal

Save benchmark results in a simple machine-readable format and make the CLI do
something useful.

### Proposed Files

```text
include/helios/csv.hpp
src/csv.cpp
tests/cpp/csv_test.cpp
apps/helios_sim/main.cpp
CMakeLists.txt
```

### Proposed Functions

```cpp
std::string simulation_result_to_csv(const SimulationResult& result);
```

### CLI Scope

Keep the CLI tiny:

```bash
./build/helios_sim --demo-fifo
```

Recommended behavior:

1. Build a tiny deterministic workload in C++.
2. Run FIFO simulation.
3. Print CSV to stdout.

Do not add command-line file parsing yet unless this phase stays comfortably
small.

### Required Tests

1. CSV includes a header.
2. CSV includes one row per job metric.
3. CSV output is deterministic.
4. Empty result emits only the header plus one trailing newline.
5. Values are written in the documented column order.
6. CSV output consistently ends with a trailing newline.

## Exception Rules

Use the existing policy:

```text
std::invalid_argument = malformed input or invalid state
std::runtime_error = valid input that cannot be completed
```

Examples:

1. Invalid job in workload: `std::invalid_argument`.
2. Duplicate job IDs in workload: `std::invalid_argument`.
3. Job request larger than total cluster resources: `std::runtime_error`.
4. Runner detects no possible future progress: `std::runtime_error`.

## Testing Philosophy

Phase 5 tests should prove full-run behavior.

Good tests answer:

1. Can an empty workload run?
2. Can a single job run to completion?
3. Can multiple jobs run deterministically?
4. Does strict FIFO behavior survive inside the full runner?
5. Are metrics computed from completed jobs correctly?
6. Is CSV output stable?
7. Does strict FIFO blocking advance time to useful completions rather than
   irrelevant arrivals?

Avoid tests that require Python, RL, plotting, or large benchmark suites.

## CMake Integration

Expected new tests by the end of Phase 5:

```text
helios_workload_test
helios_metrics_test
helios_simulation_runner_test
helios_csv_test
```

Existing tests should keep passing:

```text
helios_smoke_test
helios_job_test
helios_resources_test
helios_job_state_test
helios_event_test
helios_simulator_state_test
helios_schedule_decision_test
helios_fifo_scheduler_test
```

## Local Verification

Run:

```bash
find include src apps tests \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) -print0 \
  | xargs -0 clang-format --dry-run --Werror
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Also run the demo CLI once PR 3 exists:

```bash
./build/helios_sim --demo-fifo
```

## Reliability Risks And Mitigations

### Risk: Runner Duplicates Simulator Logic

Mitigation:

1. Use `advance_time`, `complete_running_job`, `fifo_schedule`, and
   `apply_schedule_decision`.
2. Do not manually mutate resources in the runner.

### Risk: Infinite Loop On Blocked Workload

Mitigation:

1. Reject jobs that can never fit total resources before running.
2. Detect when no pending arrival and no running completion can make progress.
3. Throw `std::runtime_error` instead of looping forever.
4. When the arrived FIFO head is blocked by busy resources, advance to the next
   running completion rather than an unrelated future arrival.

### Risk: Metrics Become Undocumented

Mitigation:

1. Define every metric in `docs/`.
2. Keep Phase 5 metrics minimal.
3. Add tests for each metric formula.

### Risk: CSV Format Changes Accidentally

Mitigation:

1. Test exact header.
2. Test exact output for a small deterministic result.
3. Standardize on a trailing newline after the header and each row.

## Definition Of Done

Phase 5 is complete when:

1. `Workload` exists and is tested.
2. Workload validation rejects invalid jobs and duplicate IDs.
3. Job metrics exist and are tested.
4. `SimulationResult` exists and is tested.
5. `run_fifo_simulation` runs empty, single-job, and multi-job workloads.
6. Runner rejects unschedulable jobs.
7. Runner is deterministic.
8. CSV output exists and is tested.
9. CLI can print a tiny FIFO demo CSV.
10. CMake builds all new targets.
11. CTest passes locally.
12. Formatting passes locally and in CI.
13. No Python bindings, Gymnasium environment, PPO, or plotting code has been
    introduced.

## Proposed Commit Sequence

PR 1:

1. `model: add workload type`
2. `model: add simulation metrics`
3. `test: cover workload and metrics`

PR 2:

4. `runner: add FIFO simulation runner`
5. `test: cover FIFO simulation runner`

PR 3:

6. `io: add simulation CSV output`
7. `app: add FIFO demo CLI`
8. `test: cover CSV output`
9. `docs: document phase 5 benchmark runner scope`

## Suggested First Implementation Step

Start with workload and metrics.

Implement only:

```text
include/helios/workload.hpp
src/workload.cpp
include/helios/metrics.hpp
src/metrics.cpp
tests/cpp/workload_test.cpp
tests/cpp/metrics_test.cpp
CMakeLists.txt
```

First coding goal:

1. Add `Workload`.
2. Add `validate_workload`.
3. Add `make_workload`.
4. Add `JobMetrics`.
5. Add `SimulationResult`.
6. Add metric constructors.
7. Test deterministic ordering and metric formulas.

Do not add the runner until workload and metrics are tested.

## Suggested Phase 6

After Phase 5, add the next scheduler baseline.

Likely Phase 6:

1. SJF scheduler.
2. SJF full-run benchmark.
3. Compare FIFO and SJF CSV outputs.
4. Start a small benchmark harness that can run multiple schedulers.

Python bindings should still wait until the C++ benchmark loop is stable.
