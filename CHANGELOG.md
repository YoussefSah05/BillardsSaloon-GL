# Changelog

All notable changes to Billiards Saloon. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Main and pause menus rebuilt with RmlUi: broadcast-style slanted bars,
  Barlow typefaces, hover and keyboard focus states, a blurred pause overlay.
- In-match HUD rebuilt with RmlUi: broadcast scorebug with the player at the
  table, ball trays once groups are set, power meter with a marker for the
  last shot's power, spin widget, control prompts and camera label.
- Referee banners that name every foul (scratch, no ball hit, wrong ball
  first) and how the frame ended; lower thirds for ball in hand and turn
  changes; a frame-over card with Next frame and Main menu.
- Settings screen (main and pause menus): fullscreen, VSync, graphics
  quality, mouse sensitivity, text and UI size (100–150%) and reduced
  motion. Changes apply immediately and are saved to the user's settings
  file (`~/Library/Application Support/Billiards Saloon/settings.json` on
  macOS, `%APPDATA%` on Windows, `~/.config` on Linux).
- Title screen over the live hall ("Press any key", build version), then
  the main hub with the table framed beside the menu. The menu camera moves
  slowly over the table (still with Reduced Motion); pause and frame-over
  keep the match view.
- Gamepad support (standard mapping, Xbox layout): menus by D-pad or
  stick with hold-to-repeat, A to confirm, B to go back; left stick aims
  (LB for fine aim), right stick sets spin or orbits the free-look camera,
  hold A to charge, Y cycles cameras, Start pauses.
- Prompts and hints switch between keyboard/mouse and gamepad wording
  depending on the device used last.
- Leaving the match or restarting the rack from the pause menu asks for
  confirmation first, with Cancel as the default.
- Control prompts show for the first few shots, then step aside.
- `--screen title|main|game|pause|settings`, `--fullscreen` and `--capture FILE.png`
  launch options; capture renders a screen, saves a PNG and quits.

### Changed
- Starting or resuming a match ignores the click or key that triggered it
  until it is released, so a menu click never starts a shot.
- The scorebug stays readable under the pause overlay, which dims and blurs the table.

### Removed
- The voxel-text HUD and menus built from cubes.

### Fixed
- Edited UI documents, shaders and data were only copied to the build
  folder when code also changed.
- The HUD reflows at large UI sizes instead of overlapping.
- Material colours were gamma-corrected twice, washing out the whole scene.

## [0.2.0] - 2026-10-05

Milestone M1: foundation for shipping, plus mouse controls and fullscreen.

### Added
- Mouse controls: move to aim (Shift for fine aim), hold the left button and
  drag back to set power, release to shoot; right button moves the cue tip for
  spin; menus follow the pointer and accept clicks.
- Fullscreen: F11, Alt+Enter, Cmd+Ctrl+F on macOS, or the menu entry.
- Game variants and table specifications load from JSON in `assets/data/`.
- Unit test suite (doctest) and CI on macOS, Linux and Windows.
- Design documents: game design, UX and frontend, intelligence (AI/ML), architecture roadmap.

### Changed
- Aiming, spin and power charging use per-second rates, so they feel the same
  at any frame rate.
- VSync is on by default.
- The game finds its assets next to the executable and runs from any folder.
- Pausing while charging a shot cancels that shot.
- The respot key (R) only works in Debug builds.

### Fixed
- Power charged per frame, so at high frame rates shots reached full power
  almost instantly.
- On HiDPI (Retina) displays the game drew into a corner of the window until
  it was resized.
- On-screen control hints overlapped.

## [0.1.0-prototype] - 2026-10-04

Playable 8-ball prototype: local two-player 8-ball with fouls and turn
resolution, a fixed-step physics model with sliding and rolling, camera modes,
and main and pause menus.

[Unreleased]: https://github.com/YoussefSah05/BillardsSaloon-GL/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/YoussefSah05/BillardsSaloon-GL/compare/v0.1.0-prototype...v0.2.0
[0.1.0-prototype]: https://github.com/YoussefSah05/BillardsSaloon-GL/releases/tag/v0.1.0-prototype
