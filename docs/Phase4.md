# Phase 4 Plan

## Purpose

Phase 4 introduces the first scheduler decision contract and the first baseline
scheduler: strict FIFO.

The goal is to answer one narrow question:

```text
Given the current simulator state, which schedulable job should start next?
```

This phase should not build RL, rewards, Python bindings, benchmark runners,
plots, dashboards, or advanced scheduling policies. It should create a small,
tested scheduling layer that future baselines and reinforcement learning can
compare against.

## What Is Already Done

### Milestone 1: Project Scaffold

Completed:

1. C++20 CMake project scaffold.
2. `helios_core` library.
3. Placeholder `helios_sim` CLI.
4. CTest smoke test.
5. Formatting scripts and `.clang-format`.
6. GitHub Actions CI for formatting, build, and tests.

### Phase 2: Model Primitives

Completed:

1. `Resources` and resource validation.
2. Resource comparison, fit, allocation, and release helpers.
3. `Job` and job validation.
4. C++ tests for job and resource invariants.

### Phase 3: Runtime Simulator Core

Completed or in review:

1. `RunningJob`.
2. `CompletedJob`.
3. Runtime job validation.
4. Tiny `Event` model.
5. `SimulatorState`.
6. Explicit time advancement.
7. `get_schedulable_jobs`.
8. Explicit start and complete transitions.
9. Tests for lifecycle, resource accounting, invalid transitions, and ordering.

Important design decisions already made:

1. `Job` stays an input description.
2. Runtime state lives outside `Job`.
3. Simulator transitions are explicit.
4. The simulator does not auto-select jobs.
5. Schedulers should use `get_schedulable_jobs`, not raw `pending_jobs`.

## High-Level Goals

Phase 4 should add:

1. A small scheduler decision type.
2. A helper that applies a scheduler decision to simulator state.
3. A strict FIFO scheduler.
4. Tests for empty queues, future jobs, unschedulable jobs, resource constraints,
   deterministic selection, and non-mutation.

Phase 4 should not add:

1. SJF.
2. EDF.
3. Priority scheduling.
4. Backfilling.
5. Random scheduling.
6. RL action spaces.
7. Reward functions.
8. Python bindings.
9. Benchmark CSV output.
10. Plots or dashboards.

## Branch Strategy

Start after the Phase 3 PR merges:

```bash
git switch main
git pull
```

Split Phase 4 into two PRs.

PR 1:

```bash
git switch -c feature/m4-scheduler-contract
```

PR 2, after PR 1 merges:

```bash
git switch main
git pull
git switch -c feature/m4-fifo-scheduler
```

Why split it:

1. The scheduler decision contract deserves focused tests.
2. FIFO is a policy layered on top of that contract.
3. Smaller PRs make review and GitHub history cleaner.

## Phase 4 Deliverables

By the end of Phase 4, the repository should contain:

1. `ScheduleDecision`.
2. `no_schedule_decision`.
3. `select_job`.
4. `apply_schedule_decision`.
5. `fifo_schedule`.
6. C++ tests for scheduler decisions.
7. C++ tests for strict FIFO behavior.
8. C++ tests for scheduler-driven simulator transitions.
9. CMake integration for scheduler tests.

## Core Design Decisions

### Split Decision Contract From Policies

Do not put every future scheduler into one `scheduler.hpp` file.

Use:

```text
include/helios/schedule_decision.hpp
include/helios/schedulers/fifo_scheduler.hpp
src/schedule_decision.cpp
src/schedulers/fifo_scheduler.cpp
tests/cpp/schedule_decision_test.cpp
tests/cpp/fifo_scheduler_test.cpp
```

This keeps the policy-neutral decision API separate from FIFO. Later schedulers
can live beside FIFO:

```text
include/helios/schedulers/sjf_scheduler.hpp
include/helios/schedulers/edf_scheduler.hpp
include/helios/schedulers/random_scheduler.hpp
```

### Scheduler Does Not Mutate State

