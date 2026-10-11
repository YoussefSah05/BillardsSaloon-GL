# Septhuit — Blueprint

The documentation map: what the game is, how it works, and where it is going.
Start here after time away from the project. The design documents explain
the theory behind each part (models, equations, algorithms and the reasons
for each choice), not just the code. Equations are written in LaTeX, which
GitHub renders.

## The game

**Septhuit — The Tournament Hall** is a realism-first pool game
presented like a televised tournament. Four pillars:

| Pillar | In one line | Document |
|--------|-------------|----------|
| Authentic physics | An exact, event-based simulation of sliding, rolling, spin, cushions and pockets | [PHYSICS.md](design/PHYSICS.md) |
| Authentic rules | WPA 8-ball, 9-ball and 10-ball, called by an on-screen referee | [RULES.md](design/RULES.md) |
| Broadcast feel | Scorebug, referee banners, replays and a TV director camera over a live 3D hall | [UX.md](design/UX.md) |
| Intelligence | Search-based AI pros guided by a trained neural evaluator, adaptive difficulty; later an AI coach and generated challenges | [INTELLIGENCE.md](design/INTELLIGENCE.md) |

## Documents

| Document | Answers |
|----------|---------|
| [design/GDD.md](design/GDD.md) | What the player does: modes, rules, shot input, presentation, equipment and customization |
| [design/RULES.md](design/RULES.md) | The referee: WPA clauses enforced, simplifications, code and tests |
| [design/UX.md](design/UX.md) | How it looks and feels: design language, frontend flow, HUD states, accessibility, RmlUi approach |
| [design/PHYSICS.md](design/PHYSICS.md) | The physics: equations of motion on cloth, event detection, collision models (ball, cushion, cue), validation |
| [design/RENDERING.md](design/RENDERING.md) | The image: microfacet shading, lamps, cloth sheen, shadows, bloom, tone mapping, procedural table geometry |
| [design/AUDIO.md](design/AUDIO.md) | The sound: the recorded sample bank, from simulator events to scheduled, layered and positioned cues, mixing without clipping |
| [design/AI.md](design/AI.md) | The computer opponents: ghost-ball geometry, error propagation, expected utility under execution noise, position value |
| [design/INTELLIGENCE.md](design/INTELLIGENCE.md) | The AI/ML plan and its theory: value functions, proper scoring, set encoders, TD learning, expert iteration, Elo, Bayesian skill estimation |
| [design/ADAPTIVE_AI.md](design/ADAPTIVE_AI.md) | The shipped learned evaluator and the Adaptive opponent: architecture, league training, held-out results, skill filter |
| [design/RELIABLE_AI.md](design/RELIABLE_AI.md) | The gated graph Transformer ensemble experiment, calibration, release gates and runtime safeguards |
| [rl/current-game-audit.md](rl/current-game-audit.md) | What the simulator offers reinforcement learning, measured: API, throughput, determinism, the narrow potting windows, risks |
| [rl/rl-system-design.md](rl/rl-system-design.md) | The RL system: shot-level API, `SingleShotEnv`, baselines, evaluation protocol, algorithms and milestones |
| [rl/experiments.md](rl/experiments.md) | The RL research log: thirteen experiments from SAC on single shots to self-play under WPA rules, with results |
| [rl/environment.md](rl/environment.md), [rl/reward-design.md](rl/reward-design.md), [rl/papers.md](rl/papers.md) | Environment reference, reward shaping, reading list |
| [PERFORMANCE.md](PERFORMANCE.md) | Frame-time bottlenecks, the fixes and before/after measurements |
| [ARCHITECTURE.md](ARCHITECTURE.md) | The code: modules, targets, data flow of a shot, milestone order |
| [../CONTRIBUTING.md](../CONTRIBUTING.md) | How work flows: branches, commits, merges, releases |
| [../CHANGELOG.md](../CHANGELOG.md) | What changed in each version |
| [../CREDITS.md](../CREDITS.md) | Third-party libraries, fonts and their licences |

## How a shot flows through the system

```
mouse / keyboard ──► Input ──► ShotControls ──► MatchSession (aim, spin, power)
                                                    │ release
                                                    ▼
                                    sim: the whole shot is simulated, then
                                    played back (ShotTrajectory)
                                                    │ balls stop
                                                    ▼
                                    Rules::judgeShot (WPA 8/9/10-ball)
                                                    │ Verdict → ShotOutcome
                          ┌─────────────────────────┼──────────────────────────┐
                          ▼                         ▼                          ▼
                 HUD: referee banner,       camera rig / director       frame over card,
                 lower third, scorebug      director cuts, replays             match flow (M10)
```

## Roadmap

| Milestone | Status |
|-----------|--------|
| M0 Housekeeping, docs, knowledge graph | done |
| M1 Foundation: library split, tests, CI, JSON data, mouse controls, fullscreen | done (v0.2.0) |
| M2 UX foundation: RmlUi menus and HUD, settings, frontend flow, gamepad, accessibility | done (v0.3.0) |
| M3 Event-based physics | done (v0.4.0) |
| M4 WPA rules and referee | done (v0.4.0) |
| M5 Shot input and broadcast presentation | done (v0.5.0); jump shots wait for airborne-ball physics |
| M6 Hall visuals, realism, equipment customization | done (v0.6.0) |
| M7 Audio | done (v0.7.0); recorded bank in Unreleased; referee voice later |
| M8 AI v1: classical search | done (v0.8.0) |
| M9 Intelligence: learned evaluator, Adaptive opponent, RL research | in progress (Unreleased) |
| M10 Modes: Quick Match, Practice, Trick Shots, The Tour | planned |
| M11 Ship: settings, packaging, release | planned |

## Working on the code

- Build, run, test and conventions: [../CONTRIBUTING.md](../CONTRIBUTING.md) and [../README.md](../README.md).
- Check visual changes: `./build/Septhuit --screen main|game|pause --capture shot.png`;
  record a clip with `--record DIR --capture-frames N` (see the README's developer options).
- Explore relationships in the code: the Graphify knowledge graph in `graphify-out/`
  (`graphify query "…"`, `graphify explain "MatchSession"`).
