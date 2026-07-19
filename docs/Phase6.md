# Phase 6 Plan

## Purpose

Phase 6 adds Helios's second classical scheduler baseline: SJF, or shortest job
first.

The goal is not to add reinforcement learning yet. The goal is to make scheduler
comparison real:

```text
same workload
same cluster resources
FIFO result
SJF result
CSV metrics for both
```

By the end of this phase, Helios should be able to run FIFO and SJF on the same
named demo workloads and compare their per-job metrics.

## What Is Already Done

### Milestone 1: Project Scaffold

Completed:

1. C++20 CMake project scaffold.
2. `helios_core` library.
3. Placeholder CLI evolved into a demo benchmark CLI.
4. CTest and CI.

### Phase 2: Model Primitives

Completed:

1. `Resources`.
2. `Job`.
3. Validation.
4. Resource fit, allocation, and release helpers.

### Phase 3: Runtime Simulator Core

Completed:

1. Runtime job state.
2. Event model.
3. Simulator state.
4. Explicit lifecycle transitions.

### Phase 4: First Scheduler Baseline

Completed:

1. `ScheduleDecision`.
2. `apply_schedule_decision`.
3. Strict FIFO scheduler.

### Phase 5: Benchmarkable FIFO Runner

Completed:

1. `Workload`.
2. `JobMetrics`.
3. `SimulationResult`.
4. FIFO simulation runner.
5. CSV output.
6. Named demo workloads.
7. CLI demo commands.

## High-Level Goals

Phase 6 should add:

1. SJF scheduler.
2. Tests for SJF policy behavior.
3. A policy-agnostic shared scheduler runner so FIFO and SJF do not need
   separate full loops.
4. CLI support for SJF demo workloads.
5. A simple comparison path for FIFO vs SJF on the same workload.

Phase 6 should not add:

1. RL.
2. Python bindings.
3. Gymnasium.
4. PPO.
5. Random workload generation.
6. Plotting.
7. Dashboards.
8. Backfilling.
9. EDF or priority scheduling.

## Important Design Note: Known Duration

Current `Job::duration` means known simulated runtime.

For Phase 6, SJF will use:

```text
job.duration
```

as the sorting key. This makes Phase 6 an oracle-style SJF baseline where the
scheduler knows each job's runtime.

This is acceptable for an early classical baseline, but it is not fully
realistic. In real GPU clusters, users often provide estimates and actual runtime
can differ.

Future realism phase:

```cpp
estimated_duration
actual_duration
```

Schedulers would use `estimated_duration`; the simulator would complete jobs
using `actual_duration`.

Do not add that split in Phase 6. Document the limitation and keep the change
small.

## Important Design Note: Non-Preemptive SJF

Phase 6 SJF is non-preemptive.

Example:

```text
time 0: long job starts
time 1: short job arrives
```

SJF should not stop the long job. The short job waits until the running job
completes and resources are released.

Do not implement shortest-remaining-time-first in Phase 6. That is a different,
preemptive scheduler family.

## Branch Strategy

Start after Phase 5 merges:

```bash
git switch main
git pull
git switch -c feature/m6-sjf-baseline
```

Recommended split if this grows:

```text
PR 1: generic scheduler runner
PR 2: SJF scheduler
PR 3: CLI comparison commands
```

If keeping one PR, keep commits separated by those boundaries.

## Phase 6 Deliverables

By the end of Phase 6, the repository should contain:

1. `sjf_schedule`.
2. SJF policy tests.
3. Generic scheduler simulation runner.
4. FIFO rewritten to use the generic runner, or a wrapper that preserves
   `run_fifo_simulation`.
5. `run_sjf_simulation`.
6. CLI support for `--demo-sjf`.
7. Optional CLI support for comparing FIFO and SJF on one named workload.
8. Tests proving FIFO behavior did not regress.
9. Tests proving SJF and FIFO can produce different results on the same workload.

## Core Design Decisions

### Keep Scheduler Policies Pure

SJF should follow the same architecture as FIFO:

```cpp
ScheduleDecision sjf_schedule(const SimulatorState& state);
```

SJF should:

1. Inspect simulator state.
2. Return `ScheduleDecision`.
3. Not mutate simulator state.
4. Use `get_schedulable_jobs(state)`.
5. Let `apply_schedule_decision` execute the choice.

### SJF Policy Semantics

SJF chooses the schedulable fitting job with the shortest duration.

Tie-breakers:

```text
1. smaller duration
2. earlier arrival_time
3. smaller JobId
```

Resource behavior:

1. Consider only schedulable jobs.
2. Ignore jobs that do not fit currently available resources.
3. If no schedulable job fits, return no decision.

This differs from strict FIFO. FIFO waits when the head job does not fit. SJF is
allowed to choose the shortest fitting schedulable job.

