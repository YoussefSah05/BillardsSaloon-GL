"""Fixed evaluation suites: versioned, hashed sets of layouts.

Every method is evaluated on the same layouts (rl-system-design.md, Section 9).
A suite is generated once by scripts/make_eval_suites.py and committed; its
SHA-256 over the canonical layout list is checked on every load, so a changed
file can never silently change results.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np

SUITE_DIR = Path(__file__).resolve().parents[2] / "eval_states"
SUITE_FORMAT = 1


def layouts_hash(layouts: list[dict]) -> str:
    canonical = json.dumps(layouts, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(canonical.encode()).hexdigest()


def load_suite(name: str, directory: Path = SUITE_DIR) -> dict:
    path = directory / f"{name}.json"
    suite = json.loads(path.read_text())
    if suite.get("format") != SUITE_FORMAT:
        raise ValueError(f"{path}: unsupported suite format {suite.get('format')}")
    digest = layouts_hash(suite["layouts"])
    if digest != suite["sha256"]:
        raise ValueError(f"{path}: layouts do not match their hash (expected {suite['sha256']}, got {digest})")
    return suite


def suite_positions(suite: dict) -> list[np.ndarray]:
    return [np.array([layout["cue"], layout["object"]], dtype=np.float64) for layout in suite["layouts"]]


def suite_cue_targets(suite: dict) -> list[np.ndarray]:
    """Cue-ball targets of a position suite (Milestone 10)."""
    return [np.array(layout["cue_target"], dtype=np.float64) for layout in suite["layouts"]]
