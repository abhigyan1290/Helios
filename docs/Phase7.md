# Phase 7 Plan

## Purpose

Phase 7 turns Helios's hand-written demo workloads into deterministic generated
benchmark suites.

The goal is not to add reinforcement learning yet. The goal is to make workload
creation standardized:

```text
same generator config
same seed
same cluster resources
same workload
FIFO result
SJF result
comparable metrics
```

By the end of this phase, Helios should be able to generate small, medium, and
large synthetic workloads in a repeatable way and use those workloads to compare
FIFO and SJF.

## What Is Already Done

### Milestone 1: Project Scaffold

Completed:

1. C++20 CMake project scaffold.
2. `helios_core` library.
3. CLI executable.
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
4. CSV output.
5. Named demo workloads.

### Phase 6: FIFO vs SJF Comparison

Completed or in review:

1. Generic scheduler runner.
2. `run_fifo_simulation`.
3. `run_sjf_simulation`.
4. Non-preemptive oracle SJF scheduler.
5. Aggregate wait metrics.
6. CLI comparison between FIFO and SJF.
7. `large-varied-100` named workload.
8. Tests proving FIFO and SJF can match or differ on the same workload.

## High-Level Goals

Phase 7 should add:

1. A deterministic workload generator config.
2. Seeded synthetic workload generation.
3. Standard generated workload sizes.
4. Tests proving generated workloads are valid and deterministic.
5. CLI support for comparing generated workloads.
6. A small benchmark-suite structure that can run FIFO and SJF over multiple
   generated workloads.

Phase 7 should not add:

1. Python bindings.
2. Gymnasium.
3. PPO.
4. RL action spaces.
5. Reward functions.
6. Plotting.
7. Dashboards.
8. New advanced schedulers.
9. Real cluster trace ingestion.
10. Estimated versus actual runtime splitting.

## Core Design Decisions

### Deterministic First

Synthetic workloads must be reproducible.

The same config and seed must always produce the same `Workload`:

```cpp
const auto a = generate_workload(config);
const auto b = generate_workload(config);
assert(a == b);
```

Different seeds should usually produce different workloads, but tests should not
depend on probabilistic behavior. Use deterministic assertions with fixed seeds.

### Keep Generation In C++ For Now

The generator should live in C++ because the simulator and benchmark runner are
still C++.

Do not add Python workload generation yet. Python will become useful later for
experiment orchestration, plotting, and RL, but Phase 7 should keep benchmark
inputs close to the simulator.

### Use Simple Config Values

Start with a clear value type:

```cpp
struct WorkloadGeneratorConfig {
  int job_count;
  unsigned int seed;
  SimTime max_arrival_time;
  Duration min_duration;
  Duration max_duration;
  ResourceAmount min_gpu_count;
  ResourceAmount max_gpu_count;
  ResourceAmount min_cpu_count;
  ResourceAmount max_cpu_count;
  ResourceAmount min_memory_mb;
  ResourceAmount max_memory_mb;

  bool operator==(const WorkloadGeneratorConfig&) const = default;
};
```

Use inclusive ranges.

Validation should reject:

1. negative `job_count`,
2. negative `max_arrival_time`,
3. `min_duration <= 0`,
4. `max_duration < min_duration`,
5. `min_gpu_count <= 0`,
6. `max_gpu_count < min_gpu_count`,
7. negative CPU or memory ranges,
8. maximum resource requests that exceed the intended benchmark cluster.

### Avoid Unschedulable Generated Jobs

The generator should not create jobs that can never fit the benchmark cluster.

Recommended API:

```cpp
Workload generate_workload(
    const WorkloadGeneratorConfig& config,
    Resources cluster_resources
);
```

This makes validation explicit: generated jobs must fit `cluster_resources`.

If a config allows impossible requests, reject the config with
`std::invalid_argument` before generating jobs.

### Stable Job IDs

Generated jobs should use stable IDs:

```text
1, 2, 3, ...
job_count
```

The workload builder can still sort by arrival time and job ID. Stable IDs make
debugging and CSV comparison much easier.

### Standard Benchmark Profiles

Add named generator profiles instead of asking users to remember many numeric
arguments.

Possible profiles:

```text
generated-small
generated-medium
generated-large
generated-bursty
generated-gpu-heavy
```

Suggested first sizes:

```text
generated-small: 25 jobs
generated-medium: 100 jobs
generated-large: 500 jobs
```

Keep `large-varied-100` as a hand-authored demo workload for now. Generated
profiles should live beside it, not replace it immediately.

## Proposed Files

```text
include/helios/workload_generator.hpp
src/workload_generator.cpp
tests/cpp/workload_generator_test.cpp
include/helios/benchmark_suite.hpp
src/benchmark_suite.cpp
tests/cpp/benchmark_suite_test.cpp
apps/helios_sim/main.cpp
README.md
CMakeLists.txt
```

