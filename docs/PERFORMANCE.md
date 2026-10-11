# Performance

Build the game in Release for play. Debug remains useful when stepping through
code, but its unoptimized collision solver makes continuously changing aim
and computer planning much slower.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
ctest --test-dir build --output-on-failure
./build/BilliardsSaloon --screen game --camera overview --profile --mute
```

## Bottlenecks and fixes

- Collision root searches recursively allocated vectors of roots and turning
  points for every candidate collision. These degree-four polynomials have
  bounded scratch requirements, so the internal search now uses stack arrays.
  The numerical method, tolerances and event ordering are unchanged.
- Aim guides sampled every ball at 60 Hz and scanned the event list from its
  beginning for every sample. They now sample just the cue and contacted ball;
  both preview and playback locate the last event with binary search.
  Simulation horizon scratch is allocated once per shot, and guide paths
  reserve their expected sample count. Layout/turn changes invalidate guides.
- Every lamp redrew all shadow casters on every frame, even during idle aiming
  or camera movement. Caster meshes, interpolated transforms and bounds are
  resolved once for all lamps; depth maps are reused when the casters, lamp
  matrices and map size match. Moving balls, placement, pockets, changed
  geometry and quality changes invalidate the cache. Exact stable transforms
  avoid interpolation round-off invalidating stationary scenes.
- Low/Balanced rendered multisampled HDR, expensive lighting and bloom at the
  native Retina resolution. Their scene resolution now fits 1280 × 720 /
  1920 × 1080 respectively, with aspect ratio preserved and no upscaling of
  smaller windows. High stays native; the final presentation, menus and HUD
  always use native pixels. GLFW documents the distinction between window
  coordinates and framebuffer pixels in its [window guide](https://www.glfw.org/docs/latest/window_guide.html#window_fbsize).
- The timer-query ring read `GL_QUERY_RESULT` unconditionally. A GPU more than
  four frames behind could block the CPU. Results are now read only when
  `GL_QUERY_RESULT_AVAILABLE` is true; an occupied slot skips measurement
  instead of waiting or overwriting its pending result. See Khronos's
  [query-object guidance](https://wikis.khronos.org/opengl/Query_Object).
- `GL_MAX_SAMPLES` was queried every frame; it is now cached per render target.
  Frame reports use actual elapsed time, independently of the gameplay delta
  clamp, so long frames are visible in the report.

## Measurements

Measured on the same macOS machine on 9 October 2026, using Release, the
saloon hall, overview camera, Balanced quality and the same 3110 × 1800
framebuffer. The optimized scene is 1866 × 1080; UI remains 3110 × 1800.
Frame/GPU figures below are representative one-second windows after startup,
with fresh racks per run. They are observations, not a guarantee for other
hardware, hall views, quality presets or shots.

| Work | Before | After |
| --- | ---: | ---: |
| Elapsed frame time, including presentation/UI | 15.8 ms | 9.6 ms |
| GPU scene, shadows, guides and post processing | 10.9 ms | 5.2 ms |
| World + shadow draw calls while stationary | 305 | 56 |
| World + shadow triangles while stationary | 346,378 | 136,254 |
| Changing aim preview, average of 240 inputs | 8.64 ms | 2.83 ms |

The preview benchmark applied 0.0005 radians of aim change per input at
1/120 second to a new eight-ball session. Both versions returned 83,564 cue
path samples across the 240 previews. The full test suite checks collision
events and final positions against the existing pooltool golden fixtures.
Additional tests cover single-ball versus full-table sampling (event
boundaries, simultaneous events, midpoints and times past the shot), stable
stationary transforms, layout invalidation and resolution presets.

`--profile` reports elapsed frame time (including swap/vsync, rather than
CPU-only work), delayed GPU samples, internal scene resolution and renderer
counts. Draw/triangle counters cover the world and shadow passes; they exclude
cue, guides, post processing, probe capture and UI. Skipped cached shadows
therefore report zero shadow casters for that frame.

To check actual playback and cache invalidation visually:

```bash
./build/BilliardsSaloon --screen game --camera overview --profile --mute --capture /tmp/game.png --capture-frames 360
./build/BilliardsSaloon --scenario replay --profile --mute --capture /tmp/replay.png --capture-frames 120
./build/BilliardsSaloon --screen pause --mute --capture /tmp/pause.png
```

The hall's two-bounce probe/reflection capture still runs once after furniture
loading (about 300 ms in this measurement). This can cause an initial hitch
or a hitch when changing hall lighting. It is separate from steady gameplay
cost and is not spread across frames by these changes.
