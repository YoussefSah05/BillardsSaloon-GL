# Billiards Saloon

A realism-first 3D pool game presented like a televised tournament.
Built from scratch in C++20 and OpenGL 4.1 — no game engine.

> **Status:** early development. The `v0.1.0-prototype` tag is a playable
> 8-ball prototype. Work toward v1.0 (event-based physics, full WPA rules,
> AI opponents, career mode) is tracked in [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

## What works today

- 8-ball against a second local player: break, open table, group assignment,
  first-contact and scratch fouls, turn changes, win/loss on the 8.
- Cue strike with power and tip offset (english, follow, draw).
- Physics: sliding→rolling cloth model with spin, ball-ball and cushion
  impulses with friction, pocket capture.
- Camera modes: aim, table overview, shot follow, free look.
- Main and pause menus, Low/Balanced/High render quality.

## Build

Requires CMake ≥ 3.28, a C++20 compiler and OpenGL 4.1. GLFW and GLM are
fetched automatically; GLAD is bundled in `external/`.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j8
cd build && ./BilliardsSaloon
```

## Project layout

| Path | Contents |
|------|----------|
| `src/app` | Main loop, menus, input, drawing |
| `src/ecs` | Sparse-set entity/component registry |
| `src/physics` | Ball, cushion and pocket simulation |
| `src/gameplay` | Game variants, match state, 8-ball rules |
| `src/render` | Shaders, meshes, cameras, overlay text |
| `assets/shaders` | GLSL shaders |
| `docs` | Design document and architecture roadmap |

## Roadmap

See [`docs/design/GDD.md`](docs/design/GDD.md) for the v1.0 game design and
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the technical plan.
