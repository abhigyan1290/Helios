"""Observation-only C++ policies exercised through the interactive bindings."""

import pytest

core = pytest.importorskip("helios._core", reason="install Helios to test Python bindings")

POLICIES = (
    "fifo_policy",
    "estimated_sjf_schedule",
    "easy_backfill_schedule",
    "RandomScheduler",
)


def policy_for(name):
    policy = getattr(core, name)
    return policy(seed=42) if name == "RandomScheduler" else policy


def resources(gpus=4, cpus=8, memory=32):
    return core.Resources(gpus, cpus, memory)


def job(job_id, duration=3, estimate=3, gpus=1, arrival=0):
    return core.Job(job_id, arrival, duration, resources(gpus, 1, 1), estimate)


def resource_snapshot(value):
    return value.gpu_count, value.cpu_count, value.memory_mb


def job_snapshot(value):
    return (value.id, value.arrival_time, value.estimated_duration,
            resource_snapshot(value.request))


def snapshot(observation):
    return (
        observation.current_time,
        resource_snapshot(observation.total_resources),
        resource_snapshot(observation.available_resources),
        tuple(job_snapshot(value) for value in observation.pending_jobs),
        tuple((job_snapshot(value.job), value.start_time, value.elapsed_time)
              for value in observation.running_jobs),
        observation.completed_job_count,
        observation.terminated,
    )


def run_episode(policy, workload):
    before = [(job_snapshot(value), value.duration) for value in workload.jobs]
    sim = core.Simulator(workload, resources())
    trace = []
    for _ in range(64):
        observation = sim.observe()
        original = snapshot(observation)
        decision = policy(observation=observation)
        assert snapshot(observation) == original
        assert snapshot(sim.observe()) == original
        if sim.done():
            assert decision.job_id is None
            break
        if decision.job_id is not None:
            selected = next(value for value in observation.pending_jobs
                            if value.id == decision.job_id)
            assert all(request <= available for request, available in zip(
                resource_snapshot(selected.request),
                resource_snapshot(observation.available_resources)))
        result = sim.step(decision)
        trace.append((original, decision.job_id, result.elapsed_time,
                      tuple(result.completed_job_ids)))
        assert snapshot(observation) == original
    else:
        pytest.fail("policy did not terminate within the small episode step bound")
    assert [(job_snapshot(value), value.duration) for value in workload.jobs] == before
    assert len(sim.result().jobs) == len(workload.jobs)
    return trace


@pytest.mark.parametrize("name", POLICIES)
@pytest.mark.parametrize("scenario", ("empty", "single", "multiple"))
def test_policy_episodes(name, scenario):
    jobs = {
        "empty": [],
        "single": [job(1, arrival=2)],
        "multiple": [job(4, arrival=2), job(2, duration=5, estimate=1, gpus=4),
                     job(1, duration=2, estimate=8, gpus=2),
                     job(3, duration=1, estimate=2), job(5, arrival=20)],
    }[scenario]
    run_episode(policy_for(name), core.Workload(jobs))


@pytest.mark.parametrize("name, expected", (
    ("fifo_policy", None),
    ("estimated_sjf_schedule", 3),
    ("easy_backfill_schedule", 3),
    ("RandomScheduler", 3),
))
def test_blocked_fifo_head_and_resource_filtering(name, expected):
    sim = core.Simulator(core.Workload([
        job(1, duration=10, estimate=10, gpus=2), job(2, gpus=4), job(3),
    ]), resources())
    sim.step(core.select_job(1))
    observation = sim.observe()
    before = snapshot(observation)
    decision = policy_for(name)(observation=observation)
    assert decision.job_id == expected
    assert snapshot(observation) == snapshot(sim.observe()) == before
    sim.step(decision)


def test_random_seed_repeatability_and_reset():
    workload = core.Workload([job(i) for i in range(1, 9)])
    first = core.RandomScheduler(seed=42)
    second = core.RandomScheduler(seed=42)
    trace = run_episode(first, workload)
    assert run_episode(second, workload) == trace
    first.reset(seed=42)
    assert run_episode(first, workload) == trace


@pytest.mark.parametrize("name", POLICIES)
def test_policies_use_estimates_without_actual_runtime_access(name):
    observations = []
    for durations in ((1, 20), (30, 2)):
        sim = core.Simulator(core.Workload([
            job(1, duration=durations[0], estimate=20),
            job(2, duration=durations[1], estimate=1),
        ]), resources())
        observations.append(sim.observe())
    assert snapshot(observations[0]) == snapshot(observations[1])
    decisions = [policy_for(name)(observation=value).job_id for value in observations]
    assert decisions[0] == decisions[1]
    if name == "estimated_sjf_schedule":
        assert decisions[0] == 2
    for observation in observations:
        assert not hasattr(observation, "duration")
        for value in observation.pending_jobs:
            assert not hasattr(value, "duration")
    for forbidden in ("fifo_schedule", "sjf_schedule", "SimulatorState"):
        assert not hasattr(core, forbidden)


@pytest.mark.parametrize("name", ("estimated_sjf_schedule", "easy_backfill_schedule"))
def test_missing_estimates_raise_without_mutation(name):
    sim = core.Simulator(core.Workload([job(1, estimate=None)]), resources())
    observation = sim.observe()
    before = snapshot(observation)
    with pytest.raises(ValueError):
        policy_for(name)(observation=observation)
    assert snapshot(observation) == snapshot(sim.observe()) == before


@pytest.mark.parametrize("name", POLICIES)
def test_policy_rejects_non_observation(name):
    with pytest.raises(TypeError):
        policy_for(name)(observation=core.Workload([job(1)]))