If this feels too large, split benchmark suite helpers into a later PR and start
with only `workload_generator`.

## PR 1: Workload Generator

### Goal

Create deterministic synthetic workloads.

### Proposed Types

```cpp
struct WorkloadGeneratorConfig {
  int job_count;
  unsigned int seed;
  SimTime max_arrival_time;
  Duration min_duration;
  Duration max_duration;
  ResourceAmount min_gpu_count;
  ResourceAmount max_gpu_count;
  ResourceAmount min_cpu_count;
  ResourceAmount max_cpu_count;
  ResourceAmount min_memory_mb;
  ResourceAmount max_memory_mb;

  bool operator==(const WorkloadGeneratorConfig&) const = default;
};
```

### Proposed Functions

```cpp
void validate_workload_generator_config(
    const WorkloadGeneratorConfig& config,
    Resources cluster_resources
);

Workload generate_workload(
    const WorkloadGeneratorConfig& config,
    Resources cluster_resources
);
```

### Required Tests

1. Valid config passes validation.
2. Negative job count is rejected.
3. Negative arrival bound is rejected.
4. Zero or negative duration lower bound is rejected.
5. Duration range with max less than min is rejected.
6. Zero GPU lower bound is rejected.
7. GPU range with max less than min is rejected.
8. GPU maximum above cluster capacity is rejected.
9. CPU maximum above cluster capacity is rejected.
10. Memory maximum above cluster capacity is rejected.
11. Generated workload has exactly `job_count` jobs.
12. Generated job IDs are unique and stable.
13. Generated arrivals are within range.
14. Generated durations are within range.
15. Generated resource requests are within range.
16. Every generated job fits the cluster resources.
17. Same config and seed produce identical workloads.
18. Different fixed seeds produce different workloads.
19. Empty generated workload is valid when `job_count == 0`.

## PR 2: Standard Generated Profiles

### Goal

Expose a small set of reusable generated workload profiles.

### Proposed Functions

```cpp
std::vector<DemoWorkloadSpec> generated_workload_specs();
Workload make_generated_workload(std::string_view name);
```

Alternative: extend existing demo workload functions if the API stays simple:

```cpp
demo_workload_specs()
make_demo_workload(name)
```

Recommended first choice: extend the existing demo workload API. It keeps CLI
usage simple.

### Proposed Profiles

```text
generated-small
generated-medium
generated-large
generated-bursty
generated-gpu-heavy
```

Profile intent:

1. `generated-small`: fast local sanity workload.
2. `generated-medium`: default comparison workload.
3. `generated-large`: scale and performance smoke workload.
4. `generated-bursty`: many jobs arrive in close time windows.
5. `generated-gpu-heavy`: many jobs request most of the cluster.

### Required Tests

1. Every generated profile appears in the workload list.
2. Every generated profile builds a valid workload.
3. Small profile has 25 jobs.
4. Medium profile has 100 jobs.
5. Large profile has 500 jobs.
6. Bursty profile contains repeated or tightly clustered arrival times.
7. GPU-heavy profile contains jobs requesting at least half the cluster GPUs.
8. Profiles are deterministic across repeated calls.

## PR 3: Benchmark Suite Comparison

### Goal

Run FIFO and SJF across several standardized workloads.

Keep this small. The suite should compare schedulers; it should not become a
full experiment framework.

### Proposed Types

```cpp
struct SchedulerSummary {
  std::string scheduler_name;
  std::string workload_name;
  int job_count;
  SimTime makespan;
  SimTime total_wait_time;
  double average_wait_time;

  bool operator==(const SchedulerSummary&) const = default;
};
```

### Proposed Functions

```cpp
SchedulerSummary summarize_result(
    std::string scheduler_name,
    std::string workload_name,
    const SimulationResult& result
);

std::vector<SchedulerSummary> compare_fifo_and_sjf(
    std::string_view workload_name,
    const Workload& workload,
    Resources cluster_resources
);
```

Use existing `run_fifo_simulation`, `run_sjf_simulation`, `total_wait_time`, and
`average_wait_time`.

### Required Tests

1. Summary copies scheduler name.
2. Summary copies workload name.
3. Summary includes job count.
4. Summary includes makespan.
5. Summary includes total wait time.
6. Summary includes average wait time.
7. FIFO and SJF comparison returns two rows.
8. FIFO and SJF comparison uses the same workload for both schedulers.
9. Comparison is deterministic.

## PR 4: CLI Support

### Goal

Make generated workloads easy to run from the terminal.

Keep existing:

```bash
./build/helios_sim --list-demo-workloads
./build/helios_sim --demo-fifo <workload-name>
./build/helios_sim --demo-sjf <workload-name>
./build/helios_sim --compare <workload-name>
```

