# Changelog

All notable changes to Billiards Saloon. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

## [0.8.0] - 2026-10-06

Computer opponents (M8).

### Added
- AI opponents that play on the same simulator and referee as you. For each
  shot they consider pots (every legal ball into every pocket, at several
  powers and spins), safeties and, when snookered, kicks; try each on the
  simulator; re-run the best with their own execution error; and pick the
  best average, valuing the position left for the next shot. They break,
  place the cue ball with ball in hand, call shots and answer the referee's
  choices.
- Five fictional players from club to champion (`assets/data/ai/players.json`),
  each with an aim, power and spin error, a safety bias, a style and a
  thinking time: Sam Whitlock, Dani Reyes, Iris Lindqvist, Marta Okafor,
  Viktor Hale.
- Match setup: Player 2 is a second human or one of them. The AI thinks on a
  worker thread (a THINKING callout shows), swings its cue onto the line,
  draws back and strikes; its aim guides are never shown.
- `--opponent ID` for captures.

## [0.7.0] - 2026-10-06

Audio (M7).

### Added
- Sound, synthesised in code (no recorded files): ball-ball clicks, cushion
  thumps, pocket drops with a rattle, the cue tip's strike, a hall's
  murmuring crowd, applause and interface ticks.
- Shot sounds come from the simulator: each collision, cushion and pocket
  plays at its moment, as loud as the impact was hard, panned by where it
  happened relative to the camera. Replays sound too.
- Applause when a frame or match is won; the crowd's murmur depends on the
  hall (full in the arena, a quiet room in the saloon).
- Settings: master, table sounds and crowd volumes. `--mute` (captures are
  always silent).
- `bs_sound_preview`: writes every sound and a mixed break to WAV files.

### Not yet
- Spoken referee calls; the referee's banners carry the calls for now.

## [0.6.0] - 2026-10-06

Hall visuals, realism and equipment customisation (M6).

### Added
- New rendering pipeline (M6): the scene is lit physically (GGX specular,
  polished resin balls, a velvet sheen on the cloth) into an HDR buffer with
  multisampling, then bloom, an ACES filmic tone curve and a light vignette.
- Lamp shadows: each of the three table lamps casts soft shadows (balls,
  cue ball contact shadows, rails), with PCF filtering by quality.
- Ball numbers printed in the white spot, from the game's own typeface.
- Quality presets now choose MSAA (1/4/8), bloom and shadow detail.
- A modelled tournament table (original design): cushions built from the
  simulator's own nose lines, jaws and rounded jaw tips, so what you see is
  what the balls hit; dark wood rails with 18 pearl sights, leather pocket
  rims over open drop pockets, a metal trim line, a slim apron and square
  legs. The table now stands at true height (cloth 30.5 in above the floor).
- The Locker (main menu): cloth, rails, trim, pocket leather, ball set,
  cue and hall lighting, changed live on the table behind the menu and
  saved in settings. The catalogue is data
  (`assets/data/equipment/catalog.json`): five cloths, four rail finishes,
  three trims, three leathers, two ball sets (the broadcast set has a
  measle cue ball), three cues and three lighting moods (Tournament Arena,
  Classic Saloon, Night Final). Original designs, no brands.
- `--screen locker` for captures.
- Two halls, chosen by the Locker's lighting: the Tournament Arena (dark
  carpet and a blue playing area, barrier boards with a gold line, tiered
  stands with seats, a scorer's desk and players' chairs, and a canopy
  light 1 m above the bed with glowing panels) and the Classic Saloon
  (wood floor, panelled walls with a dado rail, hanging globe lamps). The
  lamps move with the hall; each mood sets its own fill light.

### Fixed
- Spheres were wound clockwise, so culling showed the inside of the far
  half of every ball and lamp; balls now light and shade correctly.

## [0.5.0] - 2026-10-06

Shot input and broadcast presentation (M5).

### Added
- Aim guides from the simulator (M5): the shot is simulated as you aim, and
  the guide shows the ghost ball at first contact, the object ball's line and
  the cue ball's line after contact. Settings → Aim guide: off, ghost ball
  (short lines, the default) or full path (both balls until they stop).
- A modelled cue stick (tip, ferrule, tapered maple shaft, joint, forearm,
  wrap, butt) that sits at the chosen tip offset, pulls back with power and
  follows through on the strike.
- Broadcast director (M5): the default Broadcast camera cuts like TV while
  the balls roll. It knows the shot's future, so it cuts to a camera behind
  the pocket about a second before the first object ball drops, and goes
  wide for long shots.
