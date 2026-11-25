"""Seeded, bounded input data for the initial RL pilot (no simulation logic)."""

from dataclasses import dataclass
import hashlib
import json

import numpy as np

from ._core import Job, Resources, Workload


@dataclass(frozen=True)
class PilotConfig:
    """JSON-ready input bounds; use ``dataclasses.asdict`` to serialize.

    Bounds may be reduced, but never exceed 24 duration units or 32 arrival
    units. ``horizon`` bounds completion time under serial, non-idling service;
    it is not a simulator time limit. Estimates may exceed actual-duration caps.
    """

    job_count: int = 16
    gpu_count: int = 4
    max_duration: int = 24
    max_arrival: int = 32

    def __post_init__(self) -> None:
        for name in ("job_count", "gpu_count", "max_duration", "max_arrival"):
            if type(getattr(self, name)) is not int:
                raise TypeError(f"{name} must be an integer")
        if self.job_count < 0:
            raise ValueError("job_count must be nonnegative")
        if not 1 <= self.gpu_count <= 4:
            raise ValueError("gpu_count must be between 1 and 4")
        if not 1 <= self.max_duration <= 24:
            raise ValueError("max_duration must be between 1 and 24")
        if not 0 <= self.max_arrival <= 32:
            raise ValueError("max_arrival must be between 0 and 32")
        if self.horizon > 2**31 - 1:
            raise ValueError("serial horizon must fit the C++ simulator time type")

    @property
    def resources(self) -> Resources:
        return Resources(self.gpu_count, 8 * self.gpu_count, 32768 * self.gpu_count)

    @property
    def horizon(self) -> int:
        return self.max_arrival + self.job_count * self.max_duration


def make_pilot_workload(
    seed: int, config: PilotConfig, profile: str = "mixed"
) -> Workload:
    """Build reproducible inputs with a local RNG, leaving seed splits to callers.

    Bursty arrivals share a few wave times; staggered arrivals span the arrival
    window with bounded jitter. Mixed randomly selects either or combines both.
    Durations alternate randomly ordered short/long classes when bounds permit.
    Estimates use independent uniform [0.6, 1.4) multipliers, rounded positive;
    individual estimates can equal actual durations through rounding.
    """
    if type(seed) is not int:
        raise TypeError("seed must be an integer")
    if seed < 0:
        raise ValueError("seed must be nonnegative")
    if not isinstance(config, PilotConfig):
        raise TypeError("config must be a PilotConfig")
    if profile not in ("bursty", "staggered", "mixed"):
        raise ValueError("profile must be bursty, staggered, or mixed")

    rng = np.random.default_rng(seed)
    count = config.job_count
    if count == 0:
        return Workload([])

    mode = int(rng.integers(0, 3)) if profile == "mixed" else (
        0 if profile == "bursty" else 1
    )
    waves = np.array([0, config.max_arrival // 2, config.max_arrival])
    bursty = rng.choice(waves, size=count)
    staggered = np.clip(
        np.rint(np.linspace(0, config.max_arrival, count)).astype(int)
        + rng.integers(-1, 2, size=count),
        0,
        config.max_arrival,
    )
    if mode == 0:
        arrivals = bursty
    elif mode == 1:
        arrivals = staggered
    else:
        arrivals = np.where(rng.integers(0, 2, size=count), bursty, staggered)
    arrivals[0] = 0

    short_cap = max(1, config.max_duration // 4)
    long_floor = max(short_cap + 1, (config.max_duration + 1) // 2)
    long_jobs = rng.permutation(np.arange(count) % 2)
    jobs = []
    for job_id in range(count):
        if long_jobs[job_id] and config.max_duration > 1:
            duration = int(rng.integers(long_floor, config.max_duration + 1))
        else:
            duration = int(rng.integers(1, short_cap + 1))
        gpus = int(rng.integers(1, config.gpu_count + 1))
        request = Resources(
            gpus,
            gpus * int(rng.integers(2, 9)),
            gpus * int(rng.integers(4, 33)) * 1024,
        )
        estimate = max(1, int(np.rint(duration * rng.uniform(0.6, 1.4))))
        jobs.append(Job(job_id, int(arrivals[job_id]), duration, request, estimate))
    return Workload(jobs)


def workload_fingerprint(workload: Workload, resources: Resources) -> str:
    """SHA-256 of all job inputs and cluster resources, independent of job order."""
    def resource_fields(value: Resources) -> list[int]:
        return [value.gpu_count, value.cpu_count, value.memory_mb]

    payload = {
        "resources": resource_fields(resources),
        "jobs": [
            [job.id, job.arrival_time, job.duration,
             resource_fields(job.request), job.estimated_duration]
            for job in sorted(workload.jobs, key=lambda job: job.id)
        ],
    }
    encoded = json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()
