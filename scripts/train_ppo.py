#!/usr/bin/env python3
"""Train a bounded PPO pilot and evaluate paired, held-out workloads."""

import argparse
import csv
import hashlib
import importlib.metadata
import json
from dataclasses import asdict
from pathlib import Path
import platform
import subprocess
import tarfile
import time

import numpy as np
import torch
from sb3_contrib import MaskablePPO
from stable_baselines3.common.logger import configure
from stable_baselines3.common.monitor import Monitor

from helios import _core
from helios.rl_env import PilotEnv
from helios.rl_workloads import PilotConfig, make_pilot_workload, workload_fingerprint


FIELDS = ["policy", "training_seed", "profile", "workload_seed", "fingerprint", "job_count",
          "makespan", "average_wait_time", "p95_wait_time", "max_wait_time", "total_wait_time",
          "episode_return", "steps"]


def save_provenance(output):
    """Archive the dirty source tree too: HEAD alone cannot reproduce this run."""
    root = Path(__file__).resolve().parents[1]
    names = subprocess.check_output([
        "git", "ls-files", "-z", "--cached", "--others", "--exclude-standard", "--",
        "CMakeLists.txt", "pyproject.toml", "AGENTS.md", "include", "src", "apps",
        "python", "scripts", "tests", ".github/workflows",
    ], cwd=root).decode().split("\0")
    hashes = {}
    with tarfile.open(output / "source.tar.gz", "w:gz") as archive:
        for name in sorted(set(filter(None, names))):
            path = root / name
            if path.is_file():
                hashes[name] = hashlib.sha256(path.read_bytes()).hexdigest()
                archive.add(path, arcname=name, recursive=False)
    (output / "source_sha256.json").write_text(json.dumps(hashes, indent=2) + "\n")
    packages = sorted(f"{item.metadata['Name']}=={item.version}"
                      for item in importlib.metadata.distributions())
    (output / "installed_packages.txt").write_text("\n".join(packages) + "\n")


def make_model(env, seed):
    # Undiscounted queue-area reward equals negative total wait up to a constant.
    return MaskablePPO(
        "MlpPolicy", env, seed=seed, device="cpu", learning_rate=3e-4,
        n_steps=512, batch_size=128, n_epochs=6, gamma=1.0, gae_lambda=0.95,
        ent_coef=0.01, policy_kwargs={"net_arch": {"pi": [64, 64], "vf": [64, 64]}},
        verbose=0,
    )


def run_episode(env, *, workload_seed, model=None, baseline=None):
    obs, info = env.reset(options={"workload_seed": workload_seed})
    cumulative_reward = 0.0
    # Each job can start and finish once, and arrive once. Every legal wait
    # advances to an event; the guard catches regressions rather than censoring.
    for steps in range(1, 4 * env.config.job_count + 10):
        if model is not None:
            action, _ = model.predict(obs, deterministic=True, action_masks=env.action_masks())
            action = int(action)
        else:
            view = env.observe()
            decision = baseline(view)
            action = env.config.job_count
            if decision.job_id is not None:
                action = next(i for i, job in enumerate(view.pending_jobs)
                              if job.id == decision.job_id)
        obs, reward, terminated, truncated, _ = env.step(action)
        cumulative_reward += reward
        if truncated:
            raise RuntimeError("evaluation must not silently drop truncated episodes")
        if terminated:
            result = env.result()
            total_wait = _core.total_wait_time(result)
            if not np.isclose(cumulative_reward, -total_wait / env.reward_scale):
                raise AssertionError("reward and terminal total wait disagree")
            if len(result.jobs) != env.config.job_count:
                raise AssertionError("not all submitted jobs completed")
            return {**info, "job_count": len(result.jobs), "makespan": result.makespan,
                    "average_wait_time": _core.average_wait_time(result),
                    "p95_wait_time": _core.p95_wait_time(result),
                    "max_wait_time": _core.max_wait_time(result), "total_wait_time": total_wait,
                    "episode_return": cumulative_reward, "steps": steps}
    raise RuntimeError("episode exceeded the finite-event bound")


def write_seed_comparisons(output, rows):
    """Pair by trace, keeping training seeds separate rather than pseudo-replicating."""
    groups = {}
    for row in rows:
        groups.setdefault((row["policy"], row["training_seed"]), {})[
            (row["profile"], row["workload_seed"])] = row
    comparisons = []
    for (policy, seed), episodes in groups.items():
        if policy != "ppo":
            continue
        references = [key for key in groups if key[0] not in {"ppo", "untrained_ppo"}]
        references.append(("untrained_ppo", seed))
        for reference in references:
            baseline = groups[reference]
            if set(episodes) != set(baseline):
                raise AssertionError("unpaired evaluation groups")
            deltas = []
            for key, row in episodes.items():
                if row["fingerprint"] != baseline[key]["fingerprint"]:
                    raise AssertionError("paired workloads have different fingerprints")
                deltas.append(row["average_wait_time"] - baseline[key]["average_wait_time"])
            baseline_mean = float(np.mean([row["average_wait_time"] for row in baseline.values()]))
            delta = float(np.mean(deltas))
            comparisons.append({
                "training_seed": seed, "reference": reference[0], "episodes": len(deltas),
                "ppo_mean_wait": float(np.mean([r["average_wait_time"] for r in episodes.values()])),
                "reference_mean_wait": baseline_mean, "mean_wait_difference": delta,
                "percent_mean_wait_reduction": -100 * delta / baseline_mean if baseline_mean else "",
                "wins": sum(value < 0 for value in deltas),
                "ties": sum(value == 0 for value in deltas),
                "losses": sum(value > 0 for value in deltas),
            })
    if comparisons:
        with (output / "paired_comparisons.csv").open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(comparisons[0]))
            writer.writeheader()
            writer.writerows(comparisons)


