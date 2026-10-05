# Billiards Saloon — Blueprint

The documentation map: what the game is, how it works, and where it is going.
Start here after time away from the project.

## The game

**Billiards Saloon — The Tournament Hall** is a realism-first pool game
presented like a televised tournament. Four pillars:

| Pillar | In one line | Document |
|--------|-------------|----------|
| Authentic physics | An exact, event-based simulation of sliding, rolling, spin, cushions and pockets | [PHYSICS.md](design/PHYSICS.md) |
| Authentic rules | WPA 8-ball, 9-ball and 10-ball, called by an on-screen referee | [GDD.md](design/GDD.md#rules-scope-wpa) |
| Broadcast feel | Scorebug, referee banners, replays and a TV director camera over a live 3D hall | [UX.md](design/UX.md) |
| Intelligence | Self-play-trained AI pros, an AI coach, adaptive difficulty, generated challenges | [INTELLIGENCE.md](design/INTELLIGENCE.md) |

## Documents

| Document | Answers |
|----------|---------|
| [design/GDD.md](design/GDD.md) | What the player does: modes, rules, shot input, presentation, equipment and customization |
| [design/UX.md](design/UX.md) | How it looks and feels: design language, frontend flow, HUD states, accessibility, RmlUi approach |
| [design/PHYSICS.md](design/PHYSICS.md) | The physics: today's solver with its equations and parameters, and the event-based simulator that replaces it |
| [design/INTELLIGENCE.md](design/INTELLIGENCE.md) | The AI/ML plan: why search plus learned value and policy networks, the self-play pipeline, ONNX shipping, evaluation |
| [ARCHITECTURE.md](ARCHITECTURE.md) | The code: modules, targets, data flow of a shot, milestone order |
| [../CONTRIBUTING.md](../CONTRIBUTING.md) | How work flows: branches, commits, merges, releases |
| [../CHANGELOG.md](../CHANGELOG.md) | What changed in each version |
| [../CREDITS.md](../CREDITS.md) | Third-party libraries, fonts and their licences |

## How a shot flows through the system

```
mouse / keyboard ──► Input ──► ShotControls ──► MatchSession (aim, spin, power)
                                                    │ release
                                                    ▼
                                    physics: today a fixed-step solver,
                                    from M3 a full-shot event simulation
                                                    │ balls stop
                                                    ▼
                                    Rules (8-ball today, WPA RuleSet in M4)
                                                    │ ShotOutcome
                          ┌─────────────────────────┼──────────────────────────┐
                          ▼                         ▼                          ▼
                 HUD: referee banner,       camera rig / director       frame over card,
                 lower third, scorebug      (replays in M5)             match flow (M10)
```

## Roadmap

| Milestone | Status |
|-----------|--------|
| M0 Housekeeping, docs, knowledge graph | done |
| M1 Foundation: library split, tests, CI, JSON data, mouse controls, fullscreen | done (v0.2.0) |
| M2 UX foundation: RmlUi menus and HUD, settings, frontend flow, gamepad, accessibility | done (v0.3.0) |
| M3 Event-based physics | next |
| M4 WPA rules and referee | planned |
| M5 Shot input and broadcast presentation | planned |
| M6 Hall visuals, realism, equipment customization | planned |
| M7 Audio | planned |
| M8 AI v1: classical search | planned |
| M9 Intelligence: self-play learning (starts after M3, runs in parallel) | planned |
| M10 Modes: Quick Match, Practice, Trick Shots, The Tour | planned |
| M11 Ship: settings, packaging, release | planned |

## Working on the code

- Build, run, test and conventions: [../CLAUDE.md](../CLAUDE.md) and [../README.md](../README.md).
- Check visual changes: `./build/BilliardsSaloon --screen main|game|pause --capture shot.png`.
- Explore relationships in the code: the Graphify knowledge graph in `graphify-out/`
  (`graphify query "…"`, `graphify explain "MatchSession"`).
