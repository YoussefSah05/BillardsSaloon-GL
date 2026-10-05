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

Requires CMake ≥ 3.28, a C++20 compiler and OpenGL 4.1. GLFW, GLM and doctest are
fetched automatically; GLAD is bundled in `external/`.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j8
./build/BilliardsSaloon
ctest --test-dir build --output-on-failure   # unit tests
```

## Controls

| Action | Mouse | Keyboard |
|--------|-------|----------|
| Aim | Move the mouse (hold Shift for fine aim) | A / D (Shift for fine aim) |
| Shoot | Hold left button, drag back for power, release | Hold Space, release |
| Cancel a shot | Push the mouse forward again and release | — |
| Spin (cue tip offset) | Hold right button and move | Arrow keys, C to centre |
| Camera views | — | Tab cycles, 1 aim, 2 overview, 3 follow, 4 free look |
| Free look | Right-drag to orbit, wheel to zoom | J/L orbit, I/K tilt, U/O zoom |
| Menus | Point and click | Up/Down, Enter |
| Pause | — | Esc |
| Fullscreen | Menu entry | F11, Alt+Enter, or Cmd+Ctrl+F on macOS |
| Graphics quality / FPS in title | — | F2 / F1 |

## Developer options

```bash
./build/BilliardsSaloon --screen game                      # skip the main menu
./build/BilliardsSaloon --screen pause --capture pause.png # save a screenshot and quit
```

## Project layout

| Path | Contents |
|------|----------|
| `src/app` | Main loop, menus, input, drawing |
| `src/ecs` | Sparse-set entity/component registry |
| `src/physics` | Ball, cushion and pocket simulation |
| `src/gameplay` | Game variants, match state, 8-ball rules |
| `src/render` | Shaders, meshes, cameras, overlay text, screenshots |
| `src/ui` | RmlUi integration (fonts, input, rendering) |
| `assets/ui` | Menu documents (`.rml`) and the shared stylesheet (`theme.rcss`) |
| `assets/shaders` | GLSL shaders |
| `docs` | Design document and architecture roadmap |

## Roadmap

See [`docs/design/GDD.md`](docs/design/GDD.md) for the v1.0 game design and
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the technical plan.
