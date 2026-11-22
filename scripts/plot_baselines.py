"""Plot the saved five-scheduler benchmark comparison."""

import argparse
import csv
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


POLICIES = {
    "fifo": "FIFO",
    "oracle_sjf": "Oracle SJF",
    "estimated_sjf": "Estimated SJF",
    "random": "Random",
    "easy_backfill": "EASY backfill",
}
METRICS = (
    ("average_wait_time", "Average wait"),
    ("p95_wait_time", "P95 wait"),
    ("max_wait_time", "Maximum wait"),
    ("makespan", "Makespan"),
)


def plot_baselines(csv_path: Path, output_dir: Path) -> None:
    with csv_path.open(newline="", encoding="utf-8") as source:
        rows = list(csv.DictReader(source))
    by_policy = {row["scheduler"]: row for row in rows}
    if len(rows) != len(POLICIES) or set(by_policy) != set(POLICIES):
        raise ValueError("Expected exactly one row for each of the five schedulers")
    for field in ("workload", "job_count", "estimate_percent", "random_seed"):
        if len({row[field] for row in rows}) != 1:
            raise ValueError(f"Schedulers must share the same {field}")

    fig, axes = plt.subplots(2, 2, figsize=(12, 9), layout="constrained")
    colors = ("#277DA8", "#CB6040", "#E9A03B", "#7B6BA8", "#4C956C")
    for ax, (metric, title) in zip(axes.flat, METRICS):
        values = [float(by_policy[policy][metric]) for policy in POLICIES]
        bars = ax.barh(list(POLICIES.values()), values, color=colors)
        ax.bar_label(bars, labels=[f"{value:,.1f}" for value in values], padding=4)
        ax.invert_yaxis()
        ax.set_xlim(0, max(values) * 1.22 if max(values) else 1)
        ax.set_title(title, loc="left", weight="bold")
        ax.set_xlabel("Simulation time units")
        ax.set_axisbelow(True)
        ax.grid(axis="x", alpha=0.2)
        ax.spines[["top", "right"]].set_visible(False)
    example = rows[0]
    fig.suptitle("Scheduler comparison", fontsize=17, weight="bold")
    fig.supxlabel(
        f"{example['workload']} · {example['job_count']} jobs · single workload run\n"
        f"Runtime estimates: {example['estimate_percent']}% of actual · "
        f"random scheduler seed: {example['random_seed']}", fontsize=10
    )
    output_dir.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_dir / "all_scheduler_comparison.png", dpi=180)
    plt.rcParams["svg.hashsalt"] = "helios"
    fig.savefig(output_dir / "all_scheduler_comparison.svg", metadata={"Date": None})
    plt.close(fig)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_path", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("docs/images"))
    args = parser.parse_args()
    plot_baselines(args.csv_path, args.output_dir)
