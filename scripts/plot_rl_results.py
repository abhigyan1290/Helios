"""Plot saved, paired RL evaluation episodes without running the simulator."""

import argparse
import csv
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
import numpy as np


COLUMNS = (
    "policy", "training_seed", "profile", "workload_seed", "fingerprint",
    "job_count", "makespan", "average_wait_time", "p95_wait_time",
    "max_wait_time", "total_wait_time", "episode_return", "steps",
)


def load_results(input_path: Path) -> list[dict]:
    """Require the same held-out episodes for every policy/training-seed group."""
    with Path(input_path).open(newline="", encoding="utf-8") as source:
        reader = csv.DictReader(source)
        missing = set(COLUMNS) - set(reader.fieldnames or [])
        if missing:
            raise ValueError(f"Evaluation CSV is missing columns: {sorted(missing)}")
        rows = list(reader)
    if not rows:
        raise ValueError("Evaluation CSV contains no episodes")

    groups = {}
    workloads = {}
    for row in rows:
        if any(row.get(field) is None for field in COLUMNS):
            raise ValueError("Incomplete evaluation row")
        for field in ("policy", "profile", "fingerprint"):
            if not row[field].strip():
                raise ValueError(f"Empty {field}")
        try:
            row["training_seed"] = (int(row["training_seed"])
                                    if row["training_seed"].strip() else None)
            row["workload_seed"] = int(row["workload_seed"])
            for field in ("job_count", "steps"):
                row[field] = int(row[field])
                if row[field] < (1 if field == "job_count" else 0):
                    raise ValueError(f"Invalid {field}")
            for field in COLUMNS[6:12]:
                row[field] = float(row[field])
                if not np.isfinite(row[field]) or (field != "episode_return" and row[field] < 0):
                    raise ValueError(f"Invalid {field}")
        except (ValueError, TypeError) as error:
            raise ValueError(f"Invalid numeric evaluation value: {error}") from error
        if row["policy"] in ("ppo", "untrained_ppo") and row["training_seed"] is None:
            raise ValueError("PPO rows require a training_seed")
        episode = (row["profile"], row["workload_seed"])
        identity = (row["fingerprint"], row["job_count"])
        if episode in workloads and workloads[episode] != identity:
            raise ValueError(f"Mismatched held-out workload: {episode}")
        workloads[episode] = identity
        group = (row["policy"], row["training_seed"])
        episodes = groups.setdefault(group, set())
        if episode in episodes:
            raise ValueError(f"Duplicate evaluation episode: {group}, {episode}")
        episodes.add(episode)
    expected = set(workloads)
    if any(episodes != expected for episodes in groups.values()):
        raise ValueError("Missing paired held-out episodes in a policy/training-seed group")
    for policy in {row["policy"] for row in rows}:
        seeds = {seed for name, seed in groups if name == policy}
        if None in seeds and len(seeds) > 1:
            raise ValueError(f"Mixed empty and populated training_seed for {policy}")
    return rows


def summarize_policy(rows: list[dict]) -> dict:
    """Keep the all-row mean, seed means, and paired episode means distinct."""
    seed_values = {}
    episode_values = {}
    for row in rows:
        if row["training_seed"] is not None:
            seed_values.setdefault(row["training_seed"], []).append(row["average_wait_time"])
        episode = (row["profile"], row["workload_seed"])
        episode_values.setdefault(episode, []).append(row["average_wait_time"])
    return {
        "mean": float(np.mean([row["average_wait_time"] for row in rows])),
        "seed_means": [float(np.mean(seed_values[seed])) for seed in sorted(seed_values)],
        "episode_means": {key: float(np.mean(values)) for key, values in episode_values.items()},
    }


def _draw_comparison(ax, rows: list[dict], policies: list[str]) -> None:
    summaries = [summarize_policy([row for row in rows if row["policy"] == policy])
                 for policy in policies]
    positions = np.arange(len(policies))
    ax.bar(positions, [summary["mean"] for summary in summaries], width=0.6,
           color="#C9DCE5", edgecolor="#527384", zorder=2)
    for episode in summaries[0]["episode_means"]:
        ax.plot(positions, [summary["episode_means"][episode] for summary in summaries],
                color="#777777", alpha=0.35, linewidth=0.8, marker=".", zorder=3)
    for position, summary in zip(positions, summaries):
        means = summary["seed_means"]
        if means:
            offsets = np.linspace(-0.12, 0.12, len(means)) if len(means) > 1 else [0]
            ax.vlines(position, min(means), max(means), color="#B44C36", linewidth=2, zorder=4)
            ax.scatter(position + np.asarray(offsets), means, marker="D", s=24,
                       color="#B44C36", zorder=5)
    labels = [f"{policy}\n{sum(row['policy'] == policy for row in rows)} episodes"
              for policy in policies]
    ax.set_xticks(positions, labels, rotation=20, ha="right")
    ax.set_ylabel("Average wait (simulation time units; lower is better)")
    ax.set_ylim(bottom=0)
    ax.set_axisbelow(True)
    ax.grid(axis="y", alpha=0.2)
    ax.spines[["top", "right"]].set_visible(False)


