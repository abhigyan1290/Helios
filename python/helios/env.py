"""Gymnasium interface to the C++ scheduling simulator."""

import math

import gymnasium as gym
import numpy as np
from gymnasium import spaces

from ._core import Job, Resources, Simulator, Workload, no_schedule_decision, select_job


def _resources(value):
    return Resources(value.gpu_count, value.cpu_count, value.memory_mb)


class HeliosEnv(gym.Env):
    """Schedule a fixed workload using pending-job slots and a wait action.

    Actions 0..max_pending-1 select a displayed job; max_pending waits for an
    event. Jobs outside the displayed window accrue wait but cannot be selected.
    """

    metadata = {"render_modes": []}

    def __init__(self, workload, total_resources, *, max_pending=32, max_running=32,
                 wait_penalty=1.0):
        super().__init__()
        if not isinstance(max_pending, int) or isinstance(max_pending, bool) or max_pending < 1:
            raise ValueError("max_pending must be a positive integer")
        if not isinstance(max_running, int) or isinstance(max_running, bool) or max_running < 1:
            raise ValueError("max_running must be a positive integer")
        if not math.isfinite(wait_penalty) or wait_penalty < 0:
            raise ValueError("wait_penalty must be finite and non-negative")
        # Own copies so caller mutations cannot silently change future resets.
        self._workload = Workload([Job(j.id, j.arrival_time, j.duration, _resources(j.request),
                                       j.estimated_duration) for j in workload.jobs])
        self._resources = _resources(total_resources)
        self._sim = Simulator(self._workload, self._resources)
        self.max_pending = max_pending
        self.max_running = max_running
        self.wait_penalty = float(wait_penalty)
        self.action_space = spaces.Discrete(max_pending + 1)
        integer_max = np.iinfo(np.int32).max
        box = lambda shape: spaces.Box(0, integer_max, shape=shape, dtype=np.int64)
        self.observation_space = spaces.Dict({
            "time": box((1,)),
            "total_resources": box((3,)),
            "available_resources": box((3,)),
            # id, arrival, GPU, CPU, memory, estimate (zero means missing)
            "pending_jobs": box((max_pending, 6)),
            "pending_mask": spaces.MultiBinary(max_pending),
            "schedulable_mask": spaces.MultiBinary(max_pending),
            # id, arrival, GPU, CPU, memory, estimate, start, elapsed
            "running_jobs": box((max_running, 8)),
            "running_mask": spaces.MultiBinary(max_running),
            "pending_count": box((1,)),
            "running_count": box((1,)),
            "completed_count": box((1,)),
        })
        self._has_reset = False

    def _observation(self, view):
        pending = np.zeros((self.max_pending, 6), dtype=np.int64)
        running = np.zeros((self.max_running, 8), dtype=np.int64)
        pending_mask = np.zeros(self.max_pending, dtype=np.int8)
        schedulable = np.zeros(self.max_pending, dtype=np.int8)
        running_mask = np.zeros(self.max_running, dtype=np.int8)

        def fields(job):
            req = job.request
            return [job.id, job.arrival_time, req.gpu_count, req.cpu_count, req.memory_mb,
                    job.estimated_duration or 0]

        available = view.available_resources
        for i, job in enumerate(view.pending_jobs[:self.max_pending]):
            pending[i] = fields(job)
            pending_mask[i] = 1
            req = job.request
            schedulable[i] = (req.gpu_count <= available.gpu_count
                              and req.cpu_count <= available.cpu_count
                              and req.memory_mb <= available.memory_mb)
        ordered_running = sorted(view.running_jobs, key=lambda j: (j.start_time, j.job.id))
        for i, job in enumerate(ordered_running[:self.max_running]):
            running[i] = fields(job.job) + [job.start_time, job.elapsed_time]
            running_mask[i] = 1
        return {
            "time": np.array([view.current_time], dtype=np.int64),
            "total_resources": np.array([view.total_resources.gpu_count,
                                         view.total_resources.cpu_count,
                                         view.total_resources.memory_mb], dtype=np.int64),
            "available_resources": np.array([available.gpu_count, available.cpu_count,
                                             available.memory_mb], dtype=np.int64),
            "pending_jobs": pending, "pending_mask": pending_mask,
            "schedulable_mask": schedulable, "running_jobs": running,
            "running_mask": running_mask,
            "pending_count": np.array([len(view.pending_jobs)], dtype=np.int64),
            "running_count": np.array([len(view.running_jobs)], dtype=np.int64),
            "completed_count": np.array([view.completed_job_count], dtype=np.int64),
        }

    def reset(self, *, seed=None, options=None):
        super().reset(seed=seed)
        if options:
            raise ValueError("reset options are not supported")
        self._sim.reset(self._workload, self._resources)
        self._has_reset = True
        return self._observation(self._sim.observe()), {}

    def step(self, action):
        if not self._has_reset:
            raise RuntimeError("reset must be called before step")
        if self._sim.done():
            raise RuntimeError("cannot step a terminated simulator")
        if not self.action_space.contains(action):
            raise ValueError("action is outside the action space")
        view = self._sim.observe()
        if action == self.max_pending:
            transition = self._sim.step(no_schedule_decision())
        else:
            visible = view.pending_jobs[:self.max_pending]
            if action >= len(visible):
                raise ValueError("action selects an empty pending slot")
            # The C++ simulator validates fit before modifying its state.
            transition = self._sim.step(select_job(visible[action].id))
        reward = -self.wait_penalty * len(view.pending_jobs) * transition.elapsed_time
        return self._observation(self._sim.observe()), reward, transition.terminated, False, {}

    def result(self):
        """Return the C++ terminal result; raises if the episode is not finished."""
        return self._sim.result()
