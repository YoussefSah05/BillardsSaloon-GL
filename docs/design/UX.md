# Billiards Saloon — UX and Frontend Design

Status: v1.0 target. Last revised 2026-10-05. Companion to `GDD.md`.

## Principle: "You are watching — and playing — a televised final"

Every screen should feel like part of a sports broadcast covering a
tournament in a grand old hall. The UI speaks in broadcast graphics:
scorebugs, lower thirds, replay stingers, referee banners. The table is
always the hero: UI is compact, sits on the edges, and gets out of the way
while the player aims.

Three tests for every UI decision:

1. **Broadcast-true** — would this look at home on a TV pool final?
2. **Table-first** — does it hide the table only when it must?
3. **Readable at a glance** — can a player read it in under a second, from the couch?

## Design language

| Element | Direction |
|---------|-----------|
| Typography | Condensed sans for headings and numbers (e.g. Barlow Condensed, OFL), clean sans for body (e.g. Inter, OFL). Tabular figures for scores and clocks. Uppercase with tracking for labels. |
| Colour | Near-black translucent panels (backdrop blur on High quality), off-white text, one accent: championship gold. Status colours reserved for meaning: red = foul, green = legal/ready, blue = information. Ball colours stay true to the real set. |
| Shape | Slanted (parallelogram) edges on broadcast bars, thin gold rules, 2–4 px corner radius elsewhere. |
| Motion | Broadcast wipes and slides, 150–300 ms, ease-out. Banners slide in, hold, slide out. Every animation respects a Reduced Motion setting. |
| Sound | Every UI action has a soft cue (tick, confirm, back); referee calls are voiced and captioned. |
| Iconography | Simple line icons; input prompts show mouse/keyboard or gamepad glyphs depending on the last device used. |

Fonts and any icon set must ship with open licences (SIL OFL / CC0), credited in `CREDITS.md`.

## Frontend flow

```
Launch ─► Studio splash (≤2 s, skippable)
       ─► Title: live 3D hall, slow crane shot over the table, "PRESS ANY KEY"
       ─► Main hub
            ├─ Play ─► Quick Match ─► Match setup ─► Player intro ─► Match
            │        ├─ The Tour ───► Season hub (bracket, rankings, next opponent)
            │        ├─ Practice ───► Table setup ─► Free table
            │        └─ Trick Shots ► Challenge list (stars) ─► Challenge
            ├─ Locker (equipment: table, cloth, balls, cue, hall)
            ├─ Profile & Stats
            ├─ Settings (Gameplay · Controls · Video · Audio · Accessibility)
            └─ Quit
```

- The 3D hall stays live behind every menu; each menu has its own camera
  pose and the camera glides between them, so the frontend feels like one
  continuous broadcast set rather than separate screens.
- Every screen works with mouse, keyboard and gamepad; Back is always
  Esc / right-click / B.
- **Match setup:** discipline, race length, opponent card (AI pro nameplate,
  skill tier, play style), table cloth colour, aim-assist level.
- **Locker:** the camera moves close to the table and every equipment choice
  previews live on it (cloth colour, rails, pockets, ball set, cue); locked
  items show how to earn them.
- **AI Coach panel** (Practice, and after a frame on request): the shot the AI
  would have played, a heatmap of where the cue ball should have finished, and
  shot difficulty as a percentage, drawn on the table in 3D.
- **Player intro:** lower-third nameplates for both players with their record,
  then a crane shot down to the table; skippable.

## In-match HUD

Default state while aiming: only the scorebug and the shot controls.

