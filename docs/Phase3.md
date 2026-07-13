# Phase 3 Plan

## Purpose

Phase 3 turns the Phase 2 model primitives into the smallest useful
deterministic simulator runtime.

The goal is not full scheduling yet. The goal is to introduce enough C++
simulator state to represent known, running, and completed jobs, advance time
explicitly, and prove basic job lifecycle behavior with tests.

Python bindings, Gymnasium, PPO, dashboards, and benchmark scripts should still
wait. The C++ simulator needs a tested runtime core before it is worth exposing
to Python.

## What Is Already Done

### Milestone 1: Project Scaffold

Completed:

1. C++20 CMake project scaffold.
2. `helios_core` library.
3. Placeholder `helios_sim` CLI.
4. CTest smoke test.
5. Formatting scripts and `.clang-format`.
6. GitHub Actions CI for formatting, build, and tests.
7. README project overview and build instructions.

### Phase 2: Model Primitives

Completed:

1. `Resources` value type.
2. `ResourceAmount` semantic alias.
3. `validate_resources`.
4. `less_equal`.
5. `fits`.
6. `allocate`.
7. `release`.
8. Resource validation, comparison, fit, allocation, release, and round-trip
   tests.
9. `Job` value type.
10. `JobId`, `SimTime`, and `Duration` semantic aliases.
11. `Job::arrival_time`.
12. `Job::request`.
13. `validate_job`.
14. Job validation and equality tests.

Important design decisions already made:

1. C++ owns simulator logic.
2. `Job` is an input description, not runtime state.
3. Runtime fields like start time and completion time do not belong in `Job`.
4. Resource helpers return new values instead of mutating inputs.
5. Invalid input/state uses `std::invalid_argument`.
6. Valid but impossible operations use `std::runtime_error`.
7. Jobs must request at least one GPU.
8. CPU and memory requests may be zero for simplified synthetic workloads.

## High-Level Goals

Phase 3 should add:

1. Runtime job state separate from `Job`.
2. A tiny event representation for job arrival and job completion.
3. Minimal simulator state that owns total resources, available resources,
   pending jobs, running jobs, completed jobs, and current time.
4. A helper for schedulable jobs so future jobs are not accidentally visible to
   schedulers.
5. Deterministic state transitions for:
   - adding known jobs,
   - advancing time,
   - starting a selected job when it has arrived and resources fit,
   - completing running jobs,
   - releasing resources.
6. C++ tests for lifecycle behavior.

Phase 3 should not add:

1. FIFO, SJF, EDF, priority, random, or backfilling schedulers.
2. RL actions or rewards.
3. Python bindings.
4. Gymnasium environment.
5. Workload generation.
6. Benchmark CSV output.
7. Plots or dashboards.

## Branch Strategy

Use short feature branches from `main`.

Recommended branches:

```bash
feature/m3-runtime-job-state
feature/m3-event-model
feature/m3-simulator-state
```

Recommended first branch:

```bash
git switch main
git pull
git switch -c feature/m3-runtime-job-state
```

If the current branch is already close, such as `feature/m3-runtime-jobstate`,
it is fine to keep it. The important part is PR scope: runtime job state, not
Python bindings.

If the current branch is named for pybind work, rename or abandon it for now:

```bash
git branch -m feature/m3-runtime-job-state
```

Pybind is important later, but Phase 3 should stay inside C++ until the simulator
runtime is tested.

## Phase 3 Deliverables

By the end of Phase 3, the repository should contain:

1. `RunningJob`.
2. `CompletedJob`.
3. Runtime job validation helpers.
4. A tiny `Event` and `EventType`, unless deferred because it starts growing
   beyond a simple model.
5. A minimal `SimulatorState`.
6. `get_schedulable_jobs`.
7. Tests for empty simulator behavior.
8. Tests for single job lifecycle.
9. Tests for multiple job arrival ordering.
10. Tests for job completion ordering.
11. Tests for resource constraints.
12. Tests for invalid start and complete operations.
13. Tests that failed transitions do not mutate the original state.
14. CMake integration for new source and test targets.

## Core Design Decisions

