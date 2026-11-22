# Runtime API: Milestones 8-11

## Implemented Scope

| Milestone | Implementation | Main tests |
| --- | --- | --- |
| 8: Efficiency | Managed private state, in-place transitions, shared event stepping | `simulator_test`, runner parity tests, `scale_test` |
| 9: Runtime estimates | Optional estimates separate from actual duration; explicit synthetic estimate model | `runtime_estimates_test`, policy tests |
| 10: Baselines | Seeded random, estimated SJF, estimated EASY backfilling | `observation_policies_test`, CLI tests |
| 11: Interactive API | Reset, observe, submit one decision, wait for an event, inspect terminal metrics | `simulator_test`, interactive/batch parity tests |

Python bindings expose this interactive API. Gymnasium observations/actions,
rewards, and training remain later milestones. The interactive API does not
define an RL action space.

## Efficiency And Ownership

`Simulator` owns its state. It validates input jobs and total resources once at
reset, then maintains those invariants through checked transitions. Starting a
job checks arrival, resource fit, and timestamp overflow before changing state.
Completion releases the same request that was allocated.

The value-returning `SimulatorState` helpers remain available. Both they and the
managed simulator call the same internal start/completion implementations in
`src/simulator_transitions.hpp`; resource accounting is not duplicated.

Built-in batch runners use the managed engine. FIFO, estimated SJF, random, and
backfilling receive observations. The historical SJF runner explicitly requests
oracle observations. The legacy `run_simulation(..., SchedulerFn)` adapter keeps
support for callbacks receiving the full state, including their validation cost.

The measured bottlenecks were repeated full-state copies, repeated validation of
unchanged completed jobs, and allocation during those audits. The new path updates
private state in place and copies only the observation returned to a policy.
`validate()` remains available for expensive explicit audits and is used in tests.

The local Release comparison of the 1,000-job FIFO/SJF pair used three process
runs before and after the change. The original measurement had a median around
0.80 seconds; the managed engine measured around 0.036 seconds (roughly 22x faster).
Raw measurements are in `benchmarks/runtime_efficiency.csv`. Both checkpoints
produced byte-identical CSV output. Process startup, host load, compiler, and
allocator behavior affect these timings; this is a local comparison, not a
general performance guarantee. Observation copying, queue scanning, and vector
erasure still leave roughly quadratic work for heavily queued workloads.

## Actual And Estimated Duration

`Job::duration` remains the **actual execution duration** for compatibility.
`Job::estimated_duration` is an optional, positive duration supplied to a policy.
An absent estimate remains absent in the default observation; it never silently
falls back to actual duration.

`with_runtime_estimates(workload, percentage)` creates a copy with estimates:

```text
estimated_duration = ceil(actual_duration * percentage / 100)
```

The helper rejects nonpositive percentages and out-of-range results. It does not
alter actual duration, resource requests, arrivals, or IDs. Percentages 50, 100,
and 150 model systematic underestimation, exact knowledge, and overestimation.
Uniform scaling mostly preserves duration ordering; it is not a realistic noisy
runtime predictor. Callers can also supply unrelated per-job estimates directly.

`estimated_sjf_schedule` chooses the fitting arrived job with the smallest estimate,
then arrival time, then ID. It requires positive estimates for visible pending
jobs. Actual completion always uses `Job::duration`, even when estimates are wrong.
`run_sjf_simulation` and `sjf_schedule` remain explicitly oracle baselines.

Individual timestamps remain integer `SimTime` values. Completion-time overflow is
rejected. `total_wait_time` and `SchedulerSummary::total_wait_time` now use the
64-bit `AggregateTime` alias because a sum can exceed a single timestamp's range.
CSV column names and existing small benchmark values are unchanged by widening.

## Additional Baselines

`RandomScheduler(seed)` uniformly selects among arrived jobs fitting all available
resource dimensions. Its engine belongs to the policy instance. Repeating the
same calls with the same seed on the same standard library reproduces the sequence.
`reset(seed)` restarts that sequence. Empty or blocked queues return no decision.

EASY-style backfilling uses one estimated reservation for the FIFO head job:

1. Order arrived jobs by arrival time and ID. Start the head immediately if it fits.
2. Otherwise, project resource releases from running jobs using their estimates.
3. The first projected time at which the head fits is its reservation.
4. Scan later arrived jobs in FIFO order. A candidate must fit now and either
   finish by the reservation, or leave enough resources for the head at that time.
5. Return one candidate, or no decision. Recompute after each start or event.

Simultaneous estimated releases are grouped. All GPU, CPU, and memory constraints
participate. Pending estimates are required; running estimates are required when
forecasting a blocked head's reservation. Running jobs past their estimated end
are optimistically projected to release at `current_time + 1`, not immediately.

