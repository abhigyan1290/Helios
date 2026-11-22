# Phase 11 Plan: Python Bindings

## Purpose

Expose the existing C++ interactive simulator to Python through pybind11. A
Python caller should be able to construct a workload, run an episode one
decision at a time, and inspect its terminal results. C++ remains the sole owner
of simulation and resource-accounting logic.

This document calls the binding work **Phase 11**. The existing
`docs/Runtime.md` uses **Milestone 11** for the already implemented C++
interactive API; that milestone is a prerequisite, not this phase's deliverable.

## Starting Point

- `helios::Simulator` owns the state and provides `reset`, `observe`, `step`,
  `done`, and terminal `result` methods.
- A decision either starts one arrived, fitting job or advances to the next
  arrival or completion. Starting a job does not advance simulation time.
- Default observations are value snapshots and omit future jobs, actual job
  durations, and actual completion timestamps.
- Classical schedulers and benchmark runners already exist in C++. Python has
  benchmark scripts, but no simulator bindings or Gymnasium environment.
- The working tree contains substantial uncommitted changes. Preserve them and
  review the binding changes in small, independent increments.

## Scope And Boundaries

Expose the minimum types and operations needed for an interactive episode:
resources, job and workload input, observations and their job views, scheduling
decisions, step results, and terminal simulation metrics. Use the existing C++
validation and transition semantics. Observations and results must remain
independent snapshots whose lifetime does not depend on the simulator.

Do not expose `observe_oracle()` or `diagnostic_state()` through the normal
Python agent interface. Workload inputs necessarily contain actual durations;
keep them distinct from observations and avoid returning them to an agent via
the observation API. This is an API boundary, not a security sandbox.

Do not introduce Gymnasium spaces, reward functions, PPO, dashboards, or a
second simulator in Python in this phase. Keep existing C++ interfaces and
benchmark behavior compatible. Do not silently change existing public APIs.

## Proposed Files

- `CMakeLists.txt`: optional pybind11 extension target, preserving C++-only builds.
- `python/bindings.cpp`: bindings for the existing C++ value types and runtime.
- `python/helios/__init__.py`: a small, documented Python import surface.
- `pyproject.toml`: Python build and installation configuration, if needed for
  reproducible local installation; select and document a single build path.
- `tests/python/test_simulator_bindings.py`: behavior tests across the boundary.
- `README.md`: dependency, build, installation, and minimal usage instructions.

These are proposed paths; adjust only when the build integration requires it.
Do not add a second copy of scheduling or simulation logic.

## Implementation Sequence

### 1. Build And Import

Add an opt-in extension target using CMake and pybind11, with position-independent
code where needed for the core library. Keep configuration and tests usable when
Python bindings are disabled and pybind11 is unavailable. Document Python 3.11+
and the exact development install and import commands. Verify the extension can
be imported from a clean environment, rather than relying on an accidental
working-directory or `PYTHONPATH` setting.

### 2. Input Value Types

Bind the existing resource, job, and workload types with explicit constructors
and named fields. Retain the distinction between actual and optional estimated
durations. Let existing C++ validation reject invalid inputs; document Python
exception types and integer range behavior. Test valid inputs and representative
invalid resources, jobs, duplicate IDs, and impossible requests.

### 3. Interactive Runtime

Bind the simulator, decisions, observations, and step results. Choose and
document explicit Python operations for selecting a job and waiting; do not
silently reinterpret an invalid selection as a wait. Expose the existing reset
contract and translate C++ exceptions consistently. The Python layer must not
implement time advancement or resource allocation.

Test empty episodes; single and multiple jobs; resource constraints; arrival
and completion ordering, including ties; repeated starts at one timestamp; and
waiting when another event exists. Test invalid, future, already-running,
completed, and non-fitting selections, plus stepping after termination. Failed
actions and invalid resets must preserve state. Verify snapshots remain usable
after later steps, reset, and destruction of the simulator.

### 4. Results And End-To-End Parity

Expose terminal aggregate and per-job metrics with their existing meanings.
Reject `result()` before termination as C++ does. Run a deterministic workload
from Python and compare its transitions and final metrics with a known C++ run;
include deterministic workload generation if a generator is exposed. Test that
otherwise identical observations do not reveal hidden actual durations or future
jobs. Add a short example to the README.

## Validation

Run the normal C++ build and tests with bindings disabled, then configure and
build with bindings enabled and run the Python binding tests in a clean Python
3.11+ environment. Run the existing Python tests as well:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
pytest tests/python
```

Document the concrete binding-enabled configure/install commands when the build
path is implemented. Verify that a C++-only configuration does not require
Python or pybind11. Keep tests focused on observable behavior, not duplicated
implementations of the simulator.

## Definition Of Done

- A documented clean build and install produces an importable Python module.
- Python can create, reset, observe, step, and finish an episode using C++.
- Exposed observations preserve the existing information and ownership boundary.
- Python exceptions and failed-operation state preservation are tested.
- End-to-end results agree with a deterministic C++ reference workload.
- Existing C++ and Python tests pass, including a C++-only build.
- The README describes usage, prerequisites, and limitations.

## Follow-Up

After bindings work, specify the Gymnasium observation and action spaces,
including variable queue sizes, invalid actions, termination, and seeding. Then
add configurable rewards and evaluate PPO against the existing baselines on
matched workloads and seeds. A multi-seed comparison of all five classical
policies is also needed for a stronger benchmark reference.
