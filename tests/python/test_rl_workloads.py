"""Input generation contracts for paired RL pilot evaluations."""

from dataclasses import FrozenInstanceError, asdict
import json

import numpy as np
import pytest

from helios._core import Job, Resources, Workload
from helios.rl_workloads import PilotConfig, make_pilot_workload, workload_fingerprint


def snapshot(workload):
    return [
        (j.id, j.arrival_time, j.duration, j.request.gpu_count,
         j.request.cpu_count, j.request.memory_mb, j.estimated_duration)
        for j in workload.jobs
    ]


def test_config_defaults_serialization_and_frozen_state():
    config = PilotConfig()
    assert json.loads(json.dumps(asdict(config))) == {
        "job_count": 16, "gpu_count": 4, "max_duration": 24, "max_arrival": 32,
    }
    assert config.horizon == 416
    assert (config.resources.gpu_count, config.resources.cpu_count,
            config.resources.memory_mb) == (4, 32, 131072)
    with pytest.raises(FrozenInstanceError):
        config.job_count = 8
    config.resources.gpu_count = 99
    assert config.resources.gpu_count == 4


@pytest.mark.parametrize("profile", ["bursty", "staggered", "mixed"])
def test_seeded_reproducibility_and_variation(profile):
    config = PilotConfig()
    first = snapshot(make_pilot_workload(200000, config, profile))
    assert first == snapshot(make_pilot_workload(200000, config, profile))
    assert first != snapshot(make_pilot_workload(200001, config, profile))


@pytest.mark.parametrize("profile", ["bursty", "staggered", "mixed"])
@pytest.mark.parametrize("gpus", [1, 2, 4])
def test_bounds_resource_feasibility_and_noisy_estimates(profile, gpus):
    config = PilotConfig(gpu_count=gpus)
    for seed in range(20):
        jobs = make_pilot_workload(seed, config, profile).jobs
        assert sorted(j.id for j in jobs) == list(range(config.job_count))
        assert min(j.arrival_time for j in jobs) == 0
        assert all(0 <= j.arrival_time <= config.max_arrival for j in jobs)
        assert all(1 <= j.duration <= config.max_duration for j in jobs)
        assert any(j.duration <= 6 for j in jobs)
        assert any(j.duration >= 12 for j in jobs)
        assert any(j.estimated_duration != j.duration for j in jobs)
        for job in jobs:
            assert 1 <= job.request.gpu_count <= gpus
            assert 2 * job.request.gpu_count <= job.request.cpu_count <= 8 * job.request.gpu_count
            assert 4096 * job.request.gpu_count <= job.request.memory_mb <= 32768 * job.request.gpu_count
            assert job.request.cpu_count <= config.resources.cpu_count
            assert job.request.memory_mb <= config.resources.memory_mb
            assert job.estimated_duration >= 1
            assert abs(job.estimated_duration - job.duration) <= 0.4 * job.duration + 0.5


def test_profiles_have_distinct_arrivals():
    config = PilotConfig(job_count=64)
    bursty = {j.arrival_time for j in make_pilot_workload(42, config, "bursty").jobs}
    staggered = {j.arrival_time for j in make_pilot_workload(42, config, "staggered").jobs}
    assert bursty <= {0, 16, 32}
    assert len(staggered) > len(bursty)
    mixed = {
        tuple(j.arrival_time for j in make_pilot_workload(seed, config, "mixed").jobs)
        for seed in range(12)
    }
    assert len(mixed) > 1


@pytest.mark.parametrize("profile", ["bursty", "staggered", "mixed"])
@pytest.mark.parametrize("count", [0, 1, 2])
def test_empty_single_and_reduced_bounds(profile, count):
    config = PilotConfig(job_count=count, gpu_count=1, max_duration=1, max_arrival=0)
    jobs = make_pilot_workload(0, config, profile).jobs
    assert len(jobs) == count
    assert all(j.arrival_time == 0 and j.duration == 1 for j in jobs)


@pytest.mark.parametrize("kwargs", [
    {"job_count": -1}, {"gpu_count": 0}, {"gpu_count": 5},
    {"max_duration": 0}, {"max_duration": 25},
    {"max_arrival": -1}, {"max_arrival": 33}, {"job_count": 2**31},
])
def test_invalid_config_bounds(kwargs):
    with pytest.raises(ValueError):
        PilotConfig(**kwargs)


@pytest.mark.parametrize("field", ["job_count", "gpu_count", "max_duration", "max_arrival"])
@pytest.mark.parametrize("value", [True, 1.5, "2", None])
def test_invalid_config_types(field, value):
    with pytest.raises(TypeError):
        PilotConfig(**{field: value})


@pytest.mark.parametrize("seed, error", [(-1, ValueError), (True, TypeError), (1.5, TypeError)])
def test_invalid_seed(seed, error):
    with pytest.raises(error):
        make_pilot_workload(seed, PilotConfig())


def test_invalid_profile_and_config():
    with pytest.raises(ValueError):
        make_pilot_workload(0, PilotConfig(), "unknown")
    with pytest.raises(TypeError):
        make_pilot_workload(0, {})


def test_local_rng_does_not_change_global_numpy_state():
    before = np.random.get_state()
    make_pilot_workload(42, PilotConfig())
    after = np.random.get_state()
    assert before[0] == after[0]
    np.testing.assert_array_equal(before[1], after[1])
    assert before[2:] == after[2:]


def test_fingerprint_is_stable_and_order_independent():
    config = PilotConfig()
    workload = make_pilot_workload(42, config)
    fingerprint = workload_fingerprint(workload, config.resources)
    assert len(fingerprint) == 64
    assert int(fingerprint, 16) >= 0
    assert fingerprint == workload_fingerprint(make_pilot_workload(42, config), config.resources)
    assert fingerprint == workload_fingerprint(Workload(list(reversed(workload.jobs))), config.resources)
    assert fingerprint != workload_fingerprint(make_pilot_workload(43, config), config.resources)


@pytest.mark.parametrize("field", ["id", "arrival", "duration", "gpus", "cpus", "memory", "estimate"])
def test_fingerprint_includes_every_job_input(field):
    values = dict(id=0, arrival=0, duration=4, gpus=1, cpus=2, memory=4096, estimate=3)

    def workload():
        return Workload([Job(values["id"], values["arrival"], values["duration"],
                             Resources(values["gpus"], values["cpus"], values["memory"]),
                             values["estimate"])])

    resources = PilotConfig().resources
    original = workload_fingerprint(workload(), resources)
    values[field] += 1
    assert original != workload_fingerprint(workload(), resources)
    values["estimate"] = None
    assert original != workload_fingerprint(workload(), resources)


@pytest.mark.parametrize("field", ["gpu_count", "cpu_count", "memory_mb"])
def test_fingerprint_includes_cluster_resources(field):
    config = PilotConfig()
    workload = make_pilot_workload(0, config)
    resources = config.resources
    original = workload_fingerprint(workload, resources)
    setattr(resources, field, getattr(resources, field) + 1)
    assert original != workload_fingerprint(workload, resources)
