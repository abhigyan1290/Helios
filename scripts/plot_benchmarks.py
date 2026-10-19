"""Plot FIFO/SJF suite CSV without running or duplicating simulation logic."""

import argparse
import csv
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def plot_suite(csv_path: Path, output_dir: Path) -> None:
    with csv_path.open(newline="", encoding="utf-8") as source:
        rows = list(csv.DictReader(source))
    profiles = list(dict.fromkeys(row["workload"] for row in rows))
    if not profiles:
        raise ValueError("Suite CSV contains no workloads")
    results = {}
    for row in rows:
        key = (row["workload"], row["scheduler"])
        if key in results:
            raise ValueError(f"Duplicate suite row: {key}")
        results[key] = row
    for profile in profiles:
        fifo, sjf = (results[(profile, policy)] for policy in ("fifo", "sjf"))
        if int(fifo["job_count"]) != int(sjf["job_count"]):
            raise ValueError(f"Completion counts differ for {profile}")

    output_dir.mkdir(parents=True, exist_ok=True)
    plt.rcParams.update({"font.size": 11, "axes.spines.top": False,
                         "axes.spines.right": False, "svg.hashsalt": "helios"})
    for metric, title, ylabel in (
        ("average_wait_time", "Average Queue Wait: FIFO vs SJF", "Simulation time units"),
        ("makespan", "Workload Completion: FIFO vs SJF", "Simulation time units"),
    ):
        fig, ax = plt.subplots(figsize=(11, 5.5), layout="constrained")
        for offset, policy, color, label in (
            (-0.2, "fifo", "#277DA8", "Strict FIFO"),
            (0.2, "sjf", "#CB6040", "Oracle SJF"),
        ):
            values = [float(results[(profile, policy)][metric]) for profile in profiles]
            bars = ax.bar([i + offset for i in range(len(profiles))], values,
                          width=0.36, color=color, label=label)
            ax.bar_label(bars, labels=[f"{v:,.1f}" for v in values], padding=4, fontsize=9)
        ax.set_xticks(range(len(profiles)),
                      [f"{p.removeprefix('generated-')}\n"
                       f"{results[(p, 'fifo')]['job_count']} jobs" for p in profiles])
        ax.set_title(title, loc="left", pad=18, weight="bold")
        ax.set_ylabel(ylabel)
        ax.set_ylim(0, ax.get_ylim()[1] * 1.18)
        ax.set_axisbelow(True)
        ax.grid(axis="y", alpha=0.2)
        ax.legend(frameon=False, loc="upper left")
        fig.supxlabel("Fixed seeds, one run per profile; 4 GPUs / 32 CPUs / 128 GiB memory",
                      fontsize=9)
        fig.savefig(output_dir / f"{metric}.png", dpi=180)
        fig.savefig(output_dir / f"{metric}.svg", metadata={"Date": None})
        plt.close(fig)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_path", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("docs/images"))
    args = parser.parse_args()
    plot_suite(args.csv_path, args.output_dir)
