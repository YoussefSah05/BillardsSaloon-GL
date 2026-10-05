"""Generate golden-shot fixtures for the event simulator from pooltool.

The game's simulator (src/sim) is a C++ port of pooltool's event-based
physics. This script runs a set of reference shots through pooltool with the
same models and parameters and writes their outcome to
tests/data/golden_shots.json, which tests/test_sim_golden.cpp replays.

Run from the repository root (pooltool is only needed here, not by the game):

    uv run --python 3.12 --with pooltool-billiards==0.6.0 \
        tools/golden/generate_golden_shots.py

Frame: pooltool's, as in the simulator: z up, origin at a corner of the
playing surface, x across the table (width), y along it (length).

Known pooltool 0.6.0 defect: right after a ball changes motion state it can
report a ball-ball collision between balls that are apart and separating (in
"straight stop", 78 mm apart at t = 2.283 s); its kiss step then moves them
into contact. Such a reference is wrong from that event on, so each shot
records `valid_events` (how many reference events can be trusted) and
`final_valid` (whether the final positions can). The C++ simulator only
accepts approaching contacts and does not have this defect.
"""

from __future__ import annotations

import json
import math
from pathlib import Path

import numpy as np
import pooltool as pt
from pooltool.objects.table.specs import PocketTableSpecs
from pooltool.physics.engine import PhysicsEngine
from pooltool.physics.resolve.ball_cushion.han_2005.model import Han2005Circular, Han2005Linear
from pooltool.physics.resolve.resolver import default_resolver

OUTPUT = Path(__file__).resolve().parents[2] / "tests" / "data" / "golden_shots.json"

LENGTH = 2.54
WIDTH = 1.27
R = 0.028575

# Must match Sim::PocketTableSpec / assets/data/tables/table_9ft.json.
SPECS = PocketTableSpecs(
    l=LENGTH,
    w=WIDTH,
    cushion_width=2 * 0.0254,
    cushion_height=0.64 * 2 * R,
    corner_pocket_width=0.118,
    corner_pocket_angle=5.3,
    corner_pocket_depth=0.0417,
    corner_pocket_radius=0.062,
    corner_jaw_radius=0.02095,
    side_pocket_width=0.137,
    side_pocket_angle=7.14,
    side_pocket_depth=0.0685,
    side_pocket_radius=0.0645,
    side_jaw_radius=0.00795,
)

# The game uses Han 2005 cushions (pooltool's default is Stronge compliant).
RESOLVER = default_resolver()
RESOLVER.ball_linear_cushion = Han2005Linear()
RESOLVER.ball_circular_cushion = Han2005Circular()

EVENT_NAMES = {
    pt.EventType.STICK_BALL: "strike",
    pt.EventType.BALL_BALL: "ball_ball",
    pt.EventType.BALL_LINEAR_CUSHION: "linear_cushion",
    pt.EventType.BALL_CIRCULAR_CUSHION: "circular_cushion",
    pt.EventType.BALL_POCKET: "pocket",
}


def toward(a, b):
    """Cue direction phi (degrees) from point a to point b."""
    return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0])) % 360.0


