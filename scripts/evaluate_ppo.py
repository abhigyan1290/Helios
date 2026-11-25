#!/usr/bin/env python3
"""Replay held-out PPO evaluation from a completed experiment without training."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

from helios.rl_workloads import PilotConfig


def _load_training_module():
    path = Path(__file__).resolve().with_name("train_ppo.py")
    spec = importlib.util.spec_from_file_location("helios_replay_training", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _read_experiment(experiment):
    manifest_path = experiment / "manifest.json"
    raw = manifest_path.read_bytes()
    manifest = json.loads(raw)
    if not isinstance(manifest, dict):
        raise ValueError("manifest.json must contain an object")
    config_data = manifest.get("config")
    required = {"job_count", "gpu_count", "max_duration", "max_arrival"}
    if not isinstance(config_data, dict) or not required <= config_data.keys():
        raise ValueError("manifest config must include all four PilotConfig fields")
    config = PilotConfig(**config_data)
    if config.job_count < 1:
        raise ValueError("replay evaluation requires at least one job")

    seeds = manifest.get("training_seeds")
    if (not isinstance(seeds, list) or not seeds
            or any(type(seed) is not int or not 0 <= seed < 2**32 for seed in seeds)
            or len(set(seeds)) != len(seeds)):
        raise ValueError("training_seeds must be unique integers in [0, 2**32)")
    test_seeds = manifest.get("test_workload_seeds")
    if (not isinstance(test_seeds, list) or not test_seeds
            or any(type(seed) is not int for seed in test_seeds)
            or test_seeds != list(range(200000, 200000 + len(test_seeds)))):
        raise ValueError("test_workload_seeds must be contiguous integers starting at 200000")

    runs = manifest.get("runs")
    if (not isinstance(runs, list)
            or any(not isinstance(run, dict) or type(run.get("seed")) is not int
                   for run in runs)):
        raise ValueError("runs must contain completed training run entries with integer seeds")
    completed = [run["seed"] for run in runs]
    if len(set(completed)) != len(completed):
        raise ValueError("runs contains duplicate completed training seeds")
    missing = sorted(set(seeds) - set(completed))
    if missing:
        raise ValueError(f"experiment is incomplete; missing completed runs for seeds {missing}")
    for seed in seeds:
        for label in ("initial", "final"):
            checkpoint = experiment / f"seed_{seed}" / f"{label}.zip"
            if not checkpoint.is_file():
                raise ValueError(f"missing checkpoint: {checkpoint}")
    provenance = {
        "experiment": str(experiment),
        "input_manifest_path": str(manifest_path),
        "input_manifest_sha256": hashlib.sha256(raw).hexdigest(),
        "input_manifest": manifest,
        "torch_threads": 1,
    }
    return config, seeds, len(test_seeds), provenance


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--experiment", type=Path, required=True,
                        help="completed training experiment containing manifest.json and checkpoints")
    parser.add_argument("--output", type=Path, required=True,
                        help="new or empty replay output directory")
    args = parser.parse_args(argv)
    experiment = args.experiment.resolve()
    output = args.output.resolve()
    try:
        if output.exists() and (not output.is_dir() or any(output.iterdir())):
            raise ValueError("output directory must be empty; existing results are never overwritten")
        config, seeds, traces, provenance = _read_experiment(experiment)
    except (OSError, ValueError, TypeError) as exc:
        parser.error(str(exc))

    try:
        training = _load_training_module()
        training.torch.set_num_threads(1)
        output.mkdir(parents=True, exist_ok=True)
        with (output / "replay_provenance.json").open("x") as stream:
            json.dump(provenance, stream, indent=2)
            stream.write("\n")
        training.evaluate(output, config, seeds, traces, checkpoint_root=experiment)
    except (ImportError, OSError, RuntimeError, ValueError) as exc:
        parser.exit(1, f"replay evaluation failed: {exc}\n")


if __name__ == "__main__":
    main()
