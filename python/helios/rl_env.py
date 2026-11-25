"""Training adapter; all time advancement and resource accounting stay in C++."""

import gymnasium as gym
import numpy as np
from gymnasium import spaces

from .env import HeliosEnv
from .rl_workloads import PilotConfig, make_pilot_workload, workload_fingerprint


class PilotEnv(gym.Env):
    """Small, fully visible arrived queues with noisy duration estimates.

    Training workload seeds are sampled from [0, 100000). Explicit evaluation
    seeds are passed through reset options. Waiting is masked only when C++ has
    no event to advance to, never based on a hidden completion timestamp.
    """

    metadata = {"render_modes": []}

    def __init__(self, config=None, profile="mixed"):
        super().__init__()
        self.config = config or PilotConfig()
        if self.config.job_count < 1:
            raise ValueError("training episodes require at least one job")
        if profile not in {"mixed", "bursty", "staggered"}:
            raise ValueError("unknown workload profile")
        self.profile = profile
        self.action_space = spaces.Discrete(self.config.job_count + 1)
        self.observation_space = spaces.Box(
            0, np.inf, shape=(7 + 7 * self.config.job_count + 6 * self.config.gpu_count,),
            dtype=np.float32,
        )
        self.reward_scale = float(self.config.job_count * self.config.max_duration)
        self._base = None

    def reset(self, *, seed=None, options=None):
        super().reset(seed=seed)
        options = options or {}
        if set(options) - {"workload_seed"}:
            raise ValueError("only workload_seed is supported")
        workload_seed = options.get("workload_seed")
        if workload_seed is None:
            workload_seed = int(self.np_random.integers(0, 100000))
        workload = make_pilot_workload(workload_seed, self.config, self.profile)
        self.workload_seed = workload_seed
        self.fingerprint = workload_fingerprint(workload, self.config.resources)
        self._base = HeliosEnv(
            workload, self.config.resources, max_pending=self.config.job_count,
            max_running=self.config.gpu_count,
        )
        self._base.reset()
        return self._features(), {"workload_seed": workload_seed, "fingerprint": self.fingerprint}

    def observe(self):
        """Return the same safe C++ observation used by the baseline policies."""
        if self._base is None:
            raise RuntimeError("reset must be called first")
        return self._base._sim.observe()

    def action_masks(self):
        view = self.observe()
        mask = np.zeros(self.action_space.n, dtype=bool)
        available = view.available_resources
        for i, job in enumerate(view.pending_jobs):
            request = job.request
            mask[i] = (request.gpu_count <= available.gpu_count
                       and request.cpu_count <= available.cpu_count
                       and request.memory_mb <= available.memory_mb)
        # All episodes have a known fixed job count. This reveals neither future
        # arrival times nor running-job completion times.
        future_count = (self.config.job_count - len(view.pending_jobs)
                        - len(view.running_jobs) - view.completed_job_count)
        mask[-1] = bool(view.running_jobs) or future_count > 0
        return mask

    def _features(self):
        view = self.observe()
        cfg = self.config
        time_scale = cfg.max_arrival + cfg.job_count * cfg.max_duration
        total = cfg.resources

        def resource_fields(resources):
            return [resources.gpu_count / total.gpu_count,
                    resources.cpu_count / total.cpu_count,
                    resources.memory_mb / total.memory_mb]

        features = [view.current_time / time_scale,
                    *resource_fields(view.available_resources),
                    len(view.pending_jobs) / cfg.job_count,
                    len(view.running_jobs) / cfg.gpu_count,
                    view.completed_job_count / cfg.job_count]
        mask = self.action_masks()
        for i in range(cfg.job_count):
            if i < len(view.pending_jobs):
                job = view.pending_jobs[i]
                features.extend([1, float(mask[i]),
                                 (view.current_time - job.arrival_time) / time_scale,
                                 *resource_fields(job.request),
                                 (job.estimated_duration or 0) / cfg.max_duration])
            else:
                features.extend([0] * 7)
        running = sorted(view.running_jobs, key=lambda run: (run.start_time, run.job.id))
        for i in range(cfg.gpu_count):
            if i < len(running):
                run = running[i]
                features.extend([1, *resource_fields(run.job.request),
                                 (run.job.estimated_duration or 0) / cfg.max_duration,
                                 run.elapsed_time / cfg.max_duration])
            else:
                features.extend([0] * 6)
        return np.asarray(features, dtype=np.float32)

    def step(self, action):
        if not self.action_space.contains(action):
            raise ValueError("action is outside the action space")
        if not self.action_masks()[int(action)]:
            raise ValueError("action is masked; state is unchanged")
        _, reward, terminated, truncated, info = self._base.step(int(action))
        return self._features(), reward / self.reward_scale, terminated, truncated, info

    def result(self):
        self.observe()
        return self._base.result()

    def close(self):
        if self._base is not None:
            self._base.close()
