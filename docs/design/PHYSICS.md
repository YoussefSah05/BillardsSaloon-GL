# Billiards Saloon — Physics

Status: the event-based simulator (Part 2) is the game's physics. The
prototype solver (Part 1) was removed in 0.4.0, once the simulator matched
pooltool's golden shots and passed play-testing. Last revised 2026-10-05. Companion to `GDD.md` and
`INTELLIGENCE.md`.

## Conventions

- SI units: metres, seconds, kilograms, radians.
- World axes: **y is up**, the cloth is the plane y = 0, x runs along the
  table, z across it. Ball centres rest at y = R.
- `v` is a ball's linear velocity, `ω` its angular velocity, `R` its radius,
  `m` its mass, `g = 9.81 m/s²`.
- Moment of inertia of a solid ball: `I = 2/5 · m · R²`.
- **Contact-point slip** with the cloth (the point at `−R·ŷ`):
  `u = v + ω × (−R·ŷ)`. In components: `u = (v_x + R·ω_z, 0, v_z − R·ω_x)`.
  A ball rolls without slipping when `u = 0`, i.e. `ω = (ŷ × v) / R`.

## Part 1 — The prototype solver (removed in 0.4.0)

Up to 0.3.0 the game stepped a fixed-step impulse solver at 120 Hz: a
sliding→rolling cloth model, ball-ball and flat-rail impulses with friction,
and radius-based pocket capture. It lives in git history
(`src/physics/billiards_physics.*` before 0.4.0). Its limitations are the
reason the event-based simulator exists:

- At full power a ball moves 3.2 cm per step, more than its radius: fast balls
  can pass through cushions or pocket edges (tunnelling).
- Full power is 3.8 m/s; real breaks reach 8–10 m/s.
- Cushions are flat boxes contacted at the equator; real cushions contact
  above it (nose height) and have rounded pocket jaws.
- Pockets are capture circles: no jaws, no rattles.
- No squirt (cue-ball deflection), swerve or massé; no elevation.
- Results depend on the step size, so replays and AI search are approximate.

## Part 2 — The event-based simulator (milestone M3)

Library `bs_sim` (`src/sim/`), headless, double precision, deterministic.
Ported from pooltool (Kiefl; Apache-2.0) and Leckie & Greenspan. Internally it
uses pooltool's frame (z up, origin at a corner of the playing surface);
`src/gameplay/sim_bridge.h` converts to and from the game's frame.

| File | Contents |
|------|----------|
| `sim/roots.*` | Real roots of degree ≤ 4 polynomials in a time window: split at turning points, safeguarded Newton–bisection; `firstClosingRoot` ignores grazes |
| `sim/motion.*` | Motion states, transition times, closed-form evolution, position polynomials |
| `sim/table.*` | Pocket-table geometry: 18 straight cushion segments, 12 jaw tips, 6 pockets |
| `sim/events.*` | Ball–ball, cushion (at the nose height), jaw-tip and pocket event times |
| `sim/resolve.*` | Cue strike with squirt, frictional ball–ball, Han 2005 cushion, pocket |
| `sim/simulate.*` | The event loop and `ShotTrajectory` (exact state at any time) |

In the game, `MatchSession` simulates the whole shot when the cue is released,
reads first contact and pocketed balls from the events for the referee, and
plays the trajectory back in real time. A full break simulates in 5–15 ms.

### Idea

Between collisions every ball follows a closed-form trajectory for its motion
state. Instead of stepping time, the simulator computes the time of every
possible next event, advances all balls analytically to the earliest one,
resolves it, and repeats. A whole shot is simulated at the moment of the
strike and returned as a `ShotTrajectory`:

```
events:   [t0 strike] [t1 sliding→rolling] [t2 ball–ball 0/3] [t3 cushion 3] ... [tN all stationary]
segments: per ball, piecewise closed-form motion between its events
```

Playback evaluates the segments at any time `t`, so slow motion, replays, aim
prediction and AI search all reuse the same exact result.

### Motion states and their equations

Same physics as Part 1, solved in closed form from the state's start time:

- **Sliding:** `r(t) = r₀ + v₀·t − ½·μ_s·g·t²·û₀`, `v(t) = v₀ − μ_s·g·t·û₀`,
  `ω(t) = ω₀ + (5·μ_s·g/2R)·t·(ŷ × û₀)`; the slip direction `û₀` is constant
  while sliding.
- **Rolling:** `r(t) = r₀ + v₀·t − ½·μ_r·g·t²·v̂₀`, `ω = (ŷ × v)/R`.
- **Spinning:** `ω_y(t) = ω_y0 − (5/2)·(μ_sp·g/R)·t`.
- **Airborne** (jumps): ballistic flight until the next cloth contact.

### Event times

| Event | Equation in t |
|-------|---------------|
| sliding → rolling | linear: `t = 2·|u₀| / (7·μ_s·g)` |
| rolling → spinning/stationary | linear: `t = |v₀| / (μ_r·g)` |
| spinning → stationary | linear: `t = 2R·|ω_y0| / (5·μ_sp·g)` |
| ball–ball | quartic: `|r_a(t) − r_b(t)|² = (2R)²` (positions are quadratic in t) |
| ball–straight cushion | quadratic: distance to the cushion line equals R |
| ball–cushion corner / pocket jaw | quartic: distance to a circle |
| ball–pocket | quartic: centre crosses the pocket circle |