After Phase 7, these should work:

```bash
./build/helios_sim --compare generated-small
./build/helios_sim --compare generated-medium
./build/helios_sim --compare generated-large
./build/helios_sim --compare generated-bursty
./build/helios_sim --compare generated-gpu-heavy
```

Optional suite command:

```bash
./build/helios_sim --compare-suite
```

Recommended output:

```text
workload,scheduler,job_count,makespan,total_wait_time,average_wait_time
generated-small,fifo,...
generated-small,sjf,...
generated-medium,fifo,...
generated-medium,sjf,...
```

Only add `--compare-suite` if it stays small. If it starts requiring file output,
configuration parsing, or many metric columns, defer it.

## Metrics For Phase 7

Keep metrics minimal:

1. job count,
2. makespan,
3. total wait time,
4. average wait time.

Do not add:

1. GPU utilization,
2. throughput,
3. slowdown,
4. fairness,
5. cost,
6. reward.

Those are useful later, but Phase 7 is about generating repeatable workloads and
running existing comparisons at larger scale.

## Testing Philosophy

Phase 7 tests should prove benchmark input reliability.

Good tests answer:

1. Can we generate valid workloads?
2. Are generated workloads deterministic?
3. Do generated jobs stay within configured ranges?
4. Do generated jobs fit the cluster?
5. Can FIFO and SJF run generated workloads without special cases?
6. Can generated profiles scale beyond hand-written examples?

Avoid tests that make broad statistical claims. For example, do not test that a
random distribution is "realistic." Test concrete deterministic outputs and
invariants.

## Reliability Risks And Mitigations

### Risk: Generated Jobs Cannot Fit

Mitigation:

1. Pass `cluster_resources` into the generator.
2. Validate config maximums against cluster resources.
3. Test every generated job with `fits(cluster_resources, job.request)`.

### Risk: Non-Deterministic Benchmarks

Mitigation:

1. Require explicit seeds.
2. Test same seed produces identical workload.
3. Keep standard profiles fixed.

### Risk: Workload Generator Becomes Too Realistic Too Soon

Mitigation:

1. Use simple bounded integer ranges.
2. Avoid trace ingestion.
3. Avoid distribution tuning beyond basic uniform generation.
4. Defer estimated versus actual runtime.

### Risk: CLI Becomes A Full Experiment Framework

Mitigation:

1. Keep generated workload names hard-coded in Phase 7.
2. Keep output columns small.
3. Defer config files and output directories.

### Risk: Large Tests Become Slow

Mitigation:

1. Keep unit tests mostly at 25 to 100 jobs.
2. Use one 500-job generated profile as a scale smoke test.
3. Do not add thousands of jobs until performance behavior is measured.

## Definition Of Done

Phase 7 is complete when:

1. `WorkloadGeneratorConfig` exists and is tested.
2. `generate_workload` exists and is tested.
3. Generated workloads are deterministic by seed.
4. Generated jobs stay within configured ranges.
5. Generated jobs fit the target cluster resources.
6. Standard generated profiles exist.
7. FIFO and SJF can compare generated profiles.
8. At least one generated profile has 100 or more jobs.
9. CLI can compare at least one generated workload.
10. README documents generated workload commands.
11. CMake builds all new targets.
12. CTest passes locally.
13. Formatting passes locally and in CI.
14. No Python bindings, RL, plotting, dashboards, or advanced schedulers have
    been introduced.

## Proposed Commit Sequence

PR 1:

1. `workload: add deterministic generator config`
2. `test: cover workload generator validation`
3. `test: cover deterministic workload generation`

PR 2:

4. `workload: add generated benchmark profiles`
5. `test: cover generated benchmark profiles`

PR 3:

6. `benchmark: add scheduler comparison summaries`
7. `test: cover generated FIFO vs SJF comparison`

PR 4:

8. `cli: expose generated workload comparisons`
9. `docs: document generated benchmark workloads`

## Suggested First Implementation Step

Start with the workload generator only.

Implement only:

```text
include/helios/workload_generator.hpp
src/workload_generator.cpp
tests/cpp/workload_generator_test.cpp
CMakeLists.txt
```

First coding goal:

1. Add `WorkloadGeneratorConfig`.
2. Add `validate_workload_generator_config`.
3. Add `generate_workload`.
4. Test config validation.
5. Test same seed gives the same workload.
6. Test generated jobs fit cluster resources.

Do not add CLI support in the first edit. Keep the first PR focused on the
generator core.

## Suggested Phase 8

After Phase 7, add another scheduler baseline or richer metrics.

Two good options:

1. Add random scheduling with deterministic seed control.
2. Add benchmark metrics like slowdown and throughput.

Python bindings should still wait until generated C++ benchmark comparisons are
stable.