### Keep `Job` Runtime-Free

`Job` remains the submitted workload request:

```cpp
struct Job {
  JobId id;
  SimTime arrival_time;
  Duration duration;
  Resources request;
};
```

Do not add runtime fields to `Job`.

Avoid adding:

1. `start_time`.
2. `completion_time`.
3. `state`.
4. `remaining_time`.
5. `assigned_node`.

Runtime state belongs in separate types.

### Separate Input Jobs From Runtime Jobs

Use separate types for runtime lifecycle state:

```cpp
struct RunningJob {
  Job job;
  SimTime start_time;
  SimTime completion_time;

  bool operator==(const RunningJob&) const = default;
};

struct CompletedJob {
  Job job;
  SimTime start_time;
  SimTime completion_time;

  bool operator==(const CompletedJob&) const = default;
};
```

This keeps derived times out of `Job` while making tests direct and readable.

### Completion Time Rule

Use:

```text
completion_time = start_time + job.duration
```

Be strict. A `RunningJob` or `CompletedJob` with a completion time that does not
match that rule should be invalid.

For Phase 3, keep time as integer `SimTime`.

### Pending Jobs Semantics

`pending_jobs` has a specific meaning:

```text
Jobs known to the simulator but not running or completed.
```

That means `pending_jobs` may contain:

1. Jobs that have arrived and are waiting.
2. Jobs with future `arrival_time` that are known but not yet visible to a
   scheduler.

Schedulers must not inspect raw `pending_jobs` later. They should use
`get_schedulable_jobs`.

### Schedulable Jobs Helper

Add:

```cpp
std::vector<Job> get_schedulable_jobs(const SimulatorState& state);
```

Semantics:

1. Validates simulator state.
2. Returns pending jobs where `job.arrival_time <= state.current_time`.
3. Preserves deterministic ordering by `arrival_time`, then `id`.
4. Does not mutate state.

This helper prevents future schedulers from accidentally selecting jobs that
have not arrived yet.

### Minimal Event Model

The event model is useful, but only if it stays tiny.

Possible design:

```cpp
enum class EventType {
  JobArrival,
  JobCompletion,
};

struct Event {
  SimTime time;
  EventType type;
  JobId job_id;

  bool operator==(const Event&) const = default;
};
```

Event ordering should be deterministic:

1. Earlier `time` first.
2. If equal time, `JobCompletion` before `JobArrival`.
3. If still tied, smaller `job_id` first.

Reason: processing completions before arrivals at the same time frees resources
before newly arrived jobs are considered.

Do not add:

1. `EventQueue`.
2. Priority queue wrappers.
3. Automatic event processing.
4. Event loop callbacks.
5. Scheduler hooks.

If the implementation grows beyond `Event`, `validate_event`, and `event_less`,
defer the event model to a later phase.

### Minimal Simulator State

Add a type that owns simulator state but not scheduler policy:

```cpp
struct SimulatorState {
  SimTime current_time;
  Resources total_resources;
  Resources available_resources;
  std::vector<Job> pending_jobs;
  std::vector<RunningJob> running_jobs;
  std::vector<CompletedJob> completed_jobs;

  bool operator==(const SimulatorState&) const = default;
};
```

Important: this state should not choose which job to run. Scheduler policy comes
later. Phase 3 should expose helper functions that start a specific pending job
by ID, but it should not implement FIFO, SJF, EDF, or any other policy.

## Step 1: Runtime Job State

### Goal

Represent running and completed jobs without changing `Job`.

### Proposed Files

```text
include/helios/job_state.hpp
src/job_state.cpp
tests/cpp/job_state_test.cpp
CMakeLists.txt
```

### Proposed Functions

```cpp
void validate_running_job(const RunningJob& running_job);
void validate_completed_job(const CompletedJob& completed_job);
RunningJob start_runtime_job(const Job& job, SimTime start_time);
CompletedJob complete_runtime_job(const RunningJob& running_job);
```

### Validation Rules

`start_runtime_job`:

1. Validates `job`.
2. Rejects `start_time < job.arrival_time`.
3. Sets `completion_time = start_time + job.duration`.