| Element | Placement | Content |
|---------|-----------|---------|
| Scorebug | Top-left, broadcast bar | Player names, frames won, race-to, active-player marker, group (solids/stripes) or lowest ball, foul count (9-ball three-foul rule). |
| Ball tray | Under the scorebug | Remaining balls per player, as small ball icons; potted balls drop out with a short animation. |
| Shot clock | Next to the active player | Ring that drains; amber at 10 s, red at 5 s; optional per match setup. |
| Power meter | Right edge, vertical | Fills as the stroke is drawn back; ghost marker shows the last shot's power. |
| Spin widget | Bottom-right | Cue-ball diagram with the tip contact dot; elevation arc for jump/massé. |
| Aim aids | In the world | Ghost ball, first-contact line, object-ball direction; full path predictor in Practice only. Level set in match setup. |
| Input prompts | Bottom centre, fades after a few shots | Context-sensitive ("Drag back to shoot · RMB spin · Shift fine aim"). |

States and how the UI responds:

- **Aiming:** HUD as above; the camera sits low behind the cue.
- **Shot in motion:** aim widgets fade out; the director camera takes over.
- **Shot resolved:** a referee banner slides in when something happens:
  `FOUL — CUE BALL SCRATCHED`, `BALL IN HAND`, `PUSH OUT`, `GROUPS: SOLIDS`.
  Every foul names its reason.
- **Turn change:** a lower-third slides in with the incoming player's name.
- **Ball in hand:** the cue ball follows the pointer on the cloth; legal
  areas are tinted green, illegal ones red (kitchen restriction, overlap).
- **Call shot (10-ball, optional 8-ball):** click a ball, then a pocket; the
  called pocket is outlined on the table.
- **Frame won:** slow-motion replay of the winning shot with a "REPLAY"
  stinger, then the frame summary card (pot %, safeties, break-and-runs,
  longest run, average shot time) and Next frame / Rematch / Menu.

## Pause and in-match menu

The table dims and blurs; the scorebug stays visible. Entries: Resume,
Restart rack, Settings, Rules reference (for the current discipline),
Concede, Main menu. Destructive actions (Concede, Main menu during a match)
ask for confirmation.

## Feedback and juice

- Cue strike: brief camera kick, subtle screen-space shake scaled with power
  (disabled by Reduced Motion).
- Pots: pocket rim glow and a crowd reaction scaled with shot difficulty.
- Great shots (long pots, banks, kicks, jump shots) get a small broadcast
  caption: `BANK SHOT`, `LONG POT`.
- Haptics on gamepad for strike and pot.

## Onboarding

- First launch: a 60-second guided first shot (aim, power, spin) in Practice,
  with coach marks that point at the real HUD elements.
- Rules reference cards per discipline, reachable from the pause menu.
- Tooltips on every setting, with a live preview where it applies.

## Accessibility

- Remappable controls; mouse sensitivity and invert options; hold-to-toggle alternatives.
- Text size (100–150%) and UI scale; high-contrast HUD option.
- Colour-blind support: numbers always visible on balls, optional pattern
  overlays, never colour-only status (fouls also use text and icons).
- Captions for referee and commentary audio; Reduced Motion setting.
- Full keyboard-only and gamepad-only navigation.

## Technical approach

- **RmlUi** (HTML/CSS-like, retained mode) for all player-facing UI: menus,
  HUD, banners. Styles live in `assets/ui/*.rcss`, documents in `*.rml`, so
  layout and animation iterate without recompiling. Rendered with its
  OpenGL 3 backend on our 4.1 core context, after the 3D scene.
- Text via FreeType (bundled through RmlUi); fonts in `assets/fonts/`.
- **Dear ImGui** only for developer tools (physics inspector, shot debugger),
  compiled out of Release builds.
- A thin `ui/` layer binds game state to documents (RmlUi data bindings) so
  screens read from view models, like today's `GameplayHudModel`, and never
  reach into the simulation directly.
- The current voxel-text overlay (`render/ui_overlay`, `app/overlay_screens`)
  is replaced, not extended.

## Verification

- Every screen reachable and operable with mouse only, keyboard only and gamepad only.
- HUD legible at 1280×720 and 4K; UI scale 100–150% without clipping.
- A first-time player completes the guided first shot without outside help.
- Reduced Motion removes shake and long transitions; captions cover every voiced line.