Schedulers inspect `SimulatorState` and return a decision. They do not mutate
state directly.

Decision shape:

```cpp
struct ScheduleDecision {
  std::optional<JobId> job_id;

  bool operator==(const ScheduleDecision&) const = default;
};
```

Meaning:

1. `job_id.has_value()` means the scheduler selected one job to start.
2. `std::nullopt` means no job should start now.

Phase 4 supports single-job start decisions only. Future phases may extend the
decision model, but do not add preemption, reservations, or multi-job actions
now.

### Decision Naming

Use:

```cpp
ScheduleDecision select_job(JobId job_id);
```

Do not use:

```cpp
ScheduleDecision schedule_job(JobId job_id);
```

Reason: `schedule_job` sounds like it actually starts the job. `select_job`
makes it clear that this only creates a decision object.

### Applying A Decision

Add:

```cpp
SimulatorState apply_schedule_decision(
    const SimulatorState& state,
    const ScheduleDecision& decision
);
```

Semantics:

1. If `decision.job_id` is empty, return the original state.
2. If `decision.job_id` has a value, call `start_pending_job`.
3. Preserve the existing exception policy from simulator state.

This helper keeps schedulers pure while giving tests and future simulator loops a
clean way to run:

```cpp
state = apply_schedule_decision(state, fifo_schedule(state));
```

The decision layer must not become a backdoor around simulator validation.

### Strict FIFO Scheduler Contract

Use:

```cpp
ScheduleDecision fifo_schedule(const SimulatorState& state);
```

FIFO should only consider jobs returned by:

```cpp
get_schedulable_jobs(state)
```

`get_schedulable_jobs(state)` is expected to return jobs ordered by:

1. `arrival_time`.
2. `JobId`.

FIFO should still have deterministic behavior even if tests add jobs out of
order.

Strict FIFO semantics:

1. Validate simulator state.
2. Get schedulable jobs.
3. If no jobs are schedulable, return `no_schedule_decision()`.
4. Look only at the head schedulable job.
5. If the head job fits available resources, return `select_job(head.id)`.
6. If the head job does not fit, return `no_schedule_decision()`.
7. Do not scan later jobs.
8. Do not mutate state.

This distinction is important:

```text
Available: 2 GPUs
Head job: needs 4 GPUs
Second job: needs 1 GPU
```

Strict FIFO returns no decision. It does not skip to the second job. Skipping the
blocked head job would drift toward backfilling, which belongs in a later phase.

## PR 1: Scheduler Decision Contract

### Goal

Create the policy-neutral type that represents a scheduler choice, plus the
helper that applies that choice through simulator transitions.

### Proposed Files

```text
include/helios/schedule_decision.hpp
src/schedule_decision.cpp
tests/cpp/schedule_decision_test.cpp
CMakeLists.txt
```

### Proposed Header Shape

```cpp
#pragma once

#include "helios/job.hpp"
#include "helios/simulator_state.hpp"

#include <optional>

namespace helios {

struct ScheduleDecision {
  std::optional<JobId> job_id;

  bool operator==(const ScheduleDecision&) const = default;
};

ScheduleDecision no_schedule_decision();
ScheduleDecision select_job(JobId job_id);
SimulatorState apply_schedule_decision(
    const SimulatorState& state,
    const ScheduleDecision& decision
);

}  // namespace helios
```

### Semantics

`no_schedule_decision`:

1. Returns a decision with empty `job_id`.

`select_job`:

1. Rejects negative job IDs with `std::invalid_argument`.
2. Returns a decision containing the selected job ID.

`apply_schedule_decision`:

1. Returns the original state when the decision is empty.
2. Calls `start_pending_job(state, job_id)` when the decision has a job ID.
3. Propagates simulator-state exceptions.

### Required Tests

