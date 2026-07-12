# Phase 2 Plan

## Purpose

Phase 2 establishes the first real simulator domain model for Helios. The goal is
to add small, deterministic C++ value types that future scheduling, simulation,
benchmarking, and Python bindings can safely build on.

This phase should stay narrow. It should not introduce scheduler algorithms,
event simulation, reinforcement learning, Python bindings, workload generation,
or benchmark scripts.

## High-Level Goals

1. Add a tested C++ `Resources` value type.
2. Add tested C++ resource accounting primitives.
3. Add a tested C++ `Job` value type that contains a resource request.
4. Define clear validation rules for invalid simulator inputs.
5. Keep all simulator logic in C++.
6. Preserve deterministic behavior.
7. Expand CTest coverage without adding unnecessary framework complexity yet.
8. Keep each change small enough to review.

## Branch Strategy

Use the current `milestone-2` branch to discuss and refine this plan.

For implementation, prefer short feature branches from `main`, with one pull
request per small feature:

```bash
feature/m2-resource-model
feature/m2-job-model
docs/m2-model-notes
```

Recommended workflow for the first implementation branch:

```bash
git switch main
git pull
git switch -c feature/m2-resource-model
```

After that PR merges:

```bash
git switch main
git pull
git switch -c feature/m2-job-model
```

Avoid using `milestone-2` as a long-lived integration branch. For a solo learning
project, short feature branches teach the real GitHub pull request workflow more
clearly and reduce branch confusion.

## Phase 2 Deliverables
By the end of Phase 2, the repository should contain:

1. A `Resources` model in C++.
2. Resource validation tests.
3. Resource comparison, fit, allocation, and release helpers.
4. Resource accounting tests.
5. A `Job` model in C++.
6. Job validation tests.
7. CMake integration for the new source and test targets.
8. A small README or documentation update if the public project status changes.

## Core Design Decisions

### Semantic Type Aliases
Use aliases instead of raw `int` everywhere in public model types:

```cpp
using JobId = int;
using SimTime = int;
using Duration = int;
using ResourceAmount = int;
```

These aliases keep the first implementation simple while documenting intent. If
Helios later needs larger timestamps or different resource units, these aliases
give us an obvious place to evolve the model.

### Naming
Use `arrival_time`, not `submit_time`.

In scheduling and simulation, arrival time is the standard term for when a job
becomes visible to the scheduler. This also lines up cleanly with future event
names like `JobArrival` and `JobCompletion`.

Use `memory_mb`, not `memory_gb`.

Memory should use an unambiguous integer unit. `memory_mb` gives enough
precision for synthetic GPU workloads and avoids mixing the terms GB and GiB.

### Exception Policy

Use specific exception types consistently:

- `std::invalid_argument`: malformed input or impossible state.
- `std::runtime_error`: valid input, but the requested operation cannot be
  performed.

Examples:

- Negative GPU capacity: `std::invalid_argument`.
- Invalid job duration: `std::invalid_argument`.
- Allocation request does not fit: `std::runtime_error`.
- Release would exceed total capacity: `std::runtime_error`.
- Available resources already exceed total capacity before release:
  `std::invalid_argument`.

Tests should assert the specific exception type.

## Step 1: Add The Resources Value Type

### Goal

Create the resource representation that both cluster capacity and job resource
requests will use.

### Proposed Files

```text
include/helios/resources.hpp
src/resources.cpp
tests/cpp/resources_test.cpp
CMakeLists.txt
```

### Proposed Data Model

```cpp
namespace helios {

using ResourceAmount = int;

struct Resources {
  ResourceAmount gpu_count;
  ResourceAmount cpu_count;
  ResourceAmount memory_mb;

  bool operator==(const Resources&) const = default;
};

}  // namespace helios
```

### Field Meaning

- `gpu_count`: number of GPUs.
- `cpu_count`: number of CPUs.
- `memory_mb`: memory capacity or request in megabytes.

### Initial Validation Rules

- `gpu_count >= 0`
- `cpu_count >= 0`
- `memory_mb >= 0`

Zero-capacity resources are valid. They are useful for representing an empty
cluster in tests and zero CPU or memory requests in simplified synthetic jobs.

### Proposed Helper Functions

```cpp
void validate_resources(const Resources& resources);
bool less_equal(const Resources& lhs, const Resources& rhs);
bool fits(const Resources& available, const Resources& requested);
Resources allocate(const Resources& available, const Resources& requested);
Resources release(
    const Resources& available,
    const Resources& total,
    const Resources& released
);
```

### Recommended Semantics

`validate_resources`:

- Throws `std::invalid_argument` if any resource dimension is negative.

`less_equal`:

- Returns `true` when every dimension in `lhs` is less than or equal to the
  matching dimension in `rhs`.
- Returns `false` otherwise.
- Does not mutate anything.

`fits`:

- Validates `available` and `requested`.
- Returns `true` when `requested <= available`.
- Returns `false` otherwise.
- Does not mutate anything.

`allocate`:

- Validates `available` and `requested`.
- Throws `std::runtime_error` if `requested` does not fit.
- Returns a new `Resources` value with `requested` subtracted from `available`.
- Does not mutate the input `Resources`.

