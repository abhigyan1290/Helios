# Helios

Helios is a C++/Python project for studying GPU cluster job scheduling with classical
baselines and reinforcement learning.

The first milestone is intentionally small: establish a clean repository, a C++20 build,
formatting rules, CI, and a tiny smoke test before implementing simulator logic.

## Current Status

- Milestone 1 in progress: project scaffolding and CI.
- C++ simulator logic has not been implemented yet.
- Python bindings, Gymnasium environment, and PPO training are planned future milestones.

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

Run the placeholder CLI:

```bash
./build/helios_sim
```

On Windows PowerShell, the executable is usually:

```powershell
.\build\helios_sim.exe
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

## Development Workflow

Use one branch per issue, open pull requests into `main`, and keep every change small
enough to review. Simulator features should include tests before they are merged.

