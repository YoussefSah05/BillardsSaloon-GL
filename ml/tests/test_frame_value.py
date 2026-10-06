"""The learned frame value under WPA rules (Milestone 13)."""

import numpy as np
import torch

import billiards_sim as bs
from billiards_rl.selfplay.frame_value import FrameValueNet, NumpyValue, fit_frame_value, log_loss
from billiards_rl.selfplay.frames import play_frame


def test_planner_calls_the_value_with_shared_features():
    seen = []

    def value(features):
        seen.append(np.asarray(features).shape)
        return 0.5

    env = bs.Environment(bs.Discipline.NINE_BALL, 1)
    env.reset(3, 0)
    env.play_classical("iris_lindqvist", 1)                    # the break: no evaluation
    while env.decision() not in (bs.Decision.SHOT, bs.Decision.FRAME_OVER):
        env.play_classical("iris_lindqvist", 2)
    if env.decision() == bs.Decision.SHOT:
        env.play_classical("iris_lindqvist", 3, True, value)
        assert seen and all(s == (bs.FEATURE_COUNT,) for s in seen)


def test_numpy_value_matches_torch():
    torch.manual_seed(0)
    net = FrameValueNet(bs.FEATURE_COUNT, hidden=32)
    x = np.random.default_rng(0).normal(size=(5, bs.FEATURE_COUNT)).astype(np.float32)
    with torch.no_grad():
        expected = torch.sigmoid(net(torch.as_tensor(x))).numpy()
    value = NumpyValue.from_state(NumpyValue(net).state())
    assert np.allclose([value(r) for r in x], expected, atol=1e-6)


def test_frames_record_both_points_of_view_and_fit():
    xs, zs, fs = [], [], []
    for rack in range(4):
        winner, records = play_frame(rack, rack % 2, "iris_lindqvist", (None, None), record=True)
        assert winner in (0, 1) and len(records) % 2 == 0
        for features, player in records:
            xs.append(features)
            zs.append(float(player == winner))
            fs.append(rack)
    z = np.asarray(zs)
    assert z.mean() == 0.5                          # each position appears once per player
    _, fit = fit_frame_value(np.asarray(xs, dtype=np.float32), z, np.asarray(fs), epochs=2, validation=0.25)
    assert np.isfinite(fit["val_log_loss"]) and fit["val_positions"] > 0
    assert np.isclose(log_loss(np.full(4, 0.5), np.array([0, 1, 0, 1.0])), np.log(2))