Roots come from a robust quartic solver polished with Newton iterations; only
the smallest positive real root that is geometrically valid (inside the
cushion segment, approaching) counts.

### Resolvers

- **Cue strike:** instantaneous point contact between tip and ball, giving
  speed, spin from the tip offset and elevation, plus **squirt** (the cue ball
  deflects away from the side spin) and, with elevation, **swerve** and **massé**.
- **Ball–ball:** frictional inelastic collision (throw and spin transfer),
  later Mathavan et al.'s model.
- **Cushion:** Han 2005, which contacts the ball at the cushion's nose height
  and couples spin and rebound; later Mathavan et al. 2010.
- **Pocket:** the ball drops when its centre crosses the pocket circle; jaws
  are cushion segments and circular arcs, so balls can rattle.

### Table geometry and parameters

WPA 9 ft table: 2.54 × 1.27 m playing surface, cushion nose at 64% of ball
diameter, corner and side pockets built from straight cushion segments, jaw
arcs and pocket circles (pooltool's pocket geometry). Everything lives in
`assets/data/tables/table_9ft.json` under `simulation`:

| Parameter | Value |
|-----------|-------|
| Ball | R = 0.028575 m, m = 0.170097 kg |
| Cloth | μ_s = 0.2, μ_r = 0.01, μ_sp = 0.444·R |
| Ball–ball | e = 0.95, friction μ = 0.00995 + 0.108·e^(−1.088·v_slip) (Alciatore) |
| Cushion | e = 0.85, μ = 0.2, height 0.0366 m, nose radius 1 mm |
| Corner pocket | mouth 0.118 m, angle 5.3°, depth 0.0417 m, radius 0.062 m, jaw radius 0.021 m |
| Side pocket | mouth 0.137 m, angle 7.14°, depth 0.0685 m, radius 0.0645 m, jaw radius 0.008 m |
| Cue | mass 0.567 kg, squirt end mass m/30; game power maps to 0.5–7 m/s cue speed |

Cue elevation (0–60° in the game) tilts the strike's spin axis, so off-centre
elevated hits swerve and massé; the strike's vertical velocity is dropped, so
there are no jump shots or airborne balls yet.

### Validation

- Closed-form checks for every motion state and transition time.
- Golden shots (stop, follow and draw shots, banks, a break) compared with
  pooltool's output within tolerance; stored as JSON fixtures.
- Kinetic energy never increases; identical inputs give identical trajectories.
- The old solver was kept behind a switch until the new one passed the golden
  shots and play-tests, then removed (0.4.0).
- Done: closed-form checks, randomised root-finder tests, determinism, energy
  monotonicity, a break with no overlaps at rest, and golden shots.

### Golden shots against pooltool

`tools/golden/generate_golden_shots.py` runs twelve reference shots through
pooltool 0.6.0, set up like the game (9 ft table, Han 2005 cushions, the same
ball, cue and pocket parameters). It writes `tests/data/golden_shots.json`,
and `tests/test_sim_golden.cpp` replays the shots through `bs_sim`. The shots
are stop, follow, draw, a thin cut, side spin into a cushion, a running-english
bank, a pot, a jaw hit, a three-ball cluster, a length-of-table draw, a swerve
(cue up 15°) and a massé (cue up 50°).

Result: the same collisions and pockets in the same order. Final positions
agree within 0.43 mm (typically under 0.1 mm), and event times within about
0.03%. The test allows 1 mm and 0.1% (relative).

pooltool 0.6.0 has a defect these shots expose. Right after a ball changes
motion state, it can report a collision between two balls that are apart and
separating. In the stop shot, at t = 2.283 s, the balls are 78 mm apart and
separating at 0.34 m/s. Its kiss step then moves them into contact and swaps
their velocities, and this repeats every ~15 ms. The generator detects such
events, and the fixture keeps the reference only up to the first one.
`bs_sim` accepts only approaching contacts and does not have the defect.
Regenerate with:

```bash
uv run --python 3.12 --with pooltool-billiards==0.6.0 tools/golden/generate_golden_shots.py
```

## References

- Leckie & Greenspan, *An Event-Based Pool Physics Simulator*, Advances in Computer Games 2005 (LNCS 4250).
- Leckie & Greenspan, *Pool Physics Simulation by Event Prediction 1: Motion Transitions*, ICGA Journal 2006.
- Kiefl, *Pooltool: A Python package for realistic billiards simulation*, JOSS 2024 — https://pooltool.readthedocs.io
- Han, *Dynamics in Carom and Three Cushion Billiards*, Journal of Mechanical Science and Technology 19(4), 2005.
- Mathavan, Jackson, Parkin, *A theoretical analysis of billiard ball dynamics under cushion impacts*, 2010.
- Alciatore, *The Illustrated Principles of Pool and Billiards* and technical proofs — https://drdavepoolinfo.com/physics/
- WPA, *Rules of Play* and equipment specifications — https://wpapool.com
