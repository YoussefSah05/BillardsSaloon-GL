# Billiards Saloon — Game Design Document

Status: v1.0 target. Last revised 2026-10-04.

## Identity: "The Tournament Hall"

Billiards Saloon is a realism-first pool game presented like a televised
tournament. Three pillars drive every decision:

1. **Authentic physics** — an event-based simulator (Leckie–Greenspan / pooltool
   lineage): exact sliding/rolling/spinning transitions, realistic cushions
   (Han 2005, Mathavan 2010), pocket jaws, squirt, swerve and massé.
2. **Authentic rules** — WPA World Standardized Rules for 8-ball, 9-ball and
   10-ball, enforced by an on-screen referee.
3. **Broadcast feel** — TV camera director, scorebug, shot clock, replays and
   post-shot analysis, in a frontend styled like a sports broadcast (see `UX.md`).

## Modes (all required for v1.0)

| Mode | Summary |
|------|---------|
| Quick Match | vs AI or local 2-player (hot-seat). Choose discipline, race-to-N, AI difficulty. |
| Practice | Free table, place any ball, undo, slow motion, aim predictor. |
| Trick Shots & Drills | JSON-defined scenarios with goals and 1–3 star ratings; daily seeded challenge. |
| The Tour (career) | Amateur → Regional → Pro circuit. Bracketed events, ranking points, named AI pros, unlocks (venues, cloth colours, cues). |

## Rules scope (WPA)

- **8-ball:** open table after break; groups assigned on legal pot; optional
  call-pocket; 8 on a legal break → breaker chooses spot-8 or re-rack; scratch on
  break → ball in hand behind the head string; early 8 / 8 + scratch loses.
- **9-ball:** lowest ball first; legal break requires a pot or 4 balls to rails;
  push-out on the shot after the break; three consecutive fouls lose the rack.
- **10-ball:** as 9-ball, but call shot.
- **Common:** ball in hand anywhere after standard fouls (placement UI with
  overlap check), no-rail-after-contact foul, race-to-N matches.

## Shot input

- Mouse aim with fine-aim modifier; gamepad support.
- Cue-tip offset (english / follow / draw) and cue elevation (jump/massé).
- Power from a pull-back stroke gesture (mouse or stick), with a meter.
- Aim aids gated by mode/difficulty: ghost ball, first-contact line, full
  predicted path (Practice only).

## Presentation

Frontend flow, HUD, menus, design language and accessibility are specified
in [`UX.md`](UX.md).

- Director camera: aim view, overhead, follow-ball, and cut-to-pocket using the
  known future of the simulated shot.
- Scorebug: players, race score, active group/lowest ball, shot clock, foul count.
- Referee calls (voice + banner): Foul, Ball in hand, Push out, Frame.
- Replay with slow motion; automatic replay of frame-winning shots.
- Post-shot analysis: cue-ball path trace, contact point, spin readout.
- Match stats: pot %, safety %, break-and-runs, average shot time.

## AI

Candidate shots (direct, bank, kick, safety) are generated geometrically and
evaluated by running the headless simulator. Score = pot probability under
execution noise + value of resulting cue-ball position. Named pros have
profiles: accuracy, power preference, safety tendency, break style. 4–5 tiers.

## Audio

Ball-ball clicks, cushion thuds, pocket drops, cue strike, chalk — volume/pitch
scaled from simulated impulses. Hall ambience, crowd reactions, referee voice.

## Visual direction

Tournament arena: a single brightly lit table under a rectangular light canopy,
dark surroundings, spectator stands, scorer's table. PBR materials, shadowed
key lights, HDR with filmic tonemapping, subtle bloom on the canopy.
