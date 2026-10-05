# Billiards Saloon — Physics

Status: the prototype solver ships today; the event-based simulator is
milestone M3. Last revised 2026-10-05. Companion to `GDD.md` and
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

## Part 1 — The prototype solver (shipping now)

Source: `src/physics/billiards_physics.cpp`. Fixed time step of 1/120 s,
called from `MatchSession::step` (`src/gameplay/match_session.cpp`).

### Step order

Each step:

1. Integrate positions: `x += v · Δt`.
2. Pocket capture, cushion contacts, then four solver passes of
   ball–ball → pocket → cushion.
3. Cloth friction (the motion-state model below), in up to four sub-intervals
   so a ball can change state mid-step.
4. Visual rotation: each ball's orientation turns by `|ω|·Δt` about `ω̂`.

### Cloth contact: motion states

| State | Condition | Equations |
|-------|-----------|-----------|
| **Sliding** | `|u| > 0.0015 m/s` | `v̇ = −μ_s·g·û`; `ω̇ = (5·μ_s·g / 2R) · (ŷ × û)`; the slip shrinks as `u̇ = −(7/2)·μ_s·g·û`, so the ball starts rolling after `τ_s = 2·|u₀| / (7·μ_s·g)`. |
| **Rolling** | slip ≈ 0, speed > stop threshold | `v̇ = −μ_r·g·v̂`; `ω` stays locked to `(ŷ × v)/R`; stops after `τ_r = |v| / (μ_r·g)`. |
| **Spinning** | no translation, `|ω_y| > 0.01 rad/s` | `ω̇_y = −sign(ω_y)·μ_sp·g / R` (see note). |
| **Stationary** | otherwise | `v = ω = 0`. |

Vertical spin (`ω_y`) also decays at `μ_sp·g/R` while sliding or rolling.

Note: physically, a spinning ball decelerates at `ω̇_y = −(5/2)·μ_sp·g / R`
(pooltool's model). The prototype omits the 5/2 factor; the event-based
simulator uses the physical form.

These formulas are checked by unit tests (`tests/test_physics.cpp`): a stun
shot settles at 5/7 of its speed, a rolling ball stops after
`v² / (2·μ_r·g)`, and kinetic energy never increases.

### Ball–ball collisions

- Detection: centres closer than `2R` (no swept test).
- Penetration is pushed apart by 80% of the overlap beyond 0.1 mm.
- **Normal impulse:** `J_n = −(1 + e_b) · (v_rel · n) / (1/m_a + 1/m_b)`, with
  `v_rel` the relative velocity of the contact points.
- **Friction impulse:** opposes the remaining tangential contact velocity,
  sized to stop it but capped at `μ_b · J_n` (Coulomb). This produces throw
  and transfers spin between balls.
- The first object ball the cue ball touches is recorded for the rules.

### Cushions

- The playable area is a box `[−X + R, X − R] × [−Z + R, Z − R]`; a ball past it
  is clamped back to the boundary.
- Contact at the ball's equator, normal impulse with restitution `e_c`,
  tangential friction capped at `μ_c · J_n` (so side spin changes the rebound angle).

### Pockets

- A ball is captured when its centre comes within the pocket radius of a pocket
  centre (four corners, two sides). Captured balls leave the simulation.

### Cue strike

`MatchSession::fireShot`: speed `V = 0.4 + 3.4 · power` m/s along the aim
line; tip offset sets spin directly:

- side spin: `ω_y = 0.85 · s_right · V / R`
- top/back spin: `ω_⊥ = s_forward · V / R` about the horizontal axis
  perpendicular to the aim

`s_right` and `s_forward` lie inside a disc of radius 0.75.

### Parameters

All in `assets/data/tables/table_10ft.json` (loaded by `loadTableSpecification`):

| Parameter | Value | Meaning |
|-----------|-------|---------|
| Cloth | 2.84 × 1.42 m | playing surface (a 10 ft table; WPA 9 ft is 2.54 × 1.27 m) |
| `ballRadius`, `ballMassKg` | 0.028575 m, 0.17 kg | 2¼ in pool balls |
| `cloth.slidingFriction` μ_s | 0.20 | typical 0.2 |
| `cloth.rollingFriction` μ_r | 0.010 | typical 0.005–0.015 |
| `cloth.spinningFriction` μ_sp | 0.015 | |
| `cloth.stopSpeed` | 0.006 m/s | below this a rolling ball stops |
| `ballContact.restitution` e_b, `friction` μ_b | 0.96, 0.05 | gameplay-tuned |
| `cushion.restitution` e_c, `friction` μ_c | 0.92, 0.14 | gameplay-tuned |
| `pockets.cornerRadius`, `sideRadius` | 0.090, 0.080 m | capture radius |

### Known limitations (why M3 exists)

- At full power a ball moves 3.2 cm per step, more than its radius: fast balls
  can pass through cushions or pocket edges (tunnelling).
- Full power is 3.8 m/s; real breaks reach 8–10 m/s.
- Cushions are flat boxes contacted at the equator; real cushions contact
  above it (nose height) and have rounded pocket jaws.
- Pockets are capture circles: no jaws, no rattles.
- No squirt (cue-ball deflection), swerve or massé; no elevation.
- Results depend on the step size, so replays and AI search are approximate.

## Part 2 — The event-based simulator (milestone M3)

Library `bs_sim`, headless, double precision, deterministic. Design follows
Leckie & Greenspan and pooltool (Kiefl).

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

### Table geometry

WPA 9 ft table: 2.54 × 1.27 m playing surface, cushion nose at about 63.5% of
ball diameter, corner and side pockets built from straight cushion segments,
jaw arcs and pocket circles. Defined in the table JSON, so equipment variants
are data.

### Validation

- Closed-form checks for every motion state and transition time.
- Golden shots (stop, follow and draw shots, banks, a break) compared with
  pooltool's output within tolerance; stored as JSON fixtures.
- Kinetic energy never increases; identical inputs give identical trajectories.
- The old solver stays behind a backend switch until the new one passes these
  tests and play-tests, then is removed.

## References

- Leckie & Greenspan, *An Event-Based Pool Physics Simulator*, Advances in Computer Games 2005 (LNCS 4250).
- Leckie & Greenspan, *Pool Physics Simulation by Event Prediction 1: Motion Transitions*, ICGA Journal 2006.
- Kiefl, *Pooltool: A Python package for realistic billiards simulation*, JOSS 2024 — https://pooltool.readthedocs.io
- Han, *Dynamics in Carom and Three Cushion Billiards*, Journal of Mechanical Science and Technology 19(4), 2005.
- Mathavan, Jackson, Parkin, *A theoretical analysis of billiard ball dynamics under cushion impacts*, 2010.
- Alciatore, *The Illustrated Principles of Pool and Billiards* and technical proofs — https://drdavepoolinfo.com/physics/
- WPA, *Rules of Play* and equipment specifications — https://wpapool.com
