"""Plot FIFO/SJF suite CSV without running or duplicating simulation logic."""

import argparse
import csv
import io
import math
from pathlib import Path
import statistics

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

METRICS = ("makespan", "total_wait_time", "average_wait_time", "p95_wait_time", "max_wait_time")


def load_results(csv_path: Path):
    with csv_path.open(newline="", encoding="utf-8") as source:
        reader = csv.DictReader(source)
        required = {"workload", "scheduler", "job_count", *METRICS}
        missing = required - set(reader.fieldnames or [])
        if missing:
            raise ValueError(f"CSV is missing columns {sorted(missing)}; regenerate benchmark CSV")
        rows = list(reader)
    profiles = list(dict.fromkeys(row["workload"] for row in rows))
    if not profiles:
        raise ValueError("Suite CSV contains no workloads")
    results = {}
    for row in rows:
        seed = int(row.get("seed", 0))
        key = (row["workload"], seed, row["scheduler"])
        if key in results:
            raise ValueError(f"Duplicate suite row: {key}")
        if row["scheduler"] not in ("fifo", "sjf"):
            raise ValueError("Unsupported scheduler")
        if int(row["job_count"]) <= 0:
            raise ValueError("Expected a positive completion count")
        for metric in METRICS:
            value = float(row[metric])
            if not math.isfinite(value) or value < 0:
                raise ValueError(f"Invalid metric: {metric}")
        if not math.isclose(float(row["average_wait_time"]),
                            int(row["total_wait_time"]) / int(row["job_count"]),
                            rel_tol=1e-6, abs_tol=1e-9):
            raise ValueError("Average wait does not match total wait / job count")
        maximum = float(row["max_wait_time"])
        if float(row["p95_wait_time"]) > maximum or maximum > float(row["total_wait_time"]):
            raise ValueError("Expected P95 wait <= maximum wait <= total wait")
        if float(row["average_wait_time"]) > maximum + 1e-9:
            raise ValueError("Average wait cannot exceed maximum wait")
        results[key] = row
    summaries = []
    for profile in profiles:
        seeds = sorted({seed for name, seed, _ in results if name == profile})
        counts = set()
        for seed in seeds:
            for policy in ("fifo", "sjf"):
                if (profile, seed, policy) not in results:
                    raise ValueError(f"Missing paired row: {profile}, seed {seed}, {policy}")
                counts.add(int(results[(profile, seed, policy)]["job_count"]))
        if len(counts) != 1:
            raise ValueError(f"Completion counts differ for {profile}")
        for policy in ("fifo", "sjf"):
            summary = {"workload": profile, "scheduler": policy,
                       "job_count": next(iter(counts)),
                       "seed_count": len(seeds)}
            for metric in METRICS:
                values = [float(results[(profile, seed, policy)][metric]) for seed in seeds]
                summary[f"{metric}_mean"] = statistics.mean(values)
                summary[f"{metric}_stddev"] = statistics.stdev(values) if len(values) > 1 else 0.0
            summaries.append(summary)
    return profiles, summaries


def plot_suite(csv_path: Path, output_dir: Path, prefix: str = "") -> None:
    profiles, summaries = load_results(csv_path)
    results = {(row["workload"], row["scheduler"]): row for row in summaries}
    summary_path = csv_path.with_name(csv_path.stem + "_summary.csv")
    with summary_path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=list(summaries[0]))
        writer.writeheader()
        writer.writerows(summaries)

    output_dir.mkdir(parents=True, exist_ok=True)
    plt.rcParams.update({"font.size": 11, "axes.spines.top": False,
                         "axes.spines.right": False, "svg.hashsalt": "helios"})
    for metric, title, ylabel in (
        ("average_wait_time", "Average Queue Wait: FIFO vs SJF", "Simulation time units"),
        ("p95_wait_time", "P95 Queue Wait: FIFO vs SJF", "Simulation time units"),
        ("max_wait_time", "Maximum Queue Wait: FIFO vs SJF", "Simulation time units"),
        ("makespan", "Workload Completion: FIFO vs SJF", "Simulation time units"),
    ):
        fig, ax = plt.subplots(figsize=(11, 5.5), layout="constrained")
        for offset, policy, color, label in (
            (-0.2, "fifo", "#277DA8", "Strict FIFO"),
            (0.2, "sjf", "#CB6040", "Oracle SJF"),
        ):
            values = [results[(profile, policy)][f"{metric}_mean"] for profile in profiles]
            spread = [results[(profile, policy)][f"{metric}_stddev"] for profile in profiles]
            bars = ax.bar([i + offset for i in range(len(profiles))], values,
                          yerr=spread, capsize=4, width=0.36, color=color, label=label)
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
        seed_counts = sorted({row["seed_count"] for row in summaries})
        seed_label = "/".join(str(count) for count in seed_counts)
        fig.supxlabel(f"Means across {seed_label} seeds; error bars: sample standard deviation\n"
                      "4 GPUs / 32 CPUs / 128 GiB memory", fontsize=9)
        fig.savefig(output_dir / f"{prefix}{metric}.png", dpi=180)
        svg = io.StringIO()
        fig.savefig(svg, format="svg", metadata={"Date": None})
        (output_dir / f"{prefix}{metric}.svg").write_text(
            "\n".join(line.rstrip() for line in svg.getvalue().splitlines()) + "\n",
            encoding="utf-8")
        plt.close(fig)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_path", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("docs/images"))
    parser.add_argument("--prefix", default="")
    args = parser.parse_args()
    plot_suite(args.csv_path, args.output_dir, args.prefix)