1. `no_schedule_decision` returns empty `job_id`.
2. `select_job` stores the selected job ID.
3. `select_job` rejects negative job IDs.
4. Applying an empty decision returns unchanged state.
5. Applying a job decision starts that pending job.
6. Applying a decision for a future pending job throws.
7. Applying a decision for an already-running job throws.
8. Applying a decision for a completed job throws.
9. Applying an invalid job decision propagates simulator-state errors.

## PR 2: Strict FIFO Scheduler

### Goal

Implement the first classical baseline policy.

### Proposed Files

```text
include/helios/schedulers/fifo_scheduler.hpp
src/schedulers/fifo_scheduler.cpp
tests/cpp/fifo_scheduler_test.cpp
CMakeLists.txt
```

### Proposed Header Shape

```cpp
#pragma once

#include "helios/schedule_decision.hpp"
#include "helios/simulator_state.hpp"

namespace helios {

ScheduleDecision fifo_schedule(const SimulatorState& state);

}  // namespace helios
```

### Semantics

1. Validate simulator state.
2. Call `get_schedulable_jobs(state)`.
3. Return `no_schedule_decision()` if no jobs are schedulable.
4. Look at the first schedulable job only.
5. Return `select_job(head.id)` if the head job fits available resources.
6. Return `no_schedule_decision()` if the head job does not fit.
7. Do not inspect later jobs when the head job does not fit.
8. Do not mutate state.

### FIFO Policy Tests

1. Empty simulator returns no decision.
2. Future job returns no decision.
3. Single arrived fitting job is selected.
4. Single arrived non-fitting job returns no decision.
5. Multiple arrived jobs select earliest arrival.
6. Equal arrival times select smaller job ID.
7. FIFO selects correctly when jobs were added out of order.
8. FIFO does not skip an unfitting head job.
9. FIFO does not mutate state when it returns a valid decision.
10. FIFO does not mutate state when the head job does not fit.
11. Invalid simulator state is rejected.

### Scheduler-Driven Simulator Tests

1. FIFO decision starts one pending job.
2. Applying FIFO subtracts resources.
3. Applying FIFO moves selected job from pending to running.
4. Applying FIFO does not complete jobs automatically.
5. Empty FIFO decision leaves simulator state unchanged.
6. Repeated FIFO application can start multiple jobs when resources allow.
7. Repeated FIFO application stops when the head schedulable job does not fit.

Use a repeated-start test like:

```text
Total: 4 GPUs
Job A: arrival 0, needs 1 GPU
Job B: arrival 0, needs 1 GPU
Job C: arrival 0, needs 1 GPU
```

Then apply FIFO three times and verify A, then B, then C start.

Use a blocked-head test like:

```text
Total: 4 GPUs
Job A: arrival 0, needs 4 GPUs
Job B: arrival 0, needs 1 GPU
```

After starting A, available GPUs are zero. FIFO should return no decision. It
should not start B.

## Exception Rules

Use the existing policy:

```text
std::invalid_argument = invalid input or invalid existing state
std::runtime_error = valid request that cannot be performed
```

Specific rules:

1. `select_job` throws `std::invalid_argument` for negative job IDs.
2. `fifo_schedule` throws `std::invalid_argument` when state is invalid.
3. `apply_schedule_decision` throws whatever `start_pending_job` throws when the
   selected job cannot be started.

## Testing Philosophy

Scheduler tests should prove policy decisions, not simulator internals.

Good tests answer:

1. Which job did the scheduler choose?
2. Did it ignore future jobs?
3. Did it handle empty queues?
4. Did it respect strict FIFO semantics?
5. Did it avoid mutating state?
6. Did applying the decision use simulator transitions correctly?
7. Did the decision layer avoid bypassing simulator validation?

Avoid tests that require event loops, benchmark metrics, or RL behavior.

## CMake Integration

Expected new tests:

```text
helios_schedule_decision_test
helios_fifo_scheduler_test
```

Existing tests should keep passing:

```text
helios_smoke_test
helios_job_test
helios_resources_test
helios_job_state_test
helios_event_test
helios_simulator_state_test
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

## Reliability Risks And Mitigations

### Risk: FIFO Accidentally Becomes Backfilling

Mitigation:

1. Document strict FIFO.
2. Look only at the head schedulable job.
3. Test that FIFO does not skip an unfitting head job.

### Risk: Scheduler Mutates Simulator State

Mitigation:

1. Scheduler returns `ScheduleDecision`.
2. `apply_schedule_decision` handles mutation through existing simulator
   transition helpers.
3. Test that `fifo_schedule` leaves state unchanged in both decision and
   no-decision paths.

### Risk: Future Jobs Become Visible Too Early

Mitigation:

1. FIFO uses `get_schedulable_jobs`.
2. Test that future jobs return no decision.

### Risk: Scheduler Logic Duplicates Simulator Logic

Mitigation:

1. Scheduler checks fit only to decide whether to return a job.
2. Starting jobs still goes through `start_pending_job`.
3. Resource accounting remains in simulator/resource helpers.

### Risk: Scheduler Files Become A Junk Drawer

Mitigation:

1. Keep decision contract in `schedule_decision.hpp`.
2. Keep FIFO in `schedulers/fifo_scheduler.hpp`.
3. Add future schedulers as separate files.

## Definition Of Done

Phase 4 is complete when:

1. `ScheduleDecision` exists and is tested.
2. `no_schedule_decision` exists and is tested.
3. `select_job` exists and is tested.
4. `apply_schedule_decision` exists and is tested.
5. `fifo_schedule` exists and is tested.
6. FIFO ignores future jobs.
7. FIFO follows strict FIFO semantics.
8. FIFO does not skip an unfitting head job.
9. FIFO does not mutate state.
10. Scheduler decisions can start jobs through simulator transitions.
11. Decision application cannot bypass simulator validation.
12. CMake builds all new targets.
13. CTest passes locally.
14. Formatting passes locally and in CI.
15. No SJF, EDF, RL, Python bindings, benchmarks, plots, or dashboards have
    been introduced.

## Proposed Commit Sequence

PR 1:

1. `model: add scheduler decision contract`
2. `test: cover scheduler decisions`

PR 2:

3. `scheduler: add strict FIFO policy`
4. `test: cover FIFO scheduler`
5. `test: cover scheduler-driven simulator transitions`
6. `docs: document phase 4 scheduler scope`

## Suggested First Implementation Step

Start with the scheduler decision contract.

Implement only:

```text
include/helios/schedule_decision.hpp
src/schedule_decision.cpp
tests/cpp/schedule_decision_test.cpp
CMakeLists.txt
```

First coding goal:

1. Add `ScheduleDecision`.
2. Add `no_schedule_decision`.
3. Add `select_job`.
4. Add `apply_schedule_decision`.
5. Test empty decision, selected decision, negative job ID rejection, and
   applying decisions to simulator state.

Do not add FIFO until the decision type is tested.

## Milestone Walkthrough

Phase 4 happens in two layers.

First, we create the decision layer. This is the common language every scheduler
will use. It says either "start this job ID" or "start nothing right now." This
layer does not know FIFO, SJF, EDF, or RL. It only knows how to represent a
choice and apply that choice through the simulator's existing
`start_pending_job` transition.

Second, we create FIFO as the first policy. FIFO looks at the simulator state,
asks for schedulable jobs, checks only the head job, and returns a decision. It
does not mutate state. It does not start the job itself. It does not skip blocked
jobs. It just chooses.

The flow after Phase 4 should look like:

```cpp
auto decision = fifo_schedule(state);
state = apply_schedule_decision(state, decision);
```

That is the key architecture. Policies decide. Simulator transitions execute.

## Suggested Phase 5

After Phase 4, add more classical baselines one at a time.

Likely order:

1. SJF.
2. Priority.
3. EDF.
4. Random with deterministic seed control.
5. Backfilling.

Benchmarks should start only after at least FIFO and one other baseline exist.
