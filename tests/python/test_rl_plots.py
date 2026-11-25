"""Exercise RL plotting with explicit paired evaluation data, without training."""

import csv
from pathlib import Path
import subprocess
import sys

import matplotlib.pyplot as plt
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
from plot_rl_results import COLUMNS, load_learning_curves, load_results, plot_results, summarize_policy


@pytest.fixture
def evaluation_rows():
    rows = []
    for policy, seeds in (("fifo", [""]), ("ppo", [11, 22]), ("untrained_ppo", [11, 22])):
        for seed in seeds:
            for index, (profile, workload_seed) in enumerate((("small", 3), ("large", 4))):
                wait = 2 + index + (2 if seed == 22 else 0)
                rows.append(dict(zip(COLUMNS, (
                    policy, seed, profile, workload_seed, f"fingerprint-{index}",
                    2, 20, wait, wait * 2, wait * 2, wait * 2, -wait, 2,
                ))))
    return rows


def write_csv(path, rows, columns=COLUMNS):
    with path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=columns, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)
    return path


def test_generates_four_images(tmp_path, evaluation_rows):
    source = write_csv(tmp_path / "evaluation.csv", evaluation_rows)
    before = source.read_bytes()
    figures = plt.get_fignums()
    paths = plot_results(source, tmp_path / "plots")
    assert {path.name for path in paths} == {
        f"rl_wait_{name}.{extension}"
        for name in ("comparison", "by_profile") for extension in ("png", "svg")
    }
    for path in paths:
        assert path.stat().st_size > 1000
        if path.suffix == ".png":
            assert path.read_bytes().startswith(b"\x89PNG\r\n\x1a\n")
        else:
            text = path.read_text(encoding="utf-8")
            assert "lower is better" in text
            assert "not confidence intervals" in text
            assert "Paired workload episodes" in text
    assert source.read_bytes() == before
    assert plt.get_fignums() == figures


def test_aggregation_distinguishes_episode_and_seed_variation(tmp_path, evaluation_rows):
    rows = load_results(write_csv(tmp_path / "evaluation.csv", evaluation_rows))
    summary = summarize_policy([row for row in rows if row["policy"] == "ppo"])
    assert summary["mean"] == 3.5
    assert summary["seed_means"] == [2.5, 4.5]
    assert summary["episode_means"] == {("small", 3): 3, ("large", 4): 4}
    baseline = summarize_policy([row for row in rows if row["policy"] == "fifo"])
    assert baseline["mean"] == 2.5
    assert baseline["seed_means"] == []


@pytest.mark.parametrize("case", ["empty", "missing_column", "unpaired", "duplicate",
                                  "fingerprint", "job_count", "nan", "negative", "no_seed"])
def test_invalid_input_rejected_without_outputs(tmp_path, evaluation_rows, case):
    rows = evaluation_rows
    columns = COLUMNS
    if case == "empty":
        rows = []
    elif case == "missing_column":
        columns = tuple(column for column in COLUMNS if column != "average_wait_time")
    elif case == "unpaired":
        rows = rows[:-1]
    elif case == "duplicate":
        rows = rows + [rows[0]]
    elif case in ("fingerprint", "job_count"):
        rows[-1][case] = "different" if case == "fingerprint" else 3
    elif case in ("nan", "negative"):
        rows[-1]["average_wait_time"] = "nan" if case == "nan" else -1
    else:
        rows[-1]["training_seed"] = ""
    source = write_csv(tmp_path / "evaluation.csv", rows, columns)
    with pytest.raises(ValueError):
        plot_results(source, tmp_path / "plots")
    assert not (tmp_path / "plots").exists()


def test_baseline_only_single_episode(tmp_path, evaluation_rows):
    source = write_csv(tmp_path / "evaluation.csv", evaluation_rows[:1])
    assert len(plot_results(source, tmp_path / "plots")) == 4