def shots():
    centre_x = WIDTH / 2
    corner = (WIDTH, LENGTH)  # top-right corner pocket mouth (approximate aim point)
    diagonal = (1 / math.sqrt(2), 1 / math.sqrt(2))
    pot_object = (corner[0] - 0.25 * diagonal[0], corner[1] - 0.25 * diagonal[1])
    pot_cue = (pot_object[0] - 0.4 * diagonal[0], pot_object[1] - 0.4 * diagonal[1])
    cut_object = (centre_x + 0.1, 1.6)

    return [
        dict(name="straight stop", balls={"cue": (centre_x, 0.6), "1": (centre_x, 1.2)},
             V0=2.0, phi=90.0, a=0.0, b=-0.1),
        dict(name="follow", balls={"cue": (centre_x, 0.6), "1": (centre_x, 1.2)},
             V0=2.0, phi=90.0, a=0.0, b=0.4),
        dict(name="draw", balls={"cue": (centre_x, 0.6), "1": (centre_x, 1.2)},
             V0=2.5, phi=90.0, a=0.0, b=-0.5),
        dict(name="thin cut", balls={"cue": (centre_x, 0.8), "1": cut_object},
             V0=2.2, phi=toward((centre_x, 0.8), (cut_object[0] - 1.6 * R, cut_object[1])), a=0.0, b=0.0),
        dict(name="side spin into the side cushion", balls={"cue": (centre_x, 0.7)},
             V0=1.6, phi=10.0, a=0.4, b=0.0),
        dict(name="running english bank", balls={"cue": (0.4, 0.5)},
             V0=2.4, phi=60.0, a=-0.3, b=0.2),
        dict(name="pot into the corner", balls={"cue": pot_cue, "1": pot_object},
             V0=1.5, phi=45.0, a=0.0, b=0.0),
        dict(name="jaw hit", balls={"cue": (0.3, 1.9)},
             V0=1.8, phi=toward((0.3, 1.9), (0.08, LENGTH)), a=0.0, b=0.0),
        # The cue ball is 2 mm off centre: a perfectly symmetric hit reaches
        # the 2 and the 3 at the same instant, and the order two simulators
        # process an exact tie in is arbitrary.
        dict(name="three-ball cluster", balls={
                "cue": (centre_x + 0.002, 0.5),
                "1": (centre_x, 1.4),
                "2": (centre_x + R * 1.0001, 1.4 + math.sqrt(3) * R * 1.0001),
                "3": (centre_x - R * 1.0001, 1.4 + math.sqrt(3) * R * 1.0001)},
             V0=3.0, phi=90.0, a=0.0, b=0.0),
        dict(name="length of the table with draw", balls={"cue": (centre_x, 0.3)},
             V0=3.0, phi=88.0, a=0.0, b=-0.3),
        # Elevated cue: the spin axis tilts and the cue ball curves (swerve, masse).
        dict(name="swerve with an elevated cue", balls={"cue": (centre_x, 0.4)},
             V0=2.0, phi=90.0, theta=15.0, a=0.3, b=0.0),
        dict(name="masse", balls={"cue": (centre_x, 0.9)},
             V0=1.5, phi=90.0, theta=50.0, a=0.4, b=-0.2),
    ]


def run(shot):
    ids = list(shot["balls"].keys())
    balls = {i: pt.Ball.create(i, xy=shot["balls"][i]) for i in ids}
    system = pt.System(
        cue=pt.Cue(cue_ball_id="cue", V0=shot["V0"], phi=shot["phi"], theta=shot.get("theta", 0.0), a=shot["a"], b=shot["b"]),
        table=pt.Table.from_table_specs(SPECS),
        balls=balls,
    )
    pt.simulate(system, engine=PhysicsEngine(resolver=RESOLVER), inplace=True)

    index = {ball_id: n for n, ball_id in enumerate(ids)}
    events = []
    valid_events = None
    for event in system.events:
        name = EVENT_NAMES.get(event.event_type)
        if name is None:
            continue
        if name == "ball_ball" and valid_events is None and not plausible_contact(event):
            valid_events = len(events)
        ball_ids = [i for i in event.ids if i in index]
        record = {"type": name, "time": event.time, "ball": index[ball_ids[0]]}
        if name == "ball_ball":
            record["other"] = index[ball_ids[1]]
        events.append(record)

    final = []
    for ball_id in ids:
        state = system.balls[ball_id].state
        final.append({
            "position": [float(v) for v in state.rvw[0]],
            "pocketed": int(state.s) == 4,
        })

    return {
        "name": shot["name"],
        "balls": [list(shot["balls"][i]) for i in ids],
        "strike": {"speed": shot["V0"], "phi": shot["phi"], "theta": shot.get("theta", 0.0), "a": shot["a"], "b": shot["b"]},
        "duration": float(system.t),
        "events": events,
        "valid_events": len(events) if valid_events is None else valid_events,
        "final_valid": valid_events is None,
        "final": final,
    }


def plausible_contact(event):
    """True if the two balls touch and approach each other at the event."""
    a, b = [agent.initial.state.rvw for agent in event.agents]
    offset = b[0][:2] - a[0][:2]
    distance = float(np.linalg.norm(offset))
    closing = -float(np.dot(b[1][:2] - a[1][:2], offset)) / distance
    return abs(distance - 2 * R) < 1e-5 and closing > 0.0


def main():
    fixtures = {
        "generator": "tools/golden/generate_golden_shots.py",
        "pooltool": pt.__version__,
        "table": {"length": LENGTH, "width": WIDTH},
        "shots": [run(shot) for shot in shots()],
    }
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(fixtures, indent=1) + "\n")
    for shot in fixtures["shots"]:
        kinds = [e["type"] for e in shot["events"]]
        trust = "" if shot["final_valid"] else f'  reference valid for the first {shot["valid_events"]} events'
        print(f'{shot["name"]:36s} {shot["duration"]:6.2f} s  {len(kinds):3d} events{trust}')


if __name__ == "__main__":
    main()
