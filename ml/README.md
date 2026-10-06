# ml/ — self-play learning

Python side of the Intelligence milestone (see `docs/design/INTELLIGENCE.md`).
The game's simulator, referee and classical AI are exposed as the
`billiards_sim` module, so training sees exactly the game's physics and rules.

## Setup

```bash
cd ml
uv sync                                   # Python 3.12, PyTorch, ONNX, ONNX Runtime
cd ..
cmake -B build-py -DCMAKE_BUILD_TYPE=Release -DBS_BUILD_PYTHON=ON -DBS_BUILD_TESTS=OFF \
      -DPython_EXECUTABLE=$PWD/ml/.venv/bin/python
cmake --build build-py -j8 --target billiards_sim   # writes ml/billiards_sim.*.so
```

## Shot-level API for reinforcement learning (`billiards_sim.rl`)

Any layout in, what happened out; no game session or rendering. Simulator
frame: metres, origin at a corner of the playing surface, x across
(1.27 m), y along (2.54 m). A strike is `(speed m/s, phi deg, theta deg, a, b)`.
Design: `docs/rl/rl-system-design.md`; reference: `docs/rl/environment.md`.

```python
import numpy as np, billiards_sim as bs
rl = bs.rl
gen = rl.LayoutGenerator(seed=1)
layout = gen.single_ball("medium")             # positions (2, 2), difficulty, easiest pocket
shot = rl.geometry_shot(layout["positions"])    # the geometry baseline's strike
out = rl.simulate(layout["positions"], np.array(shot["strike"]))
out["pocket_of"], out["scratch"], out["first_contact"]
batch = rl.simulate_batch(layouts, strikes)     # (B, n, 2), (B, 5): all cores, GIL released
```

Tests: `uv run pytest` (after building the module).

## The full-frame environment

```python
import billiards_sim as bs
env = bs.Environment(bs.Discipline.NINE_BALL, seed=1)
while env.decision() != bs.Decision.FRAME_OVER:
    x = env.features(env.to_act())            # FEATURE_COUNT floats, shared with the game
    env.play_classical("dani_reyes", seed=env.shots_played())
print(env.winner())
```

`ShotInput` fields: `aim` (radians), `power` (0..1), `right`, `forward`
(tip offset), `elevation` (degrees), `call` ((ball, pocket) or None),
`push_out`. `env.shoot(input)` plays a shot to rest and returns the
referee's verdict.