### Generic Scheduler Runner

The current FIFO runner owns the full simulation loop. Before adding many more
schedulers, avoid duplicating that loop.

Recommended API:

```cpp
using SchedulerFn = std::function<ScheduleDecision(const SimulatorState&)>;

SimulationResult run_simulation(
    const Workload& workload,
    Resources total_resources,
    SchedulerFn scheduler
);

SimulationResult run_fifo_simulation(
    const Workload& workload,
    Resources total_resources
);

SimulationResult run_sjf_simulation(
    const Workload& workload,
    Resources total_resources
);
```

Why `std::function`:

1. It works for plain functions like `fifo_schedule` and `sjf_schedule`.
2. It will also work later for configured schedulers, seeded random schedulers,
   or policy objects.
3. It avoids changing the runner API as soon as schedulers become stateful.

`run_fifo_simulation` should become a small wrapper:

```cpp
return run_simulation(workload, total_resources, fifo_schedule);
```

`run_sjf_simulation` should be the same shape:

```cpp
return run_simulation(workload, total_resources, sjf_schedule);
```

This keeps the simulation loop shared and makes future schedulers easier to add.

If an empty `SchedulerFn` is passed to `run_simulation`, reject it with
`std::invalid_argument`.

### Runner Semantics Stay The Same

The generic runner should preserve Phase 5 behavior:

1. Validate workload and resources.
2. Reject jobs that can never fit total resources.
3. Add all jobs to simulator state.
4. Complete due jobs.
5. Repeatedly apply scheduler decisions while possible.
6. Advance time to the next useful event.
7. Throw if no progress is possible.
8. Return `SimulationResult`.

Do not duplicate resource accounting or manually mutate state.

The runner must not contain FIFO-specific behavior.

The runner should only ask:

```cpp
auto decision = scheduler(state);
```

Then it should apply that decision through `apply_schedule_decision`.

Do not bake in assumptions like:

1. the head pending job blocks the whole queue,
2. later schedulable jobs cannot be selected,
3. FIFO ordering determines progress.

Those are scheduler policy decisions. FIFO owns strict head-of-line blocking.
SJF owns shortest-fitting-job selection. The runner owns only lifecycle
execution.

## PR 1: Generic Scheduler Runner

### Goal

Make the simulation loop reusable across schedulers.

### Proposed Files

```text
include/helios/simulation_runner.hpp
src/simulation_runner.cpp
tests/cpp/simulation_runner_test.cpp
CMakeLists.txt
```

### Proposed Changes

Add:

```cpp
using SchedulerFn = std::function<ScheduleDecision(const SimulatorState&)>;

SimulationResult run_simulation(
    const Workload& workload,
    Resources total_resources,
    SchedulerFn scheduler
);
```

Keep:

```cpp
SimulationResult run_fifo_simulation(
    const Workload& workload,
    Resources total_resources
);
```

### Required Tests

1. `run_fifo_simulation` still passes all existing runner tests.
2. `run_simulation(..., fifo_schedule)` matches `run_fifo_simulation`.
3. Empty scheduler function is rejected with `std::invalid_argument`.
4. Runner still rejects unschedulable jobs.
5. Runner remains deterministic.
6. Generic runner contains no policy-specific behavior, proven by later SJF
   tests where SJF can skip a non-fitting schedulable job.

## PR 2: SJF Scheduler

### Goal

Add the second scheduler baseline.

### Proposed Files

```text
include/helios/schedulers/sjf_scheduler.hpp
src/schedulers/sjf_scheduler.cpp
tests/cpp/sjf_scheduler_test.cpp
CMakeLists.txt
```

### Proposed Function

```cpp
ScheduleDecision sjf_schedule(const SimulatorState& state);
```

### Required Policy Tests

1. Empty simulator returns no decision.
2. Future jobs return no decision.
3. Single arrived fitting job is selected.
4. Single arrived non-fitting job returns no decision.
5. SJF selects shorter duration over earlier arrival only when both jobs are
   already schedulable.
6. SJF does not start future short jobs before arrival.
7. Equal duration tie-breaks by earlier arrival.
8. Equal duration and arrival tie-breaks by smaller JobId.
9. SJF skips non-fitting jobs and selects the shortest fitting schedulable job.
10. SJF returns no decision if schedulable jobs exist but none currently fit.
11. SJF does not mutate state.
12. Invalid simulator state is rejected.

### Required Runner Tests

1. `run_sjf_simulation` completes an empty workload.
2. `run_sjf_simulation` completes a single job.
3. SJF starts shortest schedulable job first.
4. FIFO and SJF produce identical results on a workload where jobs arrive one at
   a time.
5. FIFO and SJF produce different wait times on a workload with one long job and
   multiple short jobs available at the same time.
6. SJF runner is deterministic.

