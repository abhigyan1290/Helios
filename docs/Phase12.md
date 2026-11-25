# Phase 12: First PPO Pilot

## Goal

Produce a reproducible, tested RL scheduling experiment using the existing C++
engine. A valid result includes PPO losing to classical baselines. Do not tune
on held-out results or describe synthetic simulation as real GPU execution.

## Step 1: Establish The Training Contract

Implemented in `python/helios/rl_env.py`, alongside the existing fixed-workload
`HeliosEnv`. Keep its public behavior unchanged.

1. Delegate all job starts, event advancement, and resource accounting to C++.
2. Allocate one pending slot per submitted job so the pilot never hides an
   arrived candidate outside its window. Allocate one running slot per GPU;
   every job requests at least one GPU.
3. Normalize resource requests by capacity and time features by fixed workload
   bounds. Omit IDs from features; retain FIFO-ordered action slots.
4. Expose estimates, elapsed time, arrived waiting jobs, and current resources.
   Do not expose actual durations, actual completion timestamps, or future
   arrival times. Terminal metrics and input archives may contain truth, but
   are never policy inputs. This is an API convention, not a security sandbox.
5. Mask empty and non-fitting selections. Mask waiting when no running job or
   future arrival exists. Future-job existence follows from the known episode
   job count minus pending, running, and completed counts; no hidden event time
   is inspected. Waiting remains allowed when events exist, even if jobs fit.
6. Reject invalid actions without changing simulator state.
7. Use reward `-pending_count * elapsed / (job_count * max_duration)`.
   For completed episodes its sum equals negative total wait divided by the
   same positive constant. Set PPO gamma to 1 to preserve this objective across
   zero-time starts and variable-length event steps. Reward scaling is a pilot
   convention, not a new scheduling metric.

## Step 2: Generate Reproducible Inputs

Implemented in `python/helios/rl_workloads.py`; this generates inputs only, not
simulation behavior.

1. Start with 16 jobs on 4 GPUs, 32 CPUs, and 131072 MB.
2. Vary short/long actual durations within 1..24, arrivals within 0..32, and
   GPU requests within 1..4. Keep every individual request feasible.
3. Use bursty, staggered, and mixed arrival profiles. Mixed samples include
   combinations of the other two; these are related synthetic distributions,
   not independent real-world trace families.
4. Generate estimates with independent uniform [0.6, 1.4) multipliers and round
   positive. Estimates sometimes equal actual duration by rounding. These are
   controlled synthetic errors, not a learned runtime predictor.
5. Sample training workload seeds only from [0, 100000). Fix evaluation seeds
   200000..200029 for each of three profiles before training.
6. Fingerprint every complete workload plus cluster resources with SHA-256.

## Step 3: Keep Baselines Comparable

Expose the existing C++ observation-only policies through `python/bindings.cpp`:
strict FIFO, estimated SJF, seeded random, and estimated EASY backfill.

All receive the same arrived jobs, estimates, resources, event semantics, and
submitted workload as PPO. FIFO deliberately restricts its own choice to the
head; that policy limitation is not imposed on PPO. Oracle SJF is excluded from
the main comparison because it has privileged actual runtime information.

## Step 4: Verify Before Training

1. Run the C++ regression suite and existing Python suite.
2. Test masked episodes on all profiles with multiple workload seeds.
3. Check space containment and deterministic seeded reset sequences.
4. Prove reward identity against C++ terminal metrics.
5. Test invalid-action non-mutation and absence of runtime truth in features.
6. Perform a small real PPO update and checkpoint save/reload round trip.
7. Verify baseline and PPO episodes have identical fingerprints and job counts.
8. Fail evaluation on truncation, mismatched inputs, incomplete jobs, or failure
   to terminate within the finite-event bound; never silently drop failures.

## Step 5: Run A Fixed-Budget Pilot

Use SB3-Contrib MaskablePPO, an SB3 PPO implementation with invalid-action masks.
Install RL dependencies as an optional extra, leaving C++-only builds intact.

Run three independent initialization/training seeds (0, 1, 2), each for 65536
transitions. Use a small two-layer 64-unit actor and critic, CPU execution, and
one Torch thread. Save initial and final checkpoints, episode monitor CSVs,
optimizer/rollout logs, package versions, source revision/dirty status, input
config, and actual training time/step counts.

Select the fixed final checkpoint. No checkpoint selection or hyperparameter
tuning uses the test set. Compare final policies to their untrained initial
policies as well as the four classical baselines.

## Step 6: Evaluate And Publish

1. Evaluate deterministically with action masks on 30 traces per profile.
2. Each policy completes the same 90 episodes of 16 jobs. PPO and its untrained
   reference each have three training-seed replicates; do not count these as
   additional distinct workloads.
3. Save per-episode CSV, complete evaluation input archives, and aggregate mean
   wait, P95 wait, and makespan. Report simulation time units, not seconds.
4. Plot paired workload outcomes, separate training-seed means/ranges, and
   training reward curves. Ranges are not confidence intervals. Rolling training
   reward is not held-out evaluation performance.
5. Document results and commands in the README, including unfavorable outcomes.
6. Keep this pilot separate from existing large-scale classical benchmark CSVs.

## Commands

```bash
python3 -m venv .venv
.venv/bin/python -m pip install torch --index-url https://download.pytorch.org/whl/cpu
.venv/bin/python -m pip install -e '.[rl]' -r tests/python/requirements.txt
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
.venv/bin/python -m pytest tests/python -q
.venv/bin/python scripts/train_ppo.py --output benchmarks/rl_pilot/replication \
  --timesteps 65536 --seeds 0 1 2 --eval-traces 30 --jobs 16
.venv/bin/python scripts/plot_rl_results.py \
  --input benchmarks/rl_pilot/replication/evaluation.csv \
  --output benchmarks/rl_pilot/replication/plots
```

Use a new output directory for each training run; prior results are not
overwritten. Exact training replay depends on software/platform versions.

To reproduce held-out metrics without training again:

```bash
.venv/bin/python scripts/evaluate_ppo.py \
  --experiment benchmarks/rl_pilot/2026-11-25 \
  --output benchmarks/rl_pilot/replay
```

## Pilot Completion Notes

The initial 2026-11-25 run completed all three fixed training budgets and all
900 evaluation episodes over 90 unique workloads. PPO mean wait was 35.26,
versus FIFO 49.97 and estimated SJF 24.94 simulation units. All trained seeds
improved over their respective initial checkpoints, but PPO did not beat SJF
and its mean per-trace P95 wait was worse than FIFO. See the README and saved
experiment CSVs for seed-level results and limitations.

Local validation passed 19 C++ tests and 188 Python tests (plus existing
subtests). A second evaluation loaded the saved checkpoints without training
and reproduced both evaluation and per-seed comparison CSVs byte for byte.
Python CI now installs the optional CPU RL stack and exercises a small PPO
update/checkpoint round trip; it does not run the full pilot on every PR.

The source archive contains the finalized reproduction code. Reporting,
provenance, and replay helpers were completed while the pilot ran; the sampler,
policy features, masking, reward, and PPO training configuration were unchanged.

## Follow-Up Gates

Before claiming research-grade RL results, add larger queues and cluster sizes,
additional independent training seeds, an explicit validation split for tuning,
out-of-distribution trace tests, confidence intervals with the appropriate
experimental unit, and more realistic runtime-estimation errors. The flat
fixed-slot policy is intentionally a pilot architecture, not a demonstrated
solution for 10000-job RL episodes. Fairness/starvation and tail latency remain
separate objectives; minimizing mean wait does not guarantee them.