def load_learning_curves(input_path: Path) -> dict[str, tuple[list[int], list[float]]]:
    """Read optional sibling SB3 logs, preserving each seed's recorded steps."""
    curves = {}
    for path in sorted(Path(input_path).parent.glob("seed_*/progress.csv")):
        steps, returns = [], []
        with path.open(newline="", encoding="utf-8") as source:
            reader = csv.DictReader(source)
            required = {"time/total_timesteps", "rollout/ep_rew_mean"}
            if not reader.fieldnames:
                continue
            if not required <= set(reader.fieldnames):
                raise ValueError(f"Progress CSV {path} is missing columns: "
                                 f"{sorted(required - set(reader.fieldnames))}")
            for row in reader:
                # SB3 may not have episode returns yet, or may be writing a row.
                if any(value is None for value in row.values()):
                    continue
                step_text = row["time/total_timesteps"]
                return_text = row["rollout/ep_rew_mean"]
                if not step_text.strip() or not return_text.strip():
                    continue
                try:
                    step, reward = int(step_text), float(return_text)
                except ValueError as error:
                    raise ValueError(f"Invalid progress value in {path}") from error
                if step < 0 or not np.isfinite(reward) or (steps and step <= steps[-1]):
                    raise ValueError(f"Invalid or unordered progress value in {path}")
                steps.append(step)
                returns.append(reward)
        if steps:
            curves[path.parent.name] = (steps, returns)
    return curves


def _plot_learning_curves(curves: dict, output_dir: Path) -> list[Path]:
    fig, ax = plt.subplots(figsize=(10, 5.5), layout="constrained")
    paths = []
    try:
        for seed, (steps, returns) in curves.items():
            ax.plot(steps, returns, label=seed, marker=".", linewidth=1.5)
        ax.set_title("PPO learning curves: training workloads", loc="left", weight="bold")
        ax.set_xlabel("Total training timesteps")
        ax.set_ylabel("100-episode rolling return (higher is better)")
        ax.spines[["top", "right"]].set_visible(False)
        ax.grid(alpha=0.2)
        ax.legend(title="Training seed", frameon=False)
        fig.supxlabel("Changing training workloads, not a held-out metric.\n"
                      "SB3 rollout/ep_rew_mean: up to 100 recent episodes; no extra smoothing.",
                      fontsize=9)
        for extension in ("png", "svg"):
            path = output_dir / f"rl_learning_curve.{extension}"
            fig.savefig(path, dpi=180,
                        metadata={"Date": None} if extension == "svg" else None)
            paths.append(path)
    finally:
        plt.close(fig)
    return paths


def plot_results(input_path: Path, output_dir: Path) -> list[Path]:
    """Write comparisons and optional learning curves; return produced paths."""
    rows = load_results(input_path)
    curves = load_learning_curves(input_path)
    policies = list(dict.fromkeys(row["policy"] for row in rows))
    profiles = list(dict.fromkeys(row["profile"] for row in rows))
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    paths = []
    legend = [
        Line2D([], [], color="#527384", linewidth=8, label="Mean across all evaluation rows"),
        Line2D([], [], color="#777777", marker=".",
               label="Paired workload episodes (mean over training seeds)"),
    ]
    if any(row["training_seed"] is not None for row in rows):
        legend.append(Line2D([], [], color="#B44C36", marker="D",
                             label="Training-seed means and min-max range (not confidence intervals)"))
    with plt.rc_context({"font.size": 10, "svg.hashsalt": "helios-rl"}):
        for name, subsets in (
            ("rl_wait_comparison", [("Held-out average wait", rows)]),
            ("rl_wait_by_profile", [(profile, [row for row in rows if row["profile"] == profile])
                                    for profile in profiles]),
        ):
            fig, axes = plt.subplots(len(subsets), 1, squeeze=False,
                                     figsize=(max(9, len(policies) * 1.5), 4.5 * len(subsets) + 1.2),
                                     layout="constrained")
            try:
                for ax, (title, subset) in zip(axes.flat, subsets):
                    _draw_comparison(ax, subset, policies)
                    ax.set_title(title, loc="left", weight="bold")
                fig.suptitle("RL pilot: paired held-out workloads", weight="bold")
                fig.supxlabel("Workload variation and training-seed variation are distinct; "
                              "no confidence intervals shown.", fontsize=9)
                axes[0, 0].legend(handles=legend, frameon=False, fontsize=8,
                                  loc="lower left", bbox_to_anchor=(0, 1.12))
                for extension in ("png", "svg"):
                    path = output_dir / f"{name}.{extension}"
                    fig.savefig(path, dpi=180,
                                metadata={"Date": None} if extension == "svg" else None)
                    paths.append(path)
            finally:
                plt.close(fig)
        if curves:
            paths.extend(_plot_learning_curves(curves, output_dir))
    return paths


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True, dest="input_path")
    parser.add_argument("--output", type=Path, required=True, dest="output_dir")
    args = parser.parse_args()
    try:
        paths = plot_results(args.input_path, args.output_dir)
    except (ValueError, OSError) as error:
        parser.error(str(error))
    for path in paths:
        print(path)


if __name__ == "__main__":
    main()
