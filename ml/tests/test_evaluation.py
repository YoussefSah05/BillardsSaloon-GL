"""Evaluation suites and protocol."""

import json

import numpy as np
import pytest

from billiards_rl.baselines import GeometryPolicy, RandomPolicy, make_baseline
from billiards_rl.evaluation.evaluate import evaluate, wilson_interval
from billiards_rl.evaluation.suites import layouts_hash, load_suite, suite_positions


@pytest.mark.parametrize("name", ["easy", "medium", "hard"])
def test_suites_load_and_match_their_hash(name):
    suite = load_suite(name)
    assert suite["count"] == 1000 == len(suite["layouts"])
    assert all(layout["difficulty"] == name for layout in suite["layouts"])
    positions = suite_positions(suite)
    assert positions[0].shape == (2, 2)


def test_a_tampered_suite_is_rejected(tmp_path):
    suite = load_suite("easy")
    suite["layouts"][0]["cue"][0] += 0.001
    (tmp_path / "easy.json").write_text(json.dumps(suite))
    with pytest.raises(ValueError, match="hash"):
        load_suite("easy", directory=tmp_path)


def test_hash_is_order_sensitive():
    a = [{"cue": [0.1, 0.2], "object": [0.3, 0.4]}, {"cue": [0.5, 0.6], "object": [0.7, 0.8]}]
    assert layouts_hash(a) != layouts_hash(list(reversed(a)))


def test_wilson_interval_brackets_the_rate():
    low, high = wilson_interval(550, 1000)
    assert low < 0.55 < high
    assert high - low == pytest.approx(0.062, abs=0.002)
    assert wilson_interval(0, 10)[0] == 0.0


def test_evaluation_is_reproducible_including_noise():
    first, episodes_a = evaluate(GeometryPolicy(), "medium", (0.25, 0.0), limit=100)
    second, episodes_b = evaluate(GeometryPolicy(), "medium", (0.25, 0.0), limit=100)
    assert first.success_rate == second.success_rate
    assert [e["success"] for e in episodes_a] == [e["success"] for e in episodes_b]


def test_baselines_are_ordered_on_a_small_sample():
    random_metrics, _ = evaluate(RandomPolicy(0), "easy", limit=200)
    geometry_metrics, _ = evaluate(GeometryPolicy(), "easy", limit=200)
    search_metrics, _ = evaluate(make_baseline("sim_search"), "easy", limit=50)
    assert random_metrics.success_rate < 0.1
    assert geometry_metrics.success_rate > 0.7
    assert search_metrics.success_rate >= geometry_metrics.success_rate - 0.05
    assert np.isfinite(geometry_metrics.mean_distance_after_miss)
