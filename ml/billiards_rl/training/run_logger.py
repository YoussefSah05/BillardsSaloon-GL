"""A run's folder: resolved config, metadata, evaluations, checkpoints.

    runs/<date>-<algorithm>-<frame>-s<seed>/
        config.yaml      the resolved configuration
        meta.json        git commit and dirty flag, env version, platform,
                         library versions, seed, start and end times
        progress.csv     the training curve (Stable-Baselines3 logger)
        events.*         TensorBoard logs of the same
        eval/*.json      metrics from each evaluation
        checkpoints/     best.zip (by during-training success) and last.zip
"""

from __future__ import annotations

import json
import platform
import subprocess
from dataclasses import asdict, is_dataclass
from datetime import datetime, timezone
from pathlib import Path

import yaml


def _git(*args: str) -> str:
    try:
        return subprocess.check_output(["git", *args], text=True, stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


class RunLogger:
    def __init__(self, root: Path, config: dict, seed: int, name: str | None = None):
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        frame = config["env"].get("action_frame", "absolute")
        self.dir = root / (name or f"{stamp}-{config['algorithm']}-{frame}-s{seed}")
        (self.dir / "eval").mkdir(parents=True, exist_ok=True)
        (self.dir / "checkpoints").mkdir(exist_ok=True)
        (self.dir / "config.yaml").write_text(yaml.safe_dump(config, sort_keys=False))

        import gymnasium
        import stable_baselines3
        import torch

        from billiards_rl.envs.single_shot_env import ENV_VERSION
        self.meta = {
            "seed": seed,
            "env_version": ENV_VERSION,
            "git_commit": _git("rev-parse", "HEAD"),
            "git_dirty": bool(_git("status", "--porcelain", "--untracked-files=no")),
            "platform": platform.platform(),
            "processor": platform.processor(),
            "python": platform.python_version(),
            "torch": torch.__version__,
            "stable_baselines3": stable_baselines3.__version__,
            "gymnasium": gymnasium.__version__,
            "started": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        }
        self._write_meta()

    def _write_meta(self) -> None:
        (self.dir / "meta.json").write_text(json.dumps(self.meta, indent=1) + "\n")

    def log_eval(self, name: str, metrics) -> None:
        data = asdict(metrics) if is_dataclass(metrics) else metrics
        (self.dir / "eval" / f"{name}.json").write_text(json.dumps(data, indent=1) + "\n")

    def finish(self, **extra) -> None:
        self.meta["finished"] = datetime.now(timezone.utc).isoformat(timespec="seconds")
        self.meta.update(extra)
        self._write_meta()
