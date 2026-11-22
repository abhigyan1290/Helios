"""Exercise the Python/C++ boundary with small, deterministic episodes."""

import gc

import pytest

helios = pytest.importorskip("helios", reason="install Helios to test Python bindings")


def resources(gpus=1):
    return helios.Resources(gpus, 4, 100)


def job(job_id, arrival=0, duration=2, gpus=1, estimate=None):
    return helios.Job(job_id, arrival, duration, helios.Resources(gpus, 1, 10), estimate)


def snapshot(sim):
    obs = sim.observe()
    return (obs.current_time, obs.available_resources.gpu_count,
            [item.id for item in obs.pending_jobs],
            [item.job.id for item in obs.running_jobs], obs.completed_job_count, obs.terminated)


def test_empty_episode_and_terminal_errors():
    sim = helios.Simulator(helios.Workload([]), resources())
    assert sim.done() and sim.observe().terminated
    assert sim.result().jobs == []
    assert helios.total_wait_time(sim.result()) == 0
    with pytest.raises(RuntimeError):
        sim.step(helios.no_schedule_decision())


def test_multiple_jobs_ordering_resources_and_results():
    sim = helios.Simulator(helios.Workload([job(1, duration=3), job(2, duration=3),
                                           job(3, arrival=1, duration=1)]), resources(2))
    with pytest.raises(RuntimeError):
        sim.result()
    assert sim.step(helios.select_job(1)).elapsed_time == 0
    assert sim.step(helios.select_job(2)).elapsed_time == 0
    assert sim.observe().available_resources.gpu_count == 0
    assert sim.step(helios.no_schedule_decision()).elapsed_time == 1
    assert [j.id for j in sim.observe().pending_jobs] == [3]
    before = snapshot(sim)
    with pytest.raises(RuntimeError):
        sim.step(helios.select_job(3))
    assert snapshot(sim) == before
    assert sim.step(helios.no_schedule_decision()).completed_job_ids == [1, 2]
    before = snapshot(sim)
    with pytest.raises(RuntimeError):
        sim.step(helios.select_job(1))
    assert snapshot(sim) == before
    sim.step(helios.select_job(3))
    assert sim.step(helios.no_schedule_decision()).completed_job_ids == [3]
    result = sim.result()
    assert sim.done() and result.makespan == 4
    assert [(j.job_id, j.start_time, j.completion_time, j.wait_time) for j in result.jobs] == [
        (1, 0, 3, 0), (2, 0, 3, 0), (3, 3, 4, 2)]
    assert helios.total_wait_time(result) == 2
    assert helios.average_wait_time(result) == pytest.approx(2 / 3)
    assert helios.p95_wait_time(result) == helios.max_wait_time(result) == 2


def test_invalid_actions_reset_and_snapshot_lifetime():
    workload = helios.Workload([job(1), job(2, arrival=5)])
    sim = helios.Simulator(workload, resources())
    old = sim.observe()
    for job_id, exception in [(-1, ValueError), (2, RuntimeError), (99, RuntimeError)]:
        before = snapshot(sim)
        with pytest.raises(exception):
            sim.step(helios.select_job(job_id))
        assert snapshot(sim) == before
    sim.step(helios.select_job(1))
    before = snapshot(sim)
    with pytest.raises(RuntimeError):
        sim.step(helios.select_job(1))
    assert snapshot(sim) == before
    with pytest.raises((ValueError, RuntimeError)):
        sim.reset(workload, resources(0))
    assert snapshot(sim) == before
    sim.reset(workload, resources())
    assert snapshot(sim) == (0, 1, [1], [], 0, False)
    del sim
    gc.collect()
    assert old.current_time == 0 and old.pending_jobs[0].id == 1


def test_observation_hides_future_and_actual_duration():
    first = helios.Simulator(helios.Workload([job(1, duration=2, estimate=3)]), resources())
    second = helios.Simulator(helios.Workload([job(1, duration=9, estimate=3),
                                              job(2, arrival=10)]), resources())
    assert snapshot(first) == snapshot(second)
    view = second.observe().pending_jobs[0]
    assert view.estimated_duration == 3
    assert not hasattr(view, "duration")
    assert not hasattr(second, "observe_oracle")
    assert not hasattr(second, "diagnostic_state")


def test_invalid_workload_and_integer_overflow():
    with pytest.raises(ValueError):
        helios.Workload([job(1), job(1)])
    with pytest.raises(ValueError):
        helios.Workload([job(1, duration=0)])
    with pytest.raises((ValueError, RuntimeError)):
        helios.Simulator(helios.Workload([job(1, gpus=2)]), resources())
    with pytest.raises((TypeError, OverflowError)):
        job(2**100)


@pytest.mark.parametrize("resource_request", [helios.Resources(1, 5, 10),
                                      helios.Resources(1, 1, 101)])
def test_cpu_and_memory_limits(resource_request):
    with pytest.raises((ValueError, RuntimeError)):
        helios.Simulator(helios.Workload([helios.Job(1, 0, 1, resource_request)]), resources())