def test_cli(tmp_path, evaluation_rows):
    source = write_csv(tmp_path / "evaluation.csv", evaluation_rows)
    script = Path(__file__).resolve().parents[2] / "scripts" / "plot_rl_results.py"
    result = subprocess.run(
        [sys.executable, str(script), "--input", str(source), "--output", str(tmp_path / "plots")],
        capture_output=True, text=True, check=True,
    )
    assert len(result.stdout.splitlines()) == 4
    assert (tmp_path / "plots" / "rl_wait_comparison.png").exists()


@pytest.fixture
def progress_logs(tmp_path):
    logs = {
        "seed_11": [(512, ""), (1024, -2.5), (1536, -1.25)],
        "seed_22": [(512, -3.0), (1024, -2.0)],
    }
    for seed, samples in logs.items():
        directory = tmp_path / seed
        directory.mkdir()
        write_csv(directory / "progress.csv", [
            {"time/total_timesteps": step, "rollout/ep_rew_mean": reward, "time/fps": 500}
            for step, reward in samples
        ], columns=("rollout/ep_rew_mean", "time/total_timesteps", "time/fps"))
    return logs


def test_learning_curve_uses_recorded_seed_samples(tmp_path, progress_logs):
    curves = load_learning_curves(tmp_path / "evaluation.csv")
    assert curves == {
        "seed_11": ([1024, 1536], [-2.5, -1.25]),
        "seed_22": ([512, 1024], [-3.0, -2.0]),
    }


def test_generates_learning_curve(tmp_path, evaluation_rows, progress_logs):
    source = write_csv(tmp_path / "evaluation.csv", evaluation_rows)
    before = {path: path.read_bytes() for path in tmp_path.glob("seed_*/progress.csv")}
    figures = plt.get_fignums()
    paths = plot_results(source, tmp_path / "plots")
    assert len(paths) == 6
    for extension in ("png", "svg"):
        path = tmp_path / "plots" / f"rl_learning_curve.{extension}"
        assert path in paths
        assert path.stat().st_size > 1000
    text = (tmp_path / "plots" / "rl_learning_curve.svg").read_text(encoding="utf-8")
    for label in ("100-episode rolling return", "higher is better", "Total training timesteps",
                  "Changing training workloads, not a held-out metric", "up to 100 recent episodes",
                  "seed_11", "seed_22"):
        assert label in text
    assert all(path.read_bytes() == content for path, content in before.items())
    assert plt.get_fignums() == figures


@pytest.mark.parametrize("state", ["missing", "empty", "header_only", "no_returns", "partial"])
def test_missing_or_unpopulated_progress_is_optional(tmp_path, evaluation_rows, state):
    source = write_csv(tmp_path / "evaluation.csv", evaluation_rows)
    if state != "missing":
        directory = tmp_path / "seed_11"
        directory.mkdir()
        contents = {
            "empty": "",
            "header_only": "rollout/ep_rew_mean,time/total_timesteps\n",
            "no_returns": "rollout/ep_rew_mean,time/total_timesteps\n,512\n",
            "partial": "rollout/ep_rew_mean,time/total_timesteps\n-2.5",
        }
        (directory / "progress.csv").write_text(contents[state], encoding="utf-8")
    assert load_learning_curves(source) == {}
    paths = plot_results(source, tmp_path / "plots")
    assert len(paths) == 4
    assert not (tmp_path / "plots" / "rl_learning_curve.png").exists()


@pytest.mark.parametrize("contents", [
    "total_timesteps,rollout/ep_rew_mean\n512,-2\n",
    "time/total_timesteps,rollout/ep_rew_mean\n512,nan\n",
    "time/total_timesteps,rollout/ep_rew_mean\n512,-2\n256,-1\n",
])
def test_invalid_progress_rejected_before_outputs(tmp_path, evaluation_rows, contents):
    source = write_csv(tmp_path / "evaluation.csv", evaluation_rows)
    directory = tmp_path / "seed_11"
    directory.mkdir()
    (directory / "progress.csv").write_text(contents, encoding="utf-8")
    with pytest.raises(ValueError, match="[Pp]rogress"):
        plot_results(source, tmp_path / "plots")
    assert not (tmp_path / "plots").exists()