def evaluate(output, config, seeds, traces, *, checkpoint_root=None):
    checkpoint_root = checkpoint_root or output
    models = []
    for seed in seeds:
        for label, filename in [("untrained_ppo", "initial"), ("ppo", "final")]:
            models.append((label, seed, MaskablePPO.load(checkpoint_root / f"seed_{seed}" / filename,
                                                        device="cpu")))
    baselines = {"fifo": _core.fifo_policy,
                 "estimated_sjf": _core.estimated_sjf_schedule,
                 "easy_backfill": _core.easy_backfill_schedule}
    rows = []
    with (output / "evaluation_workloads.jsonl").open("w") as workload_file:
        for profile in ["bursty", "staggered", "mixed"]:
            env = PilotEnv(config, profile)
            for index in range(traces):
                workload_seed = 200000 + index
                workload = make_pilot_workload(workload_seed, config, profile)
                fingerprint = workload_fingerprint(workload, config.resources)
                workload_file.write(json.dumps({
                    "profile": profile, "seed": workload_seed, "fingerprint": fingerprint,
                    "jobs": [{"id": j.id, "arrival_time": j.arrival_time,
                              "duration": j.duration, "estimated_duration": j.estimated_duration,
                              "request": [j.request.gpu_count, j.request.cpu_count,
                                          j.request.memory_mb]} for j in workload.jobs],
                }) + "\n")
                policies = [(name, "", None, policy) for name, policy in baselines.items()]
                policies.append(("random", "", None, _core.RandomScheduler(workload_seed)))
                policies.extend((name, seed, model, None) for name, seed, model in models)
                for name, seed, model, baseline in policies:
                    row = run_episode(env, workload_seed=workload_seed,
                                      model=model, baseline=baseline)
                    if row["fingerprint"] != fingerprint:
                        raise AssertionError("policies did not receive identical workloads")
                    rows.append({"policy": name, "training_seed": seed, "profile": profile, **row})
            env.close()
    with (output / "evaluation.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    summary = []
    for name in sorted({row["policy"] for row in rows}):
        selected = [row for row in rows if row["policy"] == name]
        summary.append({"policy": name, "episodes": len(selected),
                        **{key: float(np.mean([row[key] for row in selected]))
                           for key in ["average_wait_time", "p95_wait_time", "makespan"]}})
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    write_seed_comparisons(output, rows)
    print(json.dumps(summary, indent=2), flush=True)
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--timesteps", type=int, default=65536)
    parser.add_argument("--seeds", type=int, nargs="+", default=[0, 1, 2])
    parser.add_argument("--eval-traces", type=int, default=30, help="held-out traces per profile")
    parser.add_argument("--jobs", type=int, default=16)
    args = parser.parse_args()
    if args.timesteps < 1 or args.eval_traces < 1 or not args.seeds or len(set(args.seeds)) != len(args.seeds):
        parser.error("positive budgets and unique training seeds are required")
    if min(args.seeds) < 0 or max(args.seeds) >= 2**32:
        parser.error("training seeds must be in [0, 2**32)")
    if args.output.exists() and (not args.output.is_dir() or any(args.output.iterdir())):
        parser.error("output directory must be empty; previous experiments are never overwritten")
    args.output.mkdir(parents=True, exist_ok=True)
    save_provenance(args.output)
    torch.set_num_threads(1)
    config = PilotConfig(job_count=args.jobs)
    manifest = {
        "config": asdict(config), "training_seeds": args.seeds,
        "requested_timesteps_per_seed": args.timesteps,
        "training_workload_seed_range": [0, 100000],
        "test_workload_seeds": list(range(200000, 200000 + args.eval_traces)),
        "profiles": ["bursty", "staggered", "mixed"], "device": "cpu", "torch_threads": 1,
        "python": platform.python_version(),
        "versions": {name: importlib.metadata.version(name) for name in
                     ["torch", "stable-baselines3", "sb3-contrib", "gymnasium", "numpy"]},
        "git_head": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        "git_status": subprocess.check_output(["git", "status", "--short"], text=True),
        "command": " ".join(__import__("sys").argv),
        "checkpoint_selection": "fixed final budget, no test-set selection or tuning",
        "objective": "negative total wait / (job_count * max_duration), gamma=1",
        "runs": [],
    }
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    for seed in args.seeds:
        directory = args.output / f"seed_{seed}"
        directory.mkdir()
        env = Monitor(PilotEnv(config), filename=str(directory / "episodes"))
        model = make_model(env, seed)
        model.set_logger(configure(str(directory), ["csv"]))
        model.save(directory / "initial")
        start = time.perf_counter()
        print(f"Training seed {seed}: {args.timesteps} requested transitions", flush=True)
        model.learn(total_timesteps=args.timesteps)
        seconds = time.perf_counter() - start
        model.save(directory / "final")
        manifest["runs"].append({"seed": seed, "actual_timesteps": model.num_timesteps,
                                  "wall_seconds": seconds})
        (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        env.close()
        print(f"Finished seed {seed}: {model.num_timesteps} transitions in {seconds:.1f}s", flush=True)
    evaluate(args.output, config, args.seeds, args.eval_traces)


if __name__ == "__main__":
    main()
