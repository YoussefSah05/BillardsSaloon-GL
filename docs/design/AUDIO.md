# Septhuit — Audio

Last revised 2026-10-09. Implementation: `src/audio/recorded_bank.*`
(asset decoding), `src/audio/shot_sounds.*` (event planning), and
`src/app/audio_engine.*` (miniaudio playback).

The game plays downloaded sound assets. It contains no procedural sound
synthesis and has no generated-sound fallback. The 48 bundled WAV files
occupy approximately 3.76 MiB, decode once at startup, and require no network
access during play. All sources, licenses and processing details are in
[the bundled audio credits](../../assets/audio/CREDITS.md),
`assets/audio/recipe.json` and `assets/audio/bank.json`.

## Event mapping

| Game event | Asset bank | Recording or downloaded source |
|---|---|---|
| Ball collision | ball | Pool ball contacts by bsumusictech, Za-Games and csaszi |
| Cue strike | cue | Cue-tip transients excerpted from csaszi's pool shots |
| Cushion/rail | cushion | Damped soft material impacts from Kenney |
| Pocket jaw | jaw | Light wood impact foley from Kenney |
| Pocket drop | pocket | Three potting excerpts recorded by Caitlin_100 |
| Ball placement | place | Soft pool-ball contacts, darkened with EQ |
| Racking | rack | Several contacting balls from Caitlin_100's pool recording |
| Cue movement | slide | Recorded fabric friction by Iochi Glaucus |
| Shot clock | clock | Recorded clock tick by Nicole Marie T |
| Foul notice | foul | Downloaded Kenney interface error sound |
| Menu navigation/confirmation | ui_move/ui_confirm | Kenney interface sounds |
| Frame/match win | applause_short/applause_long | Recorded applause by qubodup/eXpl0it3r |
| Hall ambience | murmur | Gregor Quendel's recorded crowd ambience |
| Moving balls | roll | Quiet between-impact texture from csaszi's pool recording |

Ball, cue and cushion banks each contain soft, medium and hard layers, with
three variants per layer. Ball layers use different recorded contacts;
cue layers use EQ versions of three recorded strikes; cushion layers use
recorded soft-impact takes and EQ. Playback pitch varies only ±1.5%.

Cushions, jaws, placement and cue movement use material foley substitutes.
They are not all isolated recordings of those exact actions on a pool table.
The core ball, cue and pocket events come from actual cue-sport recordings.
Freesound assets use its public high-quality MP3 previews rather than the
original lossless files. Source URLs and these limitations are documented
so a later sound-design pass can replace individual samples independently.

## Asset preparation

`tools/audio/import_recordings.py` uses ffmpeg to decode downloaded files and
archive members. It excerpts the selected actions, removes leading silence
and DC offsets, applies the recipe's EQ, adds short boundary fades, and
sets peak headroom. It crossfades loop boundaries. It generates no oscillators,
noise or replacement impacts. Prepared assets are mono, 48 kHz, 16-bit PCM WAV;
the game decodes them to float PCM at startup.

To reproduce the bank, obtain the files listed in `recipe.json` in a source
directory, then run:

```bash
python3 tools/audio/import_recordings.py /path/to/downloaded-recordings
```

The importer records original-file and prepared-file SHA-256 hashes. Keep
`assets/audio/CREDITS.md` and `assets/audio/licenses/` with distributions.
The recordings use CC0 or CC BY 4.0; crowd and clock creators are explicitly
credited. The application source license does not replace asset licenses.

A crossfaded rolling loop follows aggregate ball speed and the
velocity-weighted position of moving balls. Its gain and pitch fall as the
balls slow down. Pocketed balls are excluded. The crowd bed and applause
use a separate bus; the crowd is quieter in menus and the saloon.

## Simulator events and loudness

Sound planning takes the trajectory and its actual table geometry. For each
contact, the relevant balls are evolved from the preceding event's state to
the contact time. This also preserves impulses from earlier contacts at the
same timestamp, which sampling at `time - epsilon` can miss.

| Event | Speed used | Full-volume reference |
|---|---|---|
| Cue strike | Cue ball speed after the strike | 9 m/s |
| Ball collision | Positive closing speed along the line of centres | 6 m/s |
| Rail or jaw | Magnitude of velocity along the contact normal | 4 m/s |
| Pocket | Ball speed, with a minimum audible intensity | 3 m/s |

Intensity is `sqrt(clamp(speed / reference, 0, 1))`. This keeps gentle contacts
audible. Cushion intensity depends on normal speed: a fast grazing contact
should not sound like a square hit. Pocket intensity has a floor of 0.35;
other events below 0.03 intensity are omitted.

Only duplicate contacts involving the same participants, within 6 ms and
2 cm, are merged. Different pairs in a break retain separate sounds. The
previous broad merging suppressed much of a rack's cascade of impacts.

## Position and timing

The camera supplies the listener position and right vector. Impact gain is
`0.65 * intensity / (1 + 0.18 * distance)`, with a further 0.85 multiplier for
rails. Stereo pan projects the direction to the impact onto the camera's
right vector and stays within ±0.8.

The application schedules 50 ms of upcoming contacts against the engine's
PCM frame clock. Separate contacts can therefore start at different sample
times even when they fall between rendered frames. This reduces timing
quantization; it does not eliminate device latency or guarantee synchronization
through frame stalls longer than the look-ahead window. Slow replays scale
both event delays and sound pitch by playback speed.

Pausing cancels active and scheduled table effects. Resuming schedules the
remaining events against the audio clock, which continued while gameplay was
frozen. Restarting and skipping replays also cancel pending table effects.

## Mixer and runtime cost

The miniaudio engine has effects, crowd and interface buses under a master
volume. Table settings control gameplay effects, including rolling, clock
ticks and fouls. Crowd settings control murmur and applause.

Each decoded recording, data source and playback node is prepared at startup.
The bank uses 122 one-shot voices plus two loops. A contact resets an existing
voice instead of decoding a file or allocating a sound node. Ball variants
have four voices; cushion and jaw variants have three; other one-shots have
two. If a bank fills, its quietest voice is reused, with age as a tie-breaker.
Interface sounds cannot steal table or crowd voices.

Seeking, scheduled starts, fades and buses use the engine APIs documented
in the [miniaudio manual](https://miniaud.io/docs/manual/index.html).
A stereo-linked limiter caps output at 0.92 with instantaneous attack and an
approximately 80 ms release. Its callback allocates no memory and takes no
locks. Muted or unavailable engines skip application sound planning. Missing
or corrupt audio assets produce an actionable startup message and disable
audio instead of falling back to synthesised effects.

## Validation and previews

```bash
cmake --build build -j8
ctest --test-dir build --output-on-failure
./build/bs_sound_preview /tmp/billiards-recorded-preview
```

The headless tests check event intensity and preservation of separate rack
contacts. `bs_audio_tests` checks bank coverage, PCM decoding, finite/bounded
samples, loop seams and fades. It renders the actual engine without a device
to check delayed starts, voice reuse, limiting under dense overlaps, stereo
positioning, volume controls, rolling and cancellation of pending sounds.

The preview tool exports each bundled sample and `break_mix.wav`, using the
same mixer as the game. Numeric tests verify implementation and output
bounds; listening on the player's audio device remains part of sound-design
assessment.

## Remaining limits

Positioning uses stereo panning, without head-related filtering. Rolling is
one aggregate source, not one per ball. Samples retain their recorded room
character rather than matching the selected hall's acoustics. Cue-tip material,
strike offset and miscues do not have separate sounds. There are no spoken
referee calls.
