"""Loading trained models for evaluation."""

from __future__ import annotations

from pathlib import Path

from stable_baselines3 import PPO, SAC, TD3
from stable_baselines3.common.buffers import DictReplayBuffer

ALGORITHMS = {"sac": SAC, "td3": TD3, "ppo": PPO}


def load_policy_model(algorithm: str, path: str | Path):
    """Loads a checkpoint for acting only.

    Off-policy models are loaded with a minimal replay buffer: a HER buffer
    cannot be rebuilt without its training environment, and evaluation never
    needs one.
    """
    if algorithm in ("elite", "awr"):
        from billiards_rl.agents.elite import EliteAgent
        return EliteAgent.load(Path(path).with_suffix(".pt"))
    cls = ALGORITHMS[algorithm]
    if cls is PPO:
        return cls.load(path, device="cpu")
    try:
        return cls.load(path, device="cpu", custom_objects={"buffer_size": 1})
    except AssertionError:
        return cls.load(path, device="cpu", custom_objects={
            "buffer_size": 1, "replay_buffer_class": DictReplayBuffer, "replay_buffer_kwargs": {}})