## PR 3: CLI Comparison

### Goal

Let users run SJF demos and compare FIFO vs SJF on the same named workload.

### Proposed Files

```text
apps/helios_sim/main.cpp
tests/cpp or script-level CLI tests if added later
README.md
```

### CLI Commands

Keep existing:

```bash
./build/helios_sim --list-demo-workloads
./build/helios_sim --demo-fifo fifo-blocking
```

Add:

```bash
./build/helios_sim --demo-sjf fifo-blocking
```

Optional comparison command:

```bash
./build/helios_sim --compare fifo-blocking
```

Recommended output for `--compare` can stay simple:

```text
scheduler,makespan,total_wait_time,average_wait_time
fifo,...
sjf,...
```

Only add comparison output if it stays small. If it starts pulling in more
aggregate metrics, split it into its own phase.

Keep CLI comparison intentionally tiny. It should answer only:

```text
How did FIFO and SJF compare on this named workload?
```

Do not turn Phase 6 into a full benchmark framework.

## Metrics For Phase 6

Existing per-job metrics are enough:

1. wait time.
2. turnaround time.
3. makespan.

Optional simple aggregate helpers:

```cpp
SimTime total_wait_time(const SimulationResult& result);
double average_wait_time(const SimulationResult& result);
```

Add these only if needed for CLI comparison. If added, test them.

Do not add GPU utilization yet.

## Testing Philosophy

Phase 6 tests should prove scheduler differences.

Good tests answer:

1. Does SJF choose the shortest schedulable job?
2. Are tie-breakers deterministic?
3. Does SJF ignore future jobs?
4. Does SJF avoid mutating simulator state?
5. Does SJF integrate with the same runner as FIFO?
6. Can FIFO and SJF be compared on the same workload?
7. Can FIFO and SJF produce identical results on simple workloads?
8. Can FIFO and SJF produce different results on workloads designed to expose
   the policy difference?

Avoid tests that require random workloads, Python, RL, or plotting.

## Reliability Risks And Mitigations

### Risk: Duplicating The Runner Loop

Mitigation:

1. Add `run_simulation`.
2. Keep FIFO and SJF wrappers tiny.
3. Test FIFO wrapper still matches generic runner.

### Risk: Runner Preserves FIFO-Specific Logic

Mitigation:

1. Keep strict FIFO behavior inside `fifo_schedule`.
2. Keep shortest-job behavior inside `sjf_schedule`.
3. Make `run_simulation` call the scheduler and apply its decision without
   inspecting scheduler policy details.
4. Test SJF cases that would fail if the runner enforced FIFO head-of-line
   blocking.

### Risk: SJF Uses Future Jobs

Mitigation:

1. Use `get_schedulable_jobs`.
2. Test future jobs are ignored.

### Risk: SJF Becomes Non-Deterministic

Mitigation:

1. Define tie-breakers.
2. Test equal duration and equal arrival cases.

### Risk: Runtime Estimate Confusion

Mitigation:

1. Document that Phase 6 SJF uses known `Job::duration`.
2. Defer estimated vs actual runtime split.
3. Do not claim this is production-realistic SJF.

### Risk: SJF Is Confused With Preemptive Scheduling

Mitigation:

1. Document Phase 6 SJF as non-preemptive.
2. Do not interrupt running jobs when shorter jobs arrive.
3. Keep preemptive shortest-remaining-time-first out of Phase 6.

## Definition Of Done

Phase 6 is complete when:

1. Generic scheduler runner exists and is tested.
2. FIFO runner behavior is preserved.
3. SJF scheduler exists and is tested.
4. SJF runner exists and is tested.
5. SJF tie-breakers are deterministic.
6. SJF is documented and tested as non-preemptive.
7. SJF and FIFO can run on the same named workloads.
8. FIFO and SJF are tested on workloads where they match and workloads where
   they differ.
9. CLI can run SJF demos, if included.
10. README or docs mention that SJF currently uses known simulated durations.
11. CMake builds all new targets.
12. CTest passes locally.
13. Formatting passes locally and in CI.
14. No Python bindings, RL, random workload generation, plotting, or dashboards
    have been introduced.

## Suggested First Implementation Step

Start by refactoring the runner.

Implement only:

```text
run_simulation(workload, resources, scheduler)
run_fifo_simulation as a wrapper
tests proving behavior is unchanged
```

Do not add SJF until the shared runner is tested.

## Suggested Phase 7

After Phase 6, add deterministic synthetic workload generation.

Likely Phase 7:

1. Workload generator config.
2. Seeded deterministic generation.
3. Small, medium, and large generated workloads.
4. FIFO vs SJF comparison across generated workloads.
5. CSV outputs saved under `benchmarks/`.

Python bindings should still wait until C++ benchmark comparison is stable.
