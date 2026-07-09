# Helios Agent Instructions

Helios is a C++/Python reinforcement-learning project for GPU cluster job scheduling.

## Project Goal

Build a C++ discrete-event simulator for GPU cluster scheduling, expose it to Python with pybind11, wrap it as a Gymnasium reinforcement learning environment, and train PPO agents to optimize scheduling decisions against classical baselines.

## Important Rule

Do not build the entire project in one large edit.

Every change should be small, testable, and easy to review.

## Architecture Rules

* C++ owns the core simulator.
* Python owns RL training, benchmarking scripts, plotting, and experiment configuration.
* The Python Gymnasium environment should call into the C++ simulator.
* Do not duplicate simulation logic in Python.
* Keep scheduler logic modular.
* Keep reward logic configurable.

## C++ Rules

* Use C++20.
* Use CMake.
* Prefer simple value types.
* Avoid raw owning pointers.
* Prefer `std::vector`, `std::optional`, and clear ownership.
* Write deterministic logic wherever possible.
* Add unit tests for new simulator behavior.
* Use clear class interfaces before adding complex features.

## Python Rules

* Use Python 3.11+.
* Use pytest for tests.
* Use Gymnasium for the RL environment API.
* Use Stable-Baselines3 for the first PPO implementation.
* Keep training scripts separate from environment logic.
* Save benchmark outputs as CSV.

## Testing Rules

Every new major feature should include tests.

Minimum tests:

* Empty queue behavior
* Single job scheduling
* Multiple job scheduling
* Resource constraints
* Job arrival ordering
* Job completion ordering
* Invalid scheduler actions
* Deterministic workload generation

## Agent Behavior

Before editing code:

1. Explain what you plan to change.
2. Identify the files involved.
3. Identify edge cases.
4. Keep the change narrow.

After editing code:

1. Explain what changed.
2. Explain how to test it.
3. Mention any limitations or follow-up tasks.

## Do Not Do

* Do not rewrite unrelated files.
* Do not introduce RL before the simulator and baselines work.
* Do not add dashboards before benchmarks work.
* Do not hide core logic behind unnecessary abstractions.
* Do not invent metrics without documenting them.
* Do not silently change public interfaces.

## Build Commands

Configure C++:

```bash
cmake -S . -B build -G Ninja
```

Build C++:

```bash
cmake --build build
```

Run C++ tests:

```bash
ctest --test-dir build --output-on-failure
```

Run Python tests:

```bash
pytest tests/python
```

Format C++:

```bash
./scripts/format.sh
```
