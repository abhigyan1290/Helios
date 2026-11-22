"""Run paired C++ benchmarks across seeds and save CSV plus execution metadata."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import csv
import hashlib
import io
import json
from pathlib import Path
import platform
import subprocess
import sys
import time


PROFILES = ["generated-small", "generated-medium", "generated-large",
            "generated-bursty", "generated-gpu-heavy"]
SCALE_PROFILES = ["scale-1000", "scale-5000", "scale-10000", "scale-12000"]


def run_pair(task):
    simulator, profile, seed = task
    started = time.perf_counter()
    completed = subprocess.run([str(simulator), "--compare-seeded", profile, str(seed)],
                               check=True, capture_output=True, text=True)
    rows = list(csv.DictReader(io.StringIO(completed.stdout)))
    if len(rows) != 2 or [row["scheduler"] for row in rows] != ["fifo", "sjf"]:
        raise ValueError(f"Invalid paired output: {profile}, seed {seed}")
    if any(row["workload"] != profile or int(row["seed"]) != seed for row in rows):
        raise ValueError("Benchmark labels do not match requested input")
    if rows[0]["job_count"] != rows[1]["job_count"]:
        raise ValueError("Scheduler completion counts differ")
    elapsed = time.perf_counter() - started
    print(f"{profile} seed={seed}: {rows[0]['job_count']} jobs per scheduler, "
          f"{elapsed:.2f}s", file=sys.stderr, flush=True)
    return rows, {"workload": profile, "seed": seed, "paired_wall_seconds": elapsed}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sim", type=Path, default=Path("build/release/helios_sim"))
    parser.add_argument("--seeds", type=int, default=10)
    parser.add_argument("--workers", type=int, default=1)
    parser.add_argument("--scale", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.seeds < 1 or args.workers < 1:
        parser.error("seeds and workers must be positive")
    simulator = args.sim.resolve(strict=True)
    profiles = SCALE_PROFILES if args.scale else PROFILES
    tasks = [(simulator, profile, seed) for profile in profiles for seed in range(args.seeds)]
    started = time.perf_counter()
    with ThreadPoolExecutor(max_workers=args.workers) as executor:
        results = list(executor.map(run_pair, tasks))
    rows = [row for pair, _ in results for row in pair]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    metadata = {
        "command": sys.argv,
        "platform": platform.platform(),
        "simulator": str(simulator),
        "simulator_sha256": hashlib.sha256(simulator.read_bytes()).hexdigest(),
        "seed_values": list(range(args.seeds)),
        "workers": args.workers,
        "cluster": {"gpu_count": 4, "cpu_count": 32, "memory_mb": 131072},
        "wait_metrics": {"p95_wait_time": "Nearest rank ceil(0.95 * job_count), one-based",
                         "max_wait_time": "Largest per-job wait time"},
        "total_wall_seconds": time.perf_counter() - started,
        "timing_note": "Paired process wall time includes both schedulers and generation; "
                       "concurrent workers share host resources. This is not simulated makespan.",
        "pairs": [timing for _, timing in results],
    }
    args.output.with_suffix(".metadata.json").write_text(
        json.dumps(metadata, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