`validate_running_job`:

1. Validates embedded `job`.
2. Rejects `start_time < job.arrival_time`.
3. Rejects `completion_time != start_time + job.duration`.

`complete_runtime_job`:

1. Calls `validate_running_job`.
2. Returns a `CompletedJob` with the same job and times.

`validate_completed_job`:

1. Validates embedded `job`.
2. Rejects `start_time < job.arrival_time`.
3. Rejects `completion_time != start_time + job.duration`.

### Required Tests

1. Starting a valid job creates a `RunningJob`.
2. Starting at arrival time is allowed.
3. Starting after arrival time is allowed.
4. Starting before arrival time is rejected.
5. Completion time equals `start_time + duration`.
6. Completing a running job creates a `CompletedJob`.
7. Equal running jobs compare equal.
8. Equal completed jobs compare equal.
9. `validate_running_job` rejects an invalid embedded job.
10. `validate_running_job` rejects a wrong completion time.
11. `validate_completed_job` rejects an invalid embedded job.
12. `validate_completed_job` rejects a wrong completion time.

## Step 2: Tiny Event Model

### Goal

Represent simulator events in a deterministic, testable way without building an
event loop.

### Proposed Files

```text
include/helios/event.hpp
src/event.cpp
tests/cpp/event_test.cpp
CMakeLists.txt
```

### Proposed Functions

```cpp
void validate_event(const Event& event);
bool event_less(const Event& lhs, const Event& rhs);
```

### Validation Rules

1. `time >= 0`.
2. `job_id >= 0`.

### Ordering Rules

1. Earlier `time` first.
2. If times are equal, `JobCompletion` before `JobArrival`.
3. If time and type are equal, smaller `job_id` first.

### Required Tests

1. Valid arrival event passes validation.
2. Valid completion event passes validation.
3. Negative event time is rejected.
4. Negative job ID is rejected.
5. Earlier event sorts first.
6. Completion sorts before arrival at the same time.
7. Smaller job ID sorts first when time and type match.
8. Sorting the same events twice produces the same order.

## Step 3: Minimal Simulator State

### Goal

Represent simulator state and perform explicit state transitions without adding
scheduler policy.

### Proposed Files

```text
include/helios/simulator_state.hpp
src/simulator_state.cpp
tests/cpp/simulator_state_test.cpp
CMakeLists.txt
```

### Proposed API

```cpp
SimulatorState create_simulator_state(Resources total_resources);
void validate_simulator_state(const SimulatorState& state);

SimulatorState add_pending_job(const SimulatorState& state, const Job& job);
SimulatorState advance_time(const SimulatorState& state, SimTime new_time);
SimulatorState start_pending_job(const SimulatorState& state, JobId job_id);
SimulatorState complete_running_job(const SimulatorState& state, JobId job_id);
std::vector<Job> get_schedulable_jobs(const SimulatorState& state);
```

### Semantics

`create_simulator_state`:

1. Validates total resources.
2. Sets `current_time = 0`.
3. Sets `available_resources = total_resources`.
4. Starts with empty job vectors.

`add_pending_job`:

1. Validates state and job.
2. Adds job to pending jobs.
3. Keeps pending jobs ordered by `arrival_time`, then `id`.
4. Rejects duplicate job IDs across pending, running, and completed jobs.

`advance_time`:

1. Validates state.
2. Rejects negative `new_time`.
3. Rejects `new_time < state.current_time`.
4. Returns a new state with `current_time = new_time`.
5. Does not automatically start jobs.
6. Does not automatically complete jobs.

`start_pending_job`:

1. Validates state.
2. Finds a pending job by ID.
3. Validates that the job has arrived: `job.arrival_time <= current_time`.
4. Validates that resources fit.
5. Allocates resources.
6. Moves the job from pending to running.
7. Sets `start_time = current_time`.
8. Sets `completion_time = current_time + duration`.
9. Does not choose which job should start; the caller supplies `job_id`.

`complete_running_job`:

1. Validates state.
2. Finds a running job by ID.
3. Validates that `running_job.completion_time <= current_time`.
4. Releases resources.
5. Moves the job from running to completed.

