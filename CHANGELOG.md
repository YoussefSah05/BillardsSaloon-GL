# Changelog

All notable changes to Billiards Saloon. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

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
