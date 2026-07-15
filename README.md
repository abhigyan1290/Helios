# Helios

Helios is a C++/Python project for studying GPU cluster job scheduling with classical
baselines and, later, reinforcement learning.

The current simulator core is written in C++. It can run deterministic FIFO scheduling
over small named demo workloads and emit per-job benchmark metrics as CSV.

## Current Status

- Milestone 1 scaffold is complete.
- Phase 2 added C++ job/resource model primitives.
- Phase 3 added runtime job state, events, simulator state, and explicit lifecycle transitions.
- Phase 4 added scheduler decisions and a strict FIFO scheduler.
- Phase 5 added workload/metrics types, a FIFO simulation runner, CSV output, and named demo workloads.
- Large synthetic workload generation, additional scheduler baselines, Python bindings, Gymnasium, and RL are still future work.

## Current Capabilities

- Validate jobs, resources, workloads, and simulator state.
- Allocate and release GPU/CPU/memory resources.
- Move jobs through pending, running, and completed states.
- Run strict FIFO scheduling end to end over deterministic workloads.
- Compute per-job metrics:
  - arrival time
  - start time
  - completion time
  - duration
  - wait time
  - turnaround time
- Print benchmark results as CSV.

## Planned Stack

- C++20, CMake, Ninja
- CTest for initial smoke tests; GoogleTest or Catch2 for simulator unit tests later
- Python 3.11+, pybind11, Gymnasium, PyTorch, Stable-Baselines3
- GitHub Actions CI

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Run the CLI:

```bash
./build/helios_sim
```

On Windows PowerShell, the executable is usually:

```powershell
.\build\helios_sim.exe
```

List built-in demo workloads:

```bash
./build/helios_sim --list-demo-workloads
```

Run a FIFO demo workload and print CSV:

```bash
./build/helios_sim --demo-fifo light-overlap
./build/helios_sim --demo-fifo fifo-blocking
./build/helios_sim --demo-fifo staggered-arrivals
./build/helios_sim --demo-fifo gpu-contention
```

Save output to a CSV file:

```bash
./build/helios_sim --demo-fifo fifo-blocking > benchmarks/fifo_blocking.csv
```

CSV columns:

```text
job_id,arrival_time,start_time,completion_time,duration,wait_time,turnaround_time
```

## Format

```powershell
.\scripts\format.ps1
```

or:

```bash
./scripts/format.sh
```

## Project Structure

```text
apps/helios_sim/   Command-line simulator executable
include/helios/    Public C++ headers
src/               C++ implementation
tests/cpp/         C++ tests
python/helios/     Future Python package
tests/python/      Future pytest suite
benchmarks/        Future benchmark runners and outputs
configs/           Future experiment and workload configs
docs/              Design notes and metric definitions
scripts/           Developer scripts
```

## Current Limitations

- Demo workloads are small and hand-written.
- Job durations are known simulated runtimes.
- FIFO is the only implemented scheduler baseline.
- No large-scale synthetic workload generator yet.
- No Python bindings, Gymnasium environment, PPO training, plots, or dashboards yet.

## Development Workflow

Use one branch per issue, open pull requests into `main`, and keep every change small
enough to review. Simulator features should include tests before they are merged.
