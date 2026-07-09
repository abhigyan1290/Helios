# Helios

Reinforcement-learning-based scheduler for simulated GPU cluster workloads.

![CI](https://github.com/abhigyan1290/Helios/actions/workflows/ci.yml/badge.svg)

## Overview

Helios simulates a fixed-size GPU cluster where ML jobs arrive over time. Each job has
arrival time, runtime, GPU count, memory, priority, and deadline. A scheduler decides
which jobs to run; metrics such as wait time, completion time, and GPU utilization are
tracked and compared across classical baselines and RL agents.

## Stack

- **C++20** discrete-event simulator (CMake, Ninja, GoogleTest)
- **Python 3.11+** bindings (pybind11), Gymnasium environment, Stable-Baselines3 PPO
- **GitHub Actions** CI on Linux and Windows

## Quick Start

### Build C++

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

### Run FIFO baseline

```bash
./build/helios_cli --scheduler fifo --seed 42 --jobs 100
```

### Python tests

```bash
pip install -e ".[dev]"
pytest tests/python -v
```

## Project Structure

```
include/helios/   C++ headers
src/              C++ implementation
bindings/         pybind11 module
python/helios/    Python package (env, training, benchmarks)
tests/cpp/        GoogleTest unit tests
tests/python/     pytest suite
configs/          Workload YAML configs
docs/             Architecture and metrics documentation
```

## Benchmarks

See [docs/metrics.md](docs/metrics.md) for metric definitions. Run baselines:

```bash
python -m helios.benchmarks.run_baselines --config configs/workloads/small_cluster.yaml
python -m helios.benchmarks.plot_results --input results/benchmarks.csv
```

## License

MIT — see [LICENSE](LICENSE).