`get_schedulable_jobs`:

1. Validates state.
2. Returns pending jobs whose `arrival_time <= current_time`.
3. Does not mutate state.

### Simulator State Validation

`validate_simulator_state` should check:

1. `current_time >= 0`.
2. `total_resources` is valid.
3. `available_resources` is valid.
4. `available_resources <= total_resources`.
5. All pending jobs are valid.
6. All running jobs are valid.
7. All completed jobs are valid.
8. No duplicate job IDs exist across pending, running, and completed jobs.
9. Running job resource usage plus available resources does not exceed total
   resources.

The final resource consistency check prevents impossible states such as a
cluster with four GPUs, three available GPUs, and a running job using three more
GPUs.

### Exception Rules

Use `std::invalid_argument` for invalid input or invalid existing state.

Use `std::runtime_error` for a valid request that cannot be performed in the
current state.

`add_pending_job` throws `std::invalid_argument` when:

1. State is invalid.
2. Job is invalid.

`add_pending_job` throws `std::runtime_error` when:

1. The job ID already exists.

`advance_time` throws `std::invalid_argument` when:

1. State is invalid.
2. `new_time` is negative.

`advance_time` throws `std::runtime_error` when:

1. `new_time < state.current_time`.

`start_pending_job` throws `std::invalid_argument` when:

1. State is invalid.
2. `job_id` is negative.

`start_pending_job` throws `std::runtime_error` when:

1. Job is not found.
2. Job has not arrived yet.
3. Job does not fit available resources.

`complete_running_job` throws `std::invalid_argument` when:

1. State is invalid.
2. `job_id` is negative.

`complete_running_job` throws `std::runtime_error` when:

1. Running job is not found.
2. `state.current_time < running_job.completion_time`.
3. Resource release would exceed total resources.

### Required Tests

1. Empty simulator has current time zero.
2. Empty simulator has available resources equal to total resources.
3. Empty simulator has no pending, running, or completed jobs.
4. Adding one job puts it in pending.
5. Adding jobs orders them by arrival time.
6. Adding jobs with equal arrival time orders by job ID.
7. Duplicate job IDs are rejected.
8. `get_schedulable_jobs` excludes future pending jobs.
9. `get_schedulable_jobs` includes arrived pending jobs.
10. Advancing time forward succeeds.
11. Advancing time to the same time succeeds.
12. Moving time backward is rejected.
13. Advancing time does not change resources by itself.
14. Advancing time does not move pending jobs by itself.
15. Starting a missing job is rejected.
16. Starting a job before arrival is rejected.
17. Starting a job that does not fit is rejected.
18. Starting a fitting job moves it from pending to running.
19. Starting a fitting job subtracts resources.
20. Completing a missing running job is rejected.
21. Completing a running job too early is rejected.
22. Completing a running job moves it to completed.
23. Completing a running job releases resources.
24. Single job lifecycle returns available resources to total.
25. Failed `add_pending_job` leaves the original state unchanged.
26. Failed `advance_time` leaves the original state unchanged.
27. Failed `start_pending_job` leaves the original state unchanged.
28. Failed `complete_running_job` leaves the original state unchanged.
29. Simulator validation rejects negative current time.
30. Simulator validation rejects available resources above total resources.
31. Simulator validation rejects duplicate IDs across containers.
32. Simulator validation rejects inconsistent running resource accounting.

## Explicit Time Advancement

Keep time advancement non-magical.

The intended flow is:

```cpp
state = advance_time(state, 10);
state = start_pending_job(state, job_id);
state = advance_time(state, 20);
state = complete_running_job(state, job_id);
```

`advance_time` should not start jobs, complete jobs, process events, or call a
scheduler. Later simulator loops can automate those explicit steps once the state
model is stable.

## CMake Integration

Expected new tests by the end of Phase 3:

```text
helios_job_state_test
helios_event_test
helios_simulator_state_test
```

Existing tests should continue passing:

