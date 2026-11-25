"""Replay CLI validation and delegation, without loading or training PPO models."""

from dataclasses import asdict
import hashlib
import importlib.util
import json
from pathlib import Path
from types import SimpleNamespace

import pytest

from helios.rl_workloads import PilotConfig


@pytest.fixture
def replay():
    path = Path(__file__).resolve().parents[2] / "scripts" / "evaluate_ppo.py"
    spec = importlib.util.spec_from_file_location("helios_test_replay", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


@pytest.fixture
def experiment(tmp_path):
    root = tmp_path / "experiment"
    root.mkdir()
    manifest = {
        "config": asdict(PilotConfig(job_count=4)),
        "training_seeds": [7, 2],
        "test_workload_seeds": [200000, 200001, 200002],
        "runs": [{"seed": 2, "actual_timesteps": 512},
                 {"seed": 7, "actual_timesteps": 512}],
    }
    (root / "manifest.json").write_text(json.dumps(manifest))
    for seed in manifest["training_seeds"]:
        directory = root / f"seed_{seed}"
        directory.mkdir()
        for label in ("initial", "final"):
            (directory / f"{label}.zip").write_bytes(b"fake checkpoint")
    return root


@pytest.fixture
def fake_training(replay, monkeypatch):
    calls = []

    def evaluate(output, config, seeds, traces, *, checkpoint_root):
        calls.append(("evaluate", output, config, seeds, traces, checkpoint_root))
        assert (output / "replay_provenance.json").is_file()
        (output / "evaluation.csv").write_text("policy,fingerprint\nfifo,test\n")

    module = SimpleNamespace(
        torch=SimpleNamespace(set_num_threads=lambda n: calls.append(("threads", n))),
        evaluate=evaluate,
    )
    monkeypatch.setattr(replay, "_load_training_module", lambda: module)
    return calls


def rewrite_manifest(experiment, change):
    path = experiment / "manifest.json"
    manifest = json.loads(path.read_text())
    change(manifest)
    path.write_text(json.dumps(manifest))


@pytest.mark.parametrize("existing_empty", [False, True])
def test_replays_manifest_and_records_provenance(replay, experiment, tmp_path,
                                                fake_training, existing_empty):
    before = {p.relative_to(experiment): p.read_bytes()
              for p in experiment.rglob("*") if p.is_file()}
    output = tmp_path / "replay"
    if existing_empty:
        output.mkdir()
    replay.main(["--experiment", str(experiment), "--output", str(output)])
    assert fake_training == [
        ("threads", 1),
        ("evaluate", output, PilotConfig(job_count=4), [7, 2], 3, experiment),
    ]
    provenance = json.loads((output / "replay_provenance.json").read_text())
    raw = (experiment / "manifest.json").read_bytes()
    assert provenance == {
        "experiment": str(experiment.resolve()),
        "input_manifest_path": str((experiment / "manifest.json").resolve()),
        "input_manifest_sha256": hashlib.sha256(raw).hexdigest(),
        "input_manifest": json.loads(raw),
        "torch_threads": 1,
    }
    assert (output / "evaluation.csv").is_file()
    assert before == {p.relative_to(experiment): p.read_bytes()
                      for p in experiment.rglob("*") if p.is_file()}


@pytest.mark.parametrize("field,value,message", [
    ("training_seeds", [], "training_seeds"),
    ("training_seeds", [2, 2], "training_seeds"),
    ("training_seeds", [True], "training_seeds"),
    ("training_seeds", [-1], "training_seeds"),
    ("training_seeds", [2**32], "training_seeds"),
    ("training_seeds", ["2"], "training_seeds"),
    ("training_seeds", None, "training_seeds"),
    ("test_workload_seeds", [], "contiguous"),
    ("test_workload_seeds", [200001], "contiguous"),
    ("test_workload_seeds", [200000, 200002], "contiguous"),
    ("test_workload_seeds", [200001, 200000], "contiguous"),
    ("test_workload_seeds", [200000, 200000], "contiguous"),
    ("test_workload_seeds", [200000.0], "contiguous"),
    ("test_workload_seeds", None, "contiguous"),
    ("runs", [], "incomplete"),
    ("runs", [{"seed": 7}], "incomplete"),
    ("runs", [{"seed": 7}, {"seed": 7}], "duplicate"),
    ("runs", [{"seed": True}], "integer seeds"),
    ("runs", [{}], "integer seeds"),
    ("runs", None, "completed training"),
    ("config", {}, "all four"),
    ("config", None, "all four"),
    ("config", {**asdict(PilotConfig()), "job_count": 0}, "at least one job"),
    ("config", {**asdict(PilotConfig()), "max_duration": 25}, "max_duration"),
])
def test_rejects_invalid_manifest_before_loading_evaluator(
        replay, experiment, tmp_path, monkeypatch, capsys, field, value, message):
    rewrite_manifest(experiment, lambda manifest: manifest.update({field: value}))
    monkeypatch.setattr(replay, "_load_training_module",
                        lambda: pytest.fail("invalid inputs must not load PPO"))
    output = tmp_path / "replay"
    with pytest.raises(SystemExit) as error:
        replay.main(["--experiment", str(experiment), "--output", str(output)])
    assert error.value.code == 2
    assert message in capsys.readouterr().err
    assert not output.exists()


@pytest.mark.parametrize("label", ["initial", "final"])
def test_missing_checkpoint_fails_clearly(replay, experiment, tmp_path,
                                         fake_training, capsys, label):
    missing = experiment / "seed_2" / f"{label}.zip"
    missing.unlink()
    output = tmp_path / "replay"
    with pytest.raises(SystemExit) as error:
        replay.main(["--experiment", str(experiment), "--output", str(output)])
    assert error.value.code == 2
    assert f"missing checkpoint: {missing}" in capsys.readouterr().err
    assert fake_training == []
    assert not output.exists()


@pytest.mark.parametrize("kind", ["file", "directory", "experiment"])
def test_refuses_nonempty_output(replay, experiment, tmp_path, fake_training, capsys, kind):
    output = tmp_path / "replay"
    if kind == "file":
        output.write_text("keep")
    elif kind == "directory":
        output.mkdir()
        (output / "keep.txt").write_text("keep")
    else:
        output = experiment
    with pytest.raises(SystemExit) as error:
        replay.main(["--experiment", str(experiment), "--output", str(output)])
    assert error.value.code == 2
    assert "output directory must be empty" in capsys.readouterr().err
    assert fake_training == []
    if kind == "file":
        assert output.read_text() == "keep"
    elif kind == "directory":
        assert (output / "keep.txt").read_text() == "keep"


@pytest.mark.parametrize("contents", ["not json", "[]", "{}"])
def test_malformed_manifest(replay, experiment, tmp_path, fake_training, contents):
    (experiment / "manifest.json").write_text(contents)
    with pytest.raises(SystemExit) as error:
        replay.main(["--experiment", str(experiment), "--output", str(tmp_path / "replay")])
    assert error.value.code == 2
    assert fake_training == []


def test_missing_manifest(replay, tmp_path, fake_training, capsys):
    with pytest.raises(SystemExit) as error:
        replay.main(["--experiment", str(tmp_path / "missing"),
                     "--output", str(tmp_path / "replay")])
    assert error.value.code == 2
    assert "manifest.json" in capsys.readouterr().err
    assert fake_training == []


def test_evaluation_failure_is_reported(replay, experiment, tmp_path, monkeypatch, capsys):
    def fail(*args, **kwargs):
        raise RuntimeError("invalid checkpoint archive")

    monkeypatch.setattr(replay, "_load_training_module", lambda: SimpleNamespace(
        torch=SimpleNamespace(set_num_threads=lambda n: None), evaluate=fail))
    with pytest.raises(SystemExit) as error:
        replay.main(["--experiment", str(experiment), "--output", str(tmp_path / "replay")])
    assert error.value.code == 1
    assert "replay evaluation failed: invalid checkpoint archive" in capsys.readouterr().err
