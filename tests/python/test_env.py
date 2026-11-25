"""Tests for the Gymnasium scheduling contract and C++ parity."""

import numpy as np
import pytest
from gymnasium.utils.env_checker import check_env

from helios import (HeliosEnv, Job, Resources, Simulator, Workload, no_schedule_decision,
                    select_job, total_wait_time)


def make_job(job_id, arrival=0, duration=2, gpu=1, estimate=None):
    return Job(job_id, arrival, duration, Resources(gpu, 1, 1), estimate)


def make_env(jobs, **kwargs):
    return HeliosEnv(Workload(jobs), Resources(2, 4, 10), **kwargs)


def assert_observations_equal(left, right):
    assert left.keys() == right.keys()
    for key in left:
        np.testing.assert_array_equal(left[key], right[key])


def test_checker_and_seeded_reset():
    env = make_env([make_job(1), make_job(2, arrival=3)], max_pending=1)
    check_env(env, skip_render_check=True)
    first, _ = env.reset(seed=12)
    second, _ = env.reset(seed=12)
    assert_observations_equal(first, second)


def test_empty_workload_and_step_requires_reset():
    env = make_env([], max_pending=2)
    with pytest.raises(RuntimeError, match="reset"):
        env.step(2)
    obs, info = env.reset()
    assert info == {} and env.observation_space.contains(obs)
    assert env.result().makespan == 0
    with pytest.raises(RuntimeError, match="terminated"):
        env.step(2)


def test_hand_calculated_reward_and_direct_simulator_parity():
    workload = Workload([make_job(1, duration=3), make_job(2, arrival=1, duration=1)])
    cluster = Resources(1, 4, 10)
    env = HeliosEnv(workload, cluster, max_pending=2, wait_penalty=2)
    direct = Simulator(workload, cluster)
    obs, _ = env.reset(seed=42)
    initial = {key: value.copy() for key, value in obs.items()}
    total_reward = 0
    for action, decision, expected_reward, expected_time, done in [
        (0, select_job(1), 0, 0, False),
        (2, no_schedule_decision(), 0, 1, False),
        (2, no_schedule_decision(), -4, 3, False),
        (0, select_job(2), 0, 3, False),
        (2, no_schedule_decision(), 0, 4, True),
    ]:
        obs, reward, terminated, truncated, _ = env.step(action)
        transition = direct.step(decision)
        total_reward += reward
        assert reward == expected_reward and obs["time"][0] == expected_time
        assert terminated == done == transition.terminated and not truncated
        assert env.observation_space.contains(obs)
        assert obs["time"][0] == direct.observe().current_time
        assert obs["completed_count"][0] == direct.observe().completed_job_count
    assert total_reward == -2 * total_wait_time(direct.result()) == -4
    assert env.result().makespan == direct.result().makespan == 4
    assert [(j.job_id, j.start_time, j.completion_time) for j in env.result().jobs] == [
        (j.job_id, j.start_time, j.completion_time) for j in direct.result().jobs]
    assert initial["pending_jobs"][0, 0] == 1 and initial["time"][0] == 0


def test_masks_overflow_and_hidden_wait_is_counted():
    env = make_env([make_job(1, duration=3, gpu=2), make_job(2), make_job(3)],
                   max_pending=1, max_running=1)
    obs, _ = env.reset()
    assert obs["pending_count"].tolist() == [3]
    assert obs["pending_jobs"][0, 0] == 1
    assert obs["pending_mask"].tolist() == [1]
    assert obs["schedulable_mask"].tolist() == [1]
    obs, _, _, _, _ = env.step(0)
    assert obs["pending_count"].tolist() == [2]
    assert obs["pending_jobs"][0, 0] == 2
    assert obs["schedulable_mask"].tolist() == [0]
    obs, reward, _, _, _ = env.step(1)
    assert reward == -6 and obs["schedulable_mask"].tolist() == [1]


def test_invalid_actions_are_rejected_without_state_change():
    env = make_env([make_job(1, gpu=2), make_job(2)], max_pending=3)
    initial, _ = env.reset()
    for action in (-1, 4, 2, 3):
        with pytest.raises((ValueError, RuntimeError)):
            env.step(action)
        assert_observations_equal(initial, env._observation(env._sim.observe()))
    after, *_ = env.step(0)
    with pytest.raises(RuntimeError):
        env.step(0)
    assert_observations_equal(after, env._observation(env._sim.observe()))


def test_hidden_information_and_missing_estimate():
    a = make_env([make_job(1, duration=2)])
    b = make_env([make_job(1, duration=9), make_job(2, arrival=8)])
    first, _ = a.reset()
    second, _ = b.reset()
    assert_observations_equal(first, second)
    assert first["pending_jobs"][0, 5] == 0


def test_running_window_and_simultaneous_completion():
    env = make_env([make_job(1), make_job(2)], max_pending=2, max_running=1)
    env.reset()
    env.step(0)
    obs, *_ = env.step(0)
    assert obs["running_count"].tolist() == [2]
    assert obs["running_jobs"][0, 0] == 1
    obs, _, done, _, _ = env.step(2)
    assert done and obs["completed_count"].tolist() == [2]


@pytest.mark.parametrize("kwargs", [{"max_pending": 0}, {"max_running": 0},
                                    {"wait_penalty": -1}, {"wait_penalty": float("nan")}])
def test_invalid_configuration(kwargs):
    with pytest.raises(ValueError):
        make_env([], **kwargs)