```text
helios_smoke_test
helios_job_test
helios_resources_test
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

## Testing Philosophy

Tests should prove state transitions, not scheduler quality.

Good tests answer:

1. Is simulator state valid after every transition?
2. Are resources allocated and released exactly once?
3. Are invalid transitions rejected?
4. Are failed transitions non-mutating?
5. Is ordering deterministic?
6. Can a single job move from pending to running to completed?
7. Can schedulable jobs be queried without exposing future arrivals?

Avoid tests that imply scheduler policy. For example, do not test that the
simulator automatically chooses FIFO. FIFO belongs in a later scheduler phase.

## Reliability Risks And Mitigations

### Risk: Runtime Fields Leak Into `Job`

Mitigation:

1. Keep `Job` unchanged.
2. Add `RunningJob` and `CompletedJob`.
3. Test runtime conversion separately.

### Risk: Pending Jobs Become Ambiguous

Mitigation:

1. Document that `pending_jobs` means known but not running or completed.
2. Add `get_schedulable_jobs`.
3. Future schedulers should use `get_schedulable_jobs`, not raw `pending_jobs`.

### Risk: Simulator Accidentally Becomes A Scheduler

Mitigation:

1. Require callers to pass `job_id` to `start_pending_job`.
2. Do not auto-select jobs.
3. Do not add FIFO/SJF/EDF in Phase 3.

### Risk: Resource State Becomes Inconsistent

Mitigation:

1. Use Phase 2 `allocate` and `release`.
2. Validate state after transitions.
3. Test failed transitions for non-mutation.
4. Reject duplicate IDs across all job containers.
5. Reject states where running resource usage and available resources are
   inconsistent with total resources.

### Risk: Time Behavior Becomes Ambiguous

Mitigation:

1. Use integer `SimTime`.
2. Reject backward time movement.
3. Keep automatic event processing out of Phase 3.

## Definition Of Done

Phase 3 is complete when:

1. Runtime job types exist and are tested.
2. Runtime job validation exists and is tested.
3. Event types exist and are tested, or are explicitly deferred because they
   would grow beyond a tiny model.
4. Minimal simulator state exists and is tested.
5. `get_schedulable_jobs` exists and is tested.
6. Time advancement exists and is tested.
7. A single job can be added, started, and completed through explicit C++
   transitions.
8. Resource accounting remains correct through the lifecycle.
9. Invalid transitions are rejected with documented exception types.
10. Failed transitions are tested for non-mutation.
11. CMake builds all new targets.
12. CTest passes locally.
13. Formatting passes locally and in CI.
14. No scheduler algorithms, Python bindings, Gymnasium environment, or RL code
    has been introduced.

## Proposed PR Sequence

PR 1: Runtime job state

1. `model: add runtime job state`
2. `test: cover runtime job state`

PR 2: Event model

3. `model: add simulator event type`
4. `test: cover event ordering`

PR 3: Simulator state

5. `model: add simulator state`
6. `test: cover simulator state transitions`
7. `model: add explicit time advancement`
8. `test: cover simulator time advancement`
9. `docs: document phase 3 simulator runtime scope`

Do not combine these PRs. The simulator-state PR will be large enough on its
own.

## Suggested First Implementation Step

Start with runtime job state.

Implement only:

```text
include/helios/job_state.hpp
src/job_state.cpp
tests/cpp/job_state_test.cpp
CMakeLists.txt
```

First coding goal:

1. Add `RunningJob`.
2. Add `CompletedJob`.
3. Add `validate_running_job`.
4. Add `validate_completed_job`.
5. Add `start_runtime_job`.
6. Add `complete_runtime_job`.
7. Test start time, completion time, invalid early start, strict
   completion-time validation, and equality.

Do not add simulator state in the same first edit. Keep the first PR small and
boring.

## Suggested Phase 4

After Phase 3, add the first scheduler interface and the first baseline
scheduler.

Likely Phase 4 scope:

1. `Scheduler` interface or simple policy function.
2. FIFO scheduler.
3. Tests for empty queue behavior.
4. Tests for single job scheduling.
5. Tests for multiple job scheduling.
6. Tests for resource constraints.
7. Tests that scheduler selection is deterministic.

Only after scheduler baselines exist should pybind11 and Gymnasium become the
next major focus.