`release`:

- Validates `available`, `total`, and `released`.
- Throws `std::invalid_argument` if `available > total` before release.
- Throws `std::runtime_error` if adding `released` would make the result exceed
  `total`.
- Returns a new `Resources` value with `released` added back.
- Does not mutate the input `Resources`.

Returning new values instead of mutating in place keeps behavior simple and
deterministic for early tests. A later simulator state object can choose whether
to store and update these values internally.

## Step 2: Test The Resources Model

### Goal

Make resource validation and accounting reliable before implementing scheduler
decisions.

### Proposed Test Target

```text
helios_resources_test
```

### Required Validation Tests

1. Valid resources pass validation.
2. Zero-capacity resources are valid.
3. Negative GPU capacity is rejected with `std::invalid_argument`.
4. Negative CPU capacity is rejected with `std::invalid_argument`.
5. Negative memory capacity is rejected with `std::invalid_argument`.

### Required Equality Tests

1. Equal resource values compare equal.
2. Different GPU values compare unequal.
3. Different CPU values compare unequal.
4. Different memory values compare unequal.

### Required Comparison Tests

1. `less_equal` returns true for exact match.
2. `less_equal` returns true when the left side is smaller in every dimension.
3. `less_equal` returns false if the left side exceeds GPUs.
4. `less_equal` returns false if the left side exceeds CPUs.
5. `less_equal` returns false if the left side exceeds memory.

### Required Fit Tests

1. Request fits when all requested resources are available.
2. Request does not fit when GPUs are insufficient.
3. Request does not fit when CPUs are insufficient.
4. Request does not fit when memory is insufficient.
5. Exact resource match fits.
6. Empty cluster does not fit a request that requires GPUs.

### Required Allocation Tests

1. Allocation subtracts GPUs exactly.
2. Allocation subtracts CPUs exactly.
3. Allocation subtracts memory exactly.
4. Exact-fit allocation leaves zero available resources.
5. Allocation rejects a request that does not fit with `std::runtime_error`.
6. Failed allocation leaves the original available resources unchanged.

### Required Release Tests

1. Release adds GPUs exactly.
2. Release adds CPUs exactly.
3. Release adds memory exactly.
4. Allocate then release returns to the original resources.
5. Release rejects if available GPUs already exceed total GPUs.
6. Release rejects if available CPUs already exceed total CPUs.
7. Release rejects if available memory already exceeds total memory.
8. Release rejects if released resources would exceed total capacity.
9. Release with zero resources is allowed when the state is valid.
10. Failed release leaves the original available resources unchanged.

## Step 3: Add The Job Value Type

### Goal

Create the first simulator input type: a job submitted to a GPU cluster.

### Proposed Files

```text
include/helios/job.hpp
src/job.cpp
tests/cpp/job_test.cpp
CMakeLists.txt
```

`Job` should live in `job.hpp`. Validation can live in `src/job.cpp`. Since
`Job` contains `Resources request`, `resources.hpp` should exist first.

### Proposed Data Model

```cpp
namespace helios {

using JobId = int;
using SimTime = int;
using Duration = int;

struct Job {
  JobId id;
  SimTime arrival_time;
  Duration duration;
  Resources request;

  bool operator==(const Job&) const = default;
};

}  // namespace helios
```

### Field Meaning

- `id`: stable identifier for the job.
- `arrival_time`: simulation time when the job becomes available.
- `duration`: amount of simulated time the job runs once started.
- `request`: resources required while the job is running.

### Initial Validation Rules

- `id >= 0`
- `arrival_time >= 0`
- `duration > 0`
- `request.gpu_count > 0`
- `request.cpu_count >= 0`
- `request.memory_mb >= 0`

Jobs must request at least one GPU because Helios currently models GPU-cluster
scheduling. CPU and memory may be zero to support simplified synthetic
workloads.

### Recommended Validation Interface

Use an explicit validation function:

```cpp
void validate_job(const Job& job);
```

For invalid jobs, throw `std::invalid_argument`.

This keeps `Job` as a simple value type while making validation deliberate and
easy to test. Later, if the simulator needs non-throwing parsing or workload
loading, a separate factory can return `std::optional<Job>` or an error type.

### Job Behavior To Avoid For Now

Do not add:

- Scheduler state.
- Start time.
- Completion time.
- Wait time.
- Slowdown.
- Priority.
- Deadline.
- User ID.
- Queue ID.

Those fields may be useful later, but adding them now would blur the boundary
between an input job description and simulator runtime state.

## Step 4: Test The Job Model

### Goal

Prove the job model has stable invariants before scheduler logic depends on it.

### Proposed Test Target

```text
helios_job_test
```

### Required Tests

1. Valid job passes validation.
2. Job with negative ID is rejected with `std::invalid_argument`.
3. Job with negative arrival time is rejected with `std::invalid_argument`.
4. Job with zero duration is rejected with `std::invalid_argument`.
5. Job with negative duration is rejected with `std::invalid_argument`.
6. Job with zero requested GPUs is rejected with `std::invalid_argument`.
7. Job with negative requested GPUs is rejected with `std::invalid_argument`.
8. Job with negative requested CPU is rejected with `std::invalid_argument`.
9. Job with negative requested memory is rejected with `std::invalid_argument`.
10. Minimal valid job is accepted.
11. Job with valid zero CPU and zero memory is accepted.
12. Equal job values compare equal.
13. Different job values compare unequal.

