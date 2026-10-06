"""The billiards_sim.rl shot API: shapes, determinism, outcomes, baselines.

Build the module first (from the repository root):
    cmake -B build-py -DBS_BUILD_PYTHON=ON -DCMAKE_BUILD_TYPE=Release
    cmake --build build-py --target billiards_sim
"""

import numpy as np
import pytest

import billiards_sim as bs

rl = bs.rl


@pytest.fixture(scope="module")
def table():
    return rl.table_info()


def test_table_info_describes_the_nine_foot_table(table):
    assert table["width"] == pytest.approx(1.27)
    assert table["length"] == pytest.approx(2.54)
    assert table["pockets"].shape == (6, 2)
    assert table["mouths"].shape == (6, 2)
    # A ball fits through every mouth.
    assert np.all(2 * np.asarray(table["mouth_half_width"]) > 2 * table["ball_radius"])


def test_layouts_are_seeded_legal_and_banded(table):
    a, b = rl.LayoutGenerator(5), rl.LayoutGenerator(5)
    r = table["ball_radius"]
    for band in ("easy", "medium", "hard", "any"):
        for _ in range(20):
            la, lb = a.single_ball(band), b.single_ball(band)
            np.testing.assert_array_equal(la["positions"], lb["positions"])
            p = la["positions"]
            assert p.shape == (2, 2)
            assert np.all(p[:, 0] > r) and np.all(p[:, 0] < table["width"] - r)
            assert np.all(p[:, 1] > r) and np.all(p[:, 1] < table["length"] - r)
            assert np.linalg.norm(p[0] - p[1]) > 2 * r
            if band != "any":
                assert la["difficulty"] == band
                assert 0 <= la["pocket"] < 6


def test_unknown_band_is_rejected():
    with pytest.raises(ValueError):
        rl.LayoutGenerator(1).single_ball("impossible")


def test_simulate_is_deterministic_and_complete():
    layout = rl.LayoutGenerator(3).single_ball("any")["positions"]
    strike = np.array([3.0, 47.0, 0.0, 0.1, -0.2])
    first, second = rl.simulate(layout, strike), rl.simulate(layout, strike)
    np.testing.assert_array_equal(first["final_positions"], second["final_positions"])
    assert first["complete"]
    assert np.all(np.isfinite(first["final_positions"]))
    assert first["pocket_of"].shape == (2,)


def test_batch_equals_single_shots():
    gen = rl.LayoutGenerator(8)
    layouts = np.stack([gen.single_ball("any")["positions"] for _ in range(40)])
    rng = np.random.default_rng(1)
    strikes = np.column_stack([rng.uniform(0.5, 7.0, 40), rng.uniform(0, 360, 40), np.zeros((40, 3))])
    batch = rl.simulate_batch(layouts, strikes, threads=4)
    for i in range(40):
        one = rl.simulate(layouts[i], strikes[i])
        np.testing.assert_array_equal(one["pocket_of"], batch["pocket_of"][i])
        np.testing.assert_array_equal(one["final_positions"], batch["final_positions"][i])
        assert one["scratch"] == batch["scratch"][i]


def test_a_scratch_is_detected(table):
    # Cue ball rolled at a corner mouth with the object ball out of the way.
    cue = np.array([0.6, 0.6])
    mouth = table["mouths"][0]
    phi = np.degrees(np.arctan2(*(mouth - cue)[::-1]))
    out = rl.simulate(np.array([cue, [1.0, 2.2]]), np.array([2.0, phi, 0, 0, 0]))
    assert out["scratch"]
    assert out["pocket_of"][0] == 0
    assert out["first_contact"] == -1


def test_geometry_baseline_pots_most_easy_layouts():
    gen = rl.LayoutGenerator(21)
    potted = 0
    for _ in range(200):
        layout = gen.single_ball("easy")
        shot = rl.geometry_shot(layout["positions"])
        assert shot is not None
        out = rl.simulate(layout["positions"], np.array(shot["strike"]))
        potted += int(out["pocket_of"][1] >= 0 and not out["scratch"])
    assert potted / 200 > 0.7


def test_ghost_aims_cover_every_pocket():
    layout = rl.LayoutGenerator(4).single_ball("medium")["positions"]
    aims = rl.ghost_aims(layout)
    for key in ("phi", "cut", "cue_distance", "object_distance", "ease", "blocked"):
        assert aims[key].shape == (6,)
    assert np.all((aims["phi"] >= 0) & (aims["phi"] < 360))
    assert np.all((aims["cut"] >= 0) & (aims["cut"] <= 180))


def test_bad_shapes_are_rejected():
    with pytest.raises(ValueError):
        rl.simulate(np.zeros((2, 3)), np.zeros(5))
    with pytest.raises(ValueError):
        rl.simulate(np.zeros((2, 2)) + 0.5, np.zeros(4))
