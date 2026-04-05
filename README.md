# Billiards Saloon

A 3D billiards game with full spin physics, PBR rendering, and a saloon environment.
Built from scratch in C++20 / OpenGL 3.3 — no game engine.

## Architecture

- **ECS** — sparse-set entity/component system in `src/ecs/World.h`
- **Physics** — impulse-based rigid bodies with angular-linear spin coupling (`src/physics/`)
- **Renderer** — deferred PBR pipeline, G-buffer, multiple point lights (`src/renderer/`)
- **Game** — shot system, 8-ball rules, AI opponent (`src/game/`)

## Build

\`\`\`bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
./build/BilliardsSaloon
\`\`\`

## Dependencies

- GLFW (via Homebrew)
- GLAD (bundled in `third_party/`)
- GLM, nlohmann/json (via CMake FetchContent)