### Deterministic Ordering Tests

Do not add a comparator in Phase 2 unless implementation feedback reveals a
clear need. Ordering belongs more naturally to a later workload loader, event
queue, pending queue, or scheduler policy.

If a comparator is introduced later, test:

1. Earlier `arrival_time` sorts first.
2. Equal `arrival_time` sorts by `id`.
3. Sorting the same input twice produces the same order.

## Step 5: Update CMake

### Goal

Integrate the new model and tests into the existing C++ build.

### Proposed Changes

Update `CMakeLists.txt` so:

1. `helios_core` includes any new `.cpp` files.
2. `helios_resources_test` is built and linked against `helios_core`.
3. `helios_job_test` is built and linked against `helios_core`.
4. Both tests are registered with CTest.
5. The existing `helios_smoke_test` keeps working.

Separate test targets are slightly more CMake work, but they are useful for
learning CTest and keeping failures easy to read.

### Expected Test List

After this step, CTest should include:

```text
helios_smoke_test
helios_resources_test
helios_job_test
```

## Step 6: Run Local Verification

### Required Commands

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

### Optional Formatting Check

```bash
./scripts/format.sh
```

The formatting script should be run before committing if C++ files were added or
changed.

## Step 7: Documentation Update

### Goal

Keep project documentation accurate without over-documenting unfinished systems.

### Proposed Files

```text
README.md
docs/model.md
```

Only add `docs/model.md` if the validation rules become too detailed for the
README.

### README Update

If Phase 2 implementation is merged, update the current status to mention:

- Milestone 1 scaffold is complete.
- Phase 2 introduces basic C++ job and resource primitives.
- Scheduler logic is still planned future work.

## Testing Philosophy For Phase 2

Phase 2 tests should focus on invariants, not implementation details.

Good tests answer:

- Can valid inputs be represented?
- Are invalid inputs rejected?
- Is resource accounting exact?
- Are failed operations non-mutating?
- Is behavior deterministic?

Tests should not assume future scheduler behavior.

## Reliability Risks And Mitigations

### Risk: Invalid Jobs Enter The Simulator

Mitigation:

- Add explicit job validation.
- Test every invalid field.
- Validate `Job::request` through the same resource rules.

### Risk: Resource Accounting Becomes Inconsistent

Mitigation:

- Use simple value-returning helper functions.
- Test allocation and release separately.
- Test allocate-then-release round trips.
- Reject over-release.
- Reject impossible states where available resources already exceed total
  resources.

### Risk: Python Later Duplicates C++ Logic

Mitigation:

- Keep job and resource behavior in `helios_core`.
- Future pybind11 bindings should expose these C++ types instead of
  reimplementing validation in Python.

### Risk: Adding Too Much Too Early

Mitigation:

- Do not add schedulers in Phase 2.
- Do not add event queues in Phase 2.
- Do not add runtime job state in Phase 2.
- Do not add RL or Gymnasium code in Phase 2.
- Do not add priority or deadline fields until EDF or priority scheduling work
  needs them.

## Definition Of Done

Phase 2 is complete when:

1. `Resources` exists as a C++ value type.
2. Resource validation is implemented and tested.
3. `less_equal`, `fits`, `allocate`, and `release` are implemented and tested.
4. `Job` exists as a C++ value type.
5. `Job` validation is implemented and tested.
6. CMake builds all new targets.
7. CTest passes locally.
8. Formatting passes.
9. Documentation reflects the new project status.
10. No scheduler, event loop, Python binding, or RL logic has been introduced.

## Proposed Commit Sequence

1. `model: add resource value type and validation`
2. `test: cover resource validation`
3. `model: add job value type and validation`
4. `test: cover job validation`
5. `model: add resource fit allocation release helpers`
6. `test: cover resource accounting`
7. `docs: document phase 2 model scope`

If combining into fewer commits, keep the final pull request small and ensure
each commit still builds or is easy to review.

## How To Begin

Start with the resources feature branch, because `Job` depends on
`Resources request`.

1. Save or commit this planning document if you want it preserved.
2. Return to `main` and update it:

```bash
git switch main
git pull
```

3. Create the first implementation branch:

```bash
git switch -c feature/m2-resource-model
```

4. Implement only:

```text
include/helios/resources.hpp
src/resources.cpp
tests/cpp/resources_test.cpp
CMakeLists.txt
```

5. Run:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./scripts/format.sh
```

6. Commit and open a PR for the resources model before starting jobs.

## Suggested Next Phase

After Phase 2, the next phase should likely introduce the smallest simulator
runtime concept:

1. Runtime job state, such as pending, running, and completed.
2. A deterministic event representation.
3. A minimal simulator clock.
4. Tests for empty queue behavior, single job execution, and completion
   ordering.

Scheduler algorithms should still wait until the simulator state transitions are
well tested.
