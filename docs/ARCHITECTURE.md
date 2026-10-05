# Architecture

C++20, OpenGL 4.1 core (the macOS ceiling: no compute shaders, no DSA), GLFW,
GLM, no game engine. See `docs/design/GDD.md` for what the game must do.

## Current state

Two CMake targets: `bs_game`, a headless static library (no OpenGL/GLFW) that
tests link, and the `BilliardsSaloon` executable.

| Path | Target | Role |
|------|--------|------|
| `src/core/asset_paths.*` | bs_game | `resolveAssetPath`: exe-relative asset lookup (build tree, macOS bundle, source fallback). |
| `src/ecs/` | bs_game | Sparse-set `Registry` with `view<...>().each(...)`. |
| `src/scene/components.h` | bs_game | Transform (with previous state for interpolation), Ball, Material, TableBounds, Camera tags. |
| `src/gameplay/game_variant.*` | bs_game | Variant/table types, JSON loaders, rack layouts. |
| `src/gameplay/match_session.*` | bs_game | One rack of play: ECS world, shot state machine (aim, tip offset, charge, fire), physics step, rule resolution. |
| `src/gameplay/turn_rules.*` | bs_game | `Rules::resolveShot` (8-ball only; 9-ball currently reuses it). |
| `src/physics/billiards_physics.*` | bs_game | Fixed-step impulse solver: sliding→rolling cloth model, ball-ball and rail impulses, radius-based pocket capture. To be replaced by `sim/`. |
| `src/platform/` | app | `Window` (GLFW, vsync on), `Input` (key edge detection), `Timer`. |
| `src/render/` | app | `SceneRenderer` (frame setup, world, aim guide), `Shader`, `Mesh`, `Camera`, `camera_rig`, `ui_overlay` (voxel-font text). |
| `src/app/` | app | `Application` (loop, screens, camera rig), `saloon_scene` (room, lamps, ball materials), `overlay_screens` (HUD and menus). |
| `assets/data/` | — | Game data: `variants/*.json` (rules discipline, rack, balls) and `tables/*.json` (dimensions, pockets, physics coefficients). |
| `assets/shaders/basic.*` | — | Single forward shader with procedural cloth/wood finishes and ball patterns. |
| `tests/` | bs_tests | doctest suite: ECS, rules, physics characterization, session, data loading. |

Known limitations: the table is 2.84 × 1.42 m (a 10 ft table; WPA 9 ft is
2.54 × 1.27 m); max shot speed 3.8 m/s with ~3.2 cm per step (tunneling risk);
flat rails, no pocket jaws; no audio, AI, settings or saves.

## Target architecture (v1.0)

```
src/
  core/      logging, JSON config (nlohmann), event bus, asset path resolver, app state stack
  platform/  window, input actions & rebinding, gamepad, user-data dir
  sim/       headless, double precision, no GL — the physics library (bs_sim)
  game/      RuleSet (8/9/10-ball WPA), Referee, MatchController, modes, profiles & saves
  ai/        shot generation + evaluation through sim, difficulty profiles
  present/   ShotPlayback, camera director, replay, stats
  render/    passes: shadow → PBR main → transparent → post (HDR, bloom, tonemap)
  ui/        RmlUi menus/HUD bound to view models (assets/ui/*.rml, *.rcss); Dear ImGui for debug tools only
  audio/     miniaudio, driven by sim events
  ecs/       existing Registry
  python/    pybind11 bindings of the simulator and rules (optional build)
ml/          Python training project: env, agents, self-play, ONNX export (see design/INTELLIGENCE.md)
tests/       doctest unit, golden-shot and rules tests
```

### Shot data flow

```
input ──► ShotParams ──► sim::simulateShot(TableState, ShotParams) ──► ShotTrajectory
                                                                         │
            ┌──────────────────┬──────────────────┬──────────────────────┤
            ▼                  ▼                  ▼                      ▼
        Referee           ShotPlayback       event bus → audio,     Replay (params +
     (ShotVerdict →      (evaluate at t →     camera director,      start state =
     MatchController)     ECS transforms)     stats                 deterministic)
```

A shot is simulated completely at strike time. `ShotTrajectory` holds the
ordered events (collisions, cushion hits, pockets, motion transitions) and a
piecewise-analytic segment per ball, so playback, slow motion, replays, aim
prediction and AI evaluation all reuse the same exact result.

### Event-based simulator (`sim/`)

Following Leckie & Greenspan (2006) and pooltool (Kiefl, JOSS 2024):

- Ball motion states: stationary, spinning, sliding, rolling, airborne.
- Each step computes every candidate event time (transitions: linear/quadratic;
  ball-ball and ball-pocket: quartic; cushion: quadratic/quartic), advances all
  balls analytically to the earliest event, resolves it, repeats.
- Resolvers: stick-ball (squirt/swerve, elevation), ball-ball (frictional
  inelastic, later Mathavan), cushion (Han 2005, later Mathavan 2010), pocket,
  transitions.
- Table geometry: linear cushion segments, circular jaw corners, pocket
  circles, WPA 9 ft dimensions.

### Milestone order

M0 housekeeping ✅ · M1 foundation ✅ · M2 UX foundation and broadcast
frontend · M3 event-based physics · M4 WPA rules and referee · M5 shot input
and presentation · M6 hall visuals, realism and customization · M7 audio ·
M8 AI v1 (classical search) · M9 intelligence (self-play learning, starts
after M3 and runs in parallel) · M10 modes · M11 ship.
UX comes before physics because every later milestone presents itself
through it (referee banners, ball in hand, opponent cards, season hub).

### Git workflow

- `main` is always buildable. One branch per milestone (`feat/sim-core`,
  `feat/rules-wpa`, ...), Conventional Commits, merged via PR with green CI.
- `v0.1.0-prototype` tags the pre-rewrite baseline.
- The old solver stays behind a backend switch until the event simulator passes
  golden-shot tests, then is removed in one dedicated commit.

## References

- Leckie & Greenspan, *An Event-Based Pool Physics Simulator* (ACG 2005, LNCS 4250).
- Kiefl, *Pooltool: A Python package for realistic billiards simulation*, JOSS 2024 —
  https://pooltool.readthedocs.io, https://ekiefl.github.io/2020/12/20/pooltool-alg/
- Han, *Dynamics in Carom and Three Cushion Billiards*, JMST 19(4), 2005.
- Mathavan et al., *A theoretical analysis of billiard ball dynamics under cushion impacts*, 2010.
- Dr. Dave Alciatore, pool physics resources — https://drdavepoolinfo.com/physics/
- WPA Rules of Play (2025-09-15) — https://wpapool.com