- Replays: R (left stick click) replays the last shot, and a frame-winning
  pot is replayed in slow motion before the result card; the table is put
  back exactly afterwards. A REPLAY badge shows; Space or A skips.
- `--camera broadcast|aim|overview|follow|free` and `--scenario replay`
  for captures.

- Cue elevation (M5), 0–60°: W/S or the wheel (RT/LT on a gamepad). An
  elevated off-centre hit tilts the spin axis and the cue ball curves (swerve,
  massé); the guide shows the curve and the HUD reads e.g. "CUE UP 35° ·
  MASSÉ". Two elevated golden shots match pooltool.
- Shot clock (M5): off, 30, 45 or 60 seconds, chosen in match setup and
  remembered. It runs from the shot after the break while the shooter
  places, aims or strokes, and pauses during pause, replays and referee
  questions. One 30-second extension per player per frame (T, right stick
  click). Running out is a foul: ball in hand, and it counts towards three
  fouls in 9- and 10-ball. Shown in the scorebug, red for the last 10 s.

### Changed
- The development cue-ball respot moved from R to F9 (debug builds).

### Removed
- The flat aim bar and tip marker, replaced by the cue and the guides.

## [0.4.0] - 2026-10-05

Event-based physics (M3) and WPA rules (M4).

### Added
- Event-based physics (M3): every shot is simulated exactly
  when the cue is released and played back in real time. Sliding, rolling
  and spinning follow closed-form equations; ball-ball throw and spin
  transfer, Han 2005 cushions contacting at the nose height, real pocket
  mouths with rounded jaws, and squirt from side spin. No tunnelling at any
  speed; full power now reaches break speed (about 10.8 m/s ball speed).
- `--physics legacy` runs the prototype solver for comparison.
- WPA referee (M4) for 8-ball, 9-ball and 10-ball: legal-break checks,
  no-cushion-after-contact foul, push-outs, called shots, spotted balls,
  three fouls in a row, and the referee's choices (accept the table or
  re-rack, play on or hand the shot back). Clause-by-clause tests.
- 9-ball rules are real now (they used to reuse 8-ball's) and 10-ball is new.
- Ball in hand: move the cue ball (mouse, keys or stick) with an overlap
  check, behind the head string where required; the camera goes overhead.
- Called pocket and ball follow the aim and can be changed by hand; the
  pocket glows and a chip floats over the called ball.
- Match setup screen: discipline, race length (single frame to race to 13),
  alternate or winner breaks; remembered in settings.
- Race to N: frame scores in the scorebug, frame and match result cards,
  next frame and rematch.
- WPA racking: random rack with the required positions per game.
- Scorebug for rotation games: the balls left with the one to hit marked,
  and the foul count with a warning on two.
- `--screen setup` and `--scenario foul|choice|call` for captures.
- Golden shots: ten reference shots from pooltool (generator in
  `tools/golden/`) replayed against the simulator; final positions agree
  within 0.5 mm. The comparison found a pooltool 0.6.0 defect (phantom
  collisions between separating balls) that the simulator does not share.

### Changed
- The table is a WPA 9 ft table (2.54 × 1.27 m) with pocket geometry and
  simulator coefficients in `table_9ft.json`.
- Racking and breaking run along the length of the table: the rack sits on
  the foot spot and the cue ball on the head spot (the prototype broke across
  the short side).
- Pockets are marked on the rails.
- Releasing a held button after closing a dialog or placing the cue ball no
  longer starts a shot.

### Removed
- The prototype 8-ball rules (`turn_rules`, `MatchState`).
- The prototype fixed-step solver, `--physics legacy` and the legacy table
  coefficients: the event simulator matched the golden shots and passed
  play-testing.

Deferred from M2: remappable controls (with the M11 settings work) and UI
sound cues (with M7 audio).

## [0.3.0] - 2026-10-05

Milestone M2: UX foundation and broadcast frontend.

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

[Unreleased]: https://github.com/YoussefSah05/BillardsSaloon-GL/compare/v0.7.0...HEAD
[0.7.0]: https://github.com/YoussefSah05/BillardsSaloon-GL/compare/v0.6.0...v0.7.0
[0.6.0]: https://github.com/YoussefSah05/BillardsSaloon-GL/compare/v0.5.0...v0.6.0
[0.5.0]: https://github.com/YoussefSah05/BillardsSaloon-GL/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/YoussefSah05/BillardsSaloon-GL/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/YoussefSah05/BillardsSaloon-GL/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/YoussefSah05/BillardsSaloon-GL/compare/v0.1.0-prototype...v0.2.0
[0.1.0-prototype]: https://github.com/YoussefSah05/BillardsSaloon-GL/releases/tag/v0.1.0-prototype
