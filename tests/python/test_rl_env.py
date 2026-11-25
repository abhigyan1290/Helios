"""Masked training API, C++ parity, and policy-information boundaries."""

import importlib.util
import csv
from pathlib import Path

import numpy as np
import pytest

from helios import Job, Resources, Workload, total_wait_time
from helios.rl_env import PilotEnv
from helios.rl_workloads import PilotConfig


@pytest.mark.parametrize("profile", ["bursty", "staggered", "mixed"])
def test_masked_episodes_terminate_and_rewards_equal_total_wait(profile):
    env = PilotEnv(profile=profile)
    rng = np.random.default_rng(9)
    for seed in range(10):
        obs, _ = env.reset(options={"workload_seed": seed})
        total_reward = 0
        for _ in range(4 * env.config.job_count + 10):
            assert env.observation_space.contains(obs)
            valid = np.flatnonzero(env.action_masks())
            assert valid.size
            action = int(rng.choice(valid))
            obs, reward, done, truncated, _ = env.step(action)
            assert not truncated
            total_reward += reward
            if done:
                break
        else:
            pytest.fail("finite episode failed to terminate")
        assert len(env.result().jobs) == env.config.job_count
        assert total_reward == pytest.approx(-total_wait_time(env.result()) / env.reward_scale)
        assert not env.action_masks().any()


def test_reset_seed_and_explicit_workload_seed_replay():
    env = PilotEnv()
    first, info = env.reset(seed=18)
    sequence = [env.reset()[1]["fingerprint"] for _ in range(3)]
    again, replay_info = env.reset(seed=18)
    np.testing.assert_array_equal(first, again)
    assert replay_info == info
    assert sequence == [env.reset()[1]["fingerprint"] for _ in range(3)]
    _, test_info = env.reset(options={"workload_seed": 200000})
    assert test_info["workload_seed"] == 200000
    assert all(0 <= env.reset()[1]["workload_seed"] < 100000 for _ in range(10))


def test_invalid_actions_preserve_simulator_state():
    env = PilotEnv()
    before, _ = env.reset()
    invalid = np.flatnonzero(~env.action_masks())
    assert invalid.size
    for action in [int(invalid[0]), -1, env.action_space.n]:
        with pytest.raises(ValueError):
            env.step(action)
        np.testing.assert_array_equal(before, env._features())


def test_runtime_truth_and_future_arrival_times_do_not_enter_features(monkeypatch):
    import helios.rl_env as module

    def make(duration, future):
        return Workload([Job(0, 0, duration, Resources(1, 1, 1), 7),
                         Job(1, future, 3, Resources(1, 1, 1), 4)])

    cfg = PilotConfig(job_count=2)
    monkeypatch.setattr(module, "make_pilot_workload", lambda *args: make(2, 10))
    left = PilotEnv(cfg)
    first, _ = left.reset(seed=3)
    monkeypatch.setattr(module, "make_pilot_workload", lambda *args: make(20, 30))
    right = PilotEnv(cfg)
    second, _ = right.reset(seed=3)
    np.testing.assert_array_equal(first, second)
    np.testing.assert_array_equal(left.action_masks(), right.action_masks())
    left.step(0)
    right.step(0)
    np.testing.assert_array_equal(left._features(), right._features())


def test_no_dead_end_wait_when_no_future_events(monkeypatch):
    import helios.rl_env as module
    monkeypatch.setattr(module, "make_pilot_workload", lambda *args: Workload([
        Job(0, 0, 2, Resources(1, 1, 1), 2)]))
    env = PilotEnv(PilotConfig(job_count=1))
    env.reset()
    assert env.action_masks().tolist() == [True, False]
    env.step(0)
    assert env.action_masks().tolist() == [False, True]
    assert env.step(1)[2]


def test_training_adapter_validation():
    with pytest.raises(ValueError):
        PilotEnv(PilotConfig(job_count=0))
    env = PilotEnv()
    with pytest.raises(RuntimeError):
        env.action_masks()
    with pytest.raises(ValueError):
        env.reset(options={"unsupported": True})


def test_checkpoint_roundtrip_and_paired_evaluation(tmp_path):
    pytest.importorskip("sb3_contrib")
    import torch
    from sb3_contrib import MaskablePPO

    path = Path(__file__).resolve().parents[2] / "scripts" / "train_ppo.py"
    spec = importlib.util.spec_from_file_location("helios_train_ppo", path)
    training = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(training)
    torch.set_num_threads(1)
    env = PilotEnv(PilotConfig(job_count=4))
    model = training.make_model(env, 3)
    # One tiny rollout verifies actual gradient updates, not just predict().
    model.n_steps = 32
    model.batch_size = 16
    model._setup_model()
    initial = training.run_episode(env, workload_seed=200000, model=model)
    model.learn(32)
    model.save(tmp_path / "model")
    restored = MaskablePPO.load(tmp_path / "model", device="cpu")
    left = training.run_episode(env, workload_seed=200000, model=model)
    right = training.run_episode(env, workload_seed=200000, model=restored)
    assert left == right
    baseline = training.run_episode(env, workload_seed=200000,
                                    baseline=training._core.fifo_policy)
    assert baseline["fingerprint"] == left["fingerprint"]
    assert baseline["job_count"] == left["job_count"] == 4
    rows = [{"policy": policy, "training_seed": seed, "profile": "mixed", **result}
            for policy, seed, result in [("ppo", 3, left), ("untrained_ppo", 3, initial),
                                         ("fifo", "", baseline)]]
    training.write_seed_comparisons(tmp_path, rows)
    with (tmp_path / "paired_comparisons.csv").open() as stream:
        comparisons = list(csv.DictReader(stream))
    assert {row["reference"] for row in comparisons} == {"fifo", "untrained_ppo"}
    assert all(int(row["wins"]) + int(row["ties"]) + int(row["losses"]) == 1
               for row in comparisons)
    fifo = next(row for row in comparisons if row["reference"] == "fifo")
    assert float(fifo["mean_wait_difference"]) == pytest.approx(
        left["average_wait_time"] - baseline["average_wait_time"])