Reservations are **predictions**, not hard guarantees with inaccurate estimates.
An underestimated backfill job can delay the head. The simulator continues to
enforce resource capacity and actual runtime; it does not preempt or kill overruns.
This is a single-head EASY policy, not conservative reservations for every job.

Algorithm background: Tsafrir and Feitelson's
[The Dynamics of Backfilling](https://dants.github.io/papers/BackfillDyn06IISWC.pdf)
discusses estimate-based backfilling and reservation behavior. Helios explicitly
defines its no-kill overrun handling above; runtime estimates are not wall-time limits.

## Interactive Contract

```cpp
#include "helios/demo_workloads.hpp"
#include "helios/runtime_estimates.hpp"
#include "helios/schedulers/easy_backfill_scheduler.hpp"
#include "helios/simulator.hpp"

auto workload = helios::with_runtime_estimates(
    helios::make_demo_workload("gpu-contention"), 100);
helios::Simulator simulator(workload, helios::demo_cluster_resources());

while (!simulator.done()) {
  const auto observation = simulator.observe();
  const auto decision = helios::easy_backfill_schedule(observation);
  const auto transition = simulator.step(decision);
  // transition reports elapsed time, the started ID, completed IDs, and termination.
}
const auto result = simulator.result();
```

Methods:

| Method | Contract |
| --- | --- |
| Constructor / `reset(workload, resources)` | Validate and load jobs, reset clock to zero, clear history. Failed input validation preserves an existing episode. |
| `observe()` | Return a value snapshot containing arrived pending jobs, running-job estimates and elapsed times, clock, resources, completed count, and termination. |
| `step(select_job(id))` | Start exactly one arrived fitting job at current time; do not advance time or start another job. |
| `step(no_schedule_decision())` | Advance to the next arrival or actual completion; complete all jobs due there, ordered by ID. Do not select a job. |
| `done()` | True only when no pending/future or running jobs remain. |
| `result()` | Return completed per-job metrics once terminal; otherwise throw. |
| `validate()` | Explicit full-state audit for debugging/tests. |

Completions release resources before a new observation exposes arrivals at the
same time. An empty workload is terminal at time zero. A successful start has
zero elapsed time, so a caller can start multiple jobs at one timestamp. Waiting
may return at an arrival where no job fits; another wait is then allowed if a
future event exists. Waiting without a future event while jobs remain throws
instead of creating an infinite loop. Stepping a terminal episode throws.

Negative IDs are `std::invalid_argument`. Missing, future, already-running,
completed, or non-fitting selections are `std::runtime_error`. Rejected actions
preserve simulator state. Invalid jobs/resources/estimates use
`std::invalid_argument`; an individually impossible resource request uses
`std::runtime_error`. Reset and action validation happen before state mutation.

Default observations contain neither actual durations nor predicted actual
completion timestamps, future IDs, future arrivals, or future queue sizes.
Two otherwise identical episodes with different hidden durations and future jobs
produce identical initial observations. Observations are independent value copies.

`observe_oracle()` and `diagnostic_state()` are explicit privileged C++ access for
legacy baselines/debugging. They expose ground truth. A future Python/RL binding
must expose `observe()` to agents and must not expose those diagnostic methods as
part of the agent observation path. This API boundary is not a security sandbox
against arbitrary native C++ callers.

## Commands And Verification

```bash
./build/helios_sim --demo-step fifo-blocking
./build/helios_sim --demo-random generated-medium 42
./build/helios_sim --demo-estimated-sjf generated-medium 50
./build/helios_sim --demo-backfill generated-medium 150
./build/helios_sim --compare-baselines generated-large 100 42
```

The five-policy comparison labels oracle and estimated SJF separately and records
the estimate percentage and random seed. It does not change the historical paired
FIFO/SJF suite schema. `benchmarks/additional_baselines.csv` is a single-profile
example, not a multi-seed evaluation of the new policies.

Tests cover invalid actions and reset, exact/missing/wrong estimates, integer
overflow, seed reproducibility, ties, resource constraints in all dimensions,
backfill reservation boundaries, overrun behavior, batch/interactive parity, and
observation leakage. The optional scale test checks all five policies through
12,000 jobs and independently replays resource usage over every result.

All C++ test assertions stay active in Release builds. AddressSanitizer and
UndefinedBehaviorSanitizer can be run with:

```bash
cmake -S . -B build/sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build/sanitize -j 4
ctest --test-dir build/sanitize --output-on-failure
```

The next milestone is the Gymnasium contract, followed by rewards and PPO training.
