# Billiards Saloon — Audio

Status: milestone M7 (0.7.0). Last revised 2026-10-06. Code:
`src/audio/sound_synth.*` (synthesis), `src/audio/shot_sounds.*` (shot sound
planning), `src/app/audio_engine.*` (playback through miniaudio).

The game ships no recorded samples. Every sound is synthesised at startup from
simple physical models, then played back in time with events from the
simulator. This document explains the models, why they are good enough for
the ear, and how the simulator's events become a mix.

## 1. Why synthesis

- **Licensing and size.** Good recordings of pool sounds are rare and rarely
  free to redistribute. Synthesis needs a few hundred lines of code and no
  asset files.
- **Variation.** A recorded click repeated a hundred times in a break sounds
  mechanical (the "machine-gun effect"). Synthesis makes several variants of
  each sound with randomised parameters.
- **Control.** Every sound is tied to a physical parameter (impact speed) that
  the simulator knows exactly.

## 2. Modal synthesis

A struck solid object vibrates in a set of **modes**: patterns of motion, each
with its own frequency. After the impact each mode rings as a damped
oscillator, so the sound is a sum of exponentially decaying sinusoids (Adrien
1991; van den Doel and Pai 1998):

$$
y(t) = \sum_{k} a_k\, e^{-t/\tau_k} \sin(2\pi f_k t), \qquad t \ge 0.
$$

Each mode $k$ has a frequency $f_k$, an amplitude $a_k$ and a decay time
$\tau_k$. Together they carry the object's identity:

- **Frequencies** come from size, shape and stiffness. A phenolic resin ball
  is small and very stiff, so its modes are high (here 1.6–7.4 kHz). A
  cushion is a soft rubber strip backed by a wooden rail, so its modes are low
  (145 Hz, 310 Hz) with one wooden mode at 820 Hz.
- **Decay times** come from internal damping. Hard resin loses little energy
  per cycle but radiates fast, giving the dry, few-millisecond "click"
  ($\tau \approx 2$–$6\,\mathrm{ms}$). Rubber absorbs energy:
  $\tau \approx 8$–$30\,\mathrm{ms}$ and a dull thud.
- **Amplitudes** come from where the object is struck and how each mode
  radiates. They are set by ear, with higher modes quieter.

The modes of a real sphere are not harmonic: their frequencies are not
integer multiples of one fundamental. The ratios used here
($3150 : 4870 : 7420 \approx 1 : 1.55 : 2.36$) are inharmonic on purpose. That
is what makes a ball sound like a struck solid, not a musical note.

**Contact transient.** The first milliseconds of an impact are broadband: the
contact force is a short pulse, and a short pulse has a wide spectrum. This is
modelled as a burst of filtered noise with a linear attack $t_a$ and
exponential decay $\tau_n$:

$$
n(t) = A \min\!\left(1, \frac{t}{t_a}\right) e^{-t/\tau_n}\, \tilde{w}(t),
$$

where $\tilde w$ is white noise $w[i] \sim \mathcal U(-1, 1)$ passed through a
one-pole low-pass filter,

$$
\tilde w[i] = \tilde w[i-1] + k\,(w[i] - \tilde w[i-1]), \qquad 0 < k \le 1.
$$

The coefficient $k$ sets the brightness. Its $-3\,\mathrm{dB}$ cutoff at sample
rate $f_s = 48\,\mathrm{kHz}$ is approximately

$$
f_c \approx \frac{f_s}{2\pi} \ln\!\frac{1}{1-k}.
$$

A soft cushion contact lasts longer, so its pulse is darker ($k = 0.12$, about
1 kHz). A ball-to-ball contact lasts about $0.2\,\mathrm{ms}$, so its pulse
is bright ($k = 0.9$).

## 3. The sound bank

| Sound | Modes ($f_k$ in Hz, $\tau_k$ in ms) | Transient | Length |
|---|---|---|---|
| Ball–ball click | 3150/4.5, 4870/3.2, 7420/2.0, 1650/6.0 | bright, 1.2 ms | 60 ms |
| Cushion thud | 145/30, 310/18, 820/8 (wood) | dark, 10 ms | 160 ms |
| Cue strike | 1250/6, 2480/4, 520/12 (shaft) | medium, 2.5 ms | 80 ms |
| Pocket drop | 190/40 knock, then five 260 Hz rattles | dark | 750 ms |

**Variants.** Six variants of each impact sound are generated from different
seeds. Each variant's mode frequencies are jittered by about $\pm 5$–$15\%$,
and one variant is picked at random on every play. Real balls differ slightly
in mass, temperature and where they are struck. This jitter imitates that and
removes the machine-gun effect.

**Pocket drop.** This is a short sequence, not a single impact: a knock on the
leather, a rattle of five decaying impacts at irregular intervals (amplitudes
$\propto e^{-0.45 i}$, the energy lost at each bounce), then a long, very dark
noise tail for the ball rolling down the drop.

**Normalisation.** Each buffer is scaled to a fixed peak, so loudness is set in
one place (the mixer) rather than baked into the synthesis.

## 4. The crowd

**Murmur** is the sound of many voices too far away to understand. Each of
$V = 24$ voices is dark noise ($f_c$ of a few hundred hertz) multiplied by a
syllable-rate envelope,

$$
e_v(t) = \left(\tfrac12 + \tfrac12 \sin(2\pi r_v t + \phi_v)\right)^2,
\qquad r_v \in [2, 5]\ \mathrm{Hz},
$$

which is the rate at which speech opens and closes. Summing many independent
voices gives a texture with natural fluctuations and no recognisable pattern.
Two further low-pass poles (about 1.1 kHz) remove hiss, and a gentle high-pass
removes rumble.

The murmur loops for the whole match, and the seam must be inaudible. Each
voice's rate is rounded so that a whole number of syllables fits in the loop,
$r_v' = \operatorname{round}(r_v T)/T$, so every envelope is periodic in $T$. A
$0.25\,\mathrm{s}$ linear crossfade between the loop's end and its start
removes the remaining mismatch in the filtered noise.

**Applause** is a crowd of $P = 60$ people clapping, each a sequence of short,
bright noise bursts at their own rate (3.5–6 claps per second) with a small
random jitter per clap. Their clapping starts within the first $0.15\,\mathrm{s}$
and stops at random times between 55% and 95% of the sound, so the applause
swells in and thins out instead of stopping at once. Uncorrelated clappers
blur into the familiar roar, and the thinning tail gives the ending.

## 5. From simulator events to sounds

The event-based simulator (see [PHYSICS.md](PHYSICS.md)) knows every impact of
a shot, with its exact time and the balls' states just before it. Sound is
planned from the trajectory, not detected from the animation, so it can never
miss a contact or be late by a frame.

**Intensity.** Each event's loudness comes from the speed of the impact:

| Event | Speed used | Full-scale speed $v_\text{full}$ |
|---|---|---|
| Cue strike | cue ball's speed just after the strike | $9\,\mathrm{m/s}$ |
| Ball–ball | closing speed along the line of centres, $\lvert(\mathbf v_i - \mathbf v_j)\cdot\hat{\mathbf n}\rvert$ | $6\,\mathrm{m/s}$ |
| Cushion | ball speed | $4\,\mathrm{m/s}$ |
| Pocket | ball speed (with a floor of 0.35) | $3\,\mathrm{m/s}$ |

The closing speed along the line of centres is the right measure for a
ball–ball click: it sets the normal impulse, and only the normal impulse
excites the balls' vibration. A thin cut at high speed is quiet. The speed is
mapped to an intensity with a square root:

$$
I = \sqrt{\operatorname{clamp}\!\left(\frac{v}{v_\text{full}}, 0, 1\right)}.
$$

The energy radiated by an impact grows with the kinetic energy delivered, about
$v^2$, so the amplitude grows about linearly with $v$. The ear, however,
perceives loudness roughly as a power of intensity (Stevens's law), and soft
taps in a quiet hall are clearly audible. The square root compresses the
range so a gentle kiss is still heard under the murmur while a break does not
clip.

**Merging.** Events of the same kind less than $6\,\mathrm{ms}$ apart are
merged into one sound at the louder intensity. In a break many contacts happen
within a few milliseconds, and the ear hears them as one sound (temporal
integration). Playing each separately would only sum to clipping. Events
quieter than $I = 0.03$ are dropped.

## 6. Placing sounds in the hall

Each cue carries the position of its impact. At play time it is placed relative
to the camera (the listener):

**Distance.** Gain falls with distance $d$ in metres:

$$
g = \frac{I}{1 + 0.35\,d}.
$$

Free-field sound falls as $1/d$. A hall is reverberant, so far sounds lose less
than that, and the table is small. The soft law keeps the far end of the table
audible from the near end.

**Panning.** The pan position comes from the direction to the sound, projected
on the camera's right vector $\hat{\mathbf r}$:

$$
p = 0.8\, \frac{\mathbf x - \mathbf x_L}{\lVert \mathbf x - \mathbf x_L \rVert} \cdot \hat{\mathbf r} \in [-0.8, 0.8].
$$

The factor 0.8 keeps sounds from collapsing fully into one speaker. miniaudio
applies it as a balance control.

**Pitch.** Harder impacts sound slightly higher and brighter (the contact
pulse is shorter), and no two impacts sound identical:

$$
\text{pitch} = (0.95 + 0.1\,I)\cdot u, \qquad u \sim \mathcal U(0.97, 1.03).
$$

## 7. The mixer

Playback uses miniaudio's engine with three groups (buses): **effects**,
**crowd** and **interface**. Each group has its own volume, set from the
player's settings, under a master volume. A pool of 48 voices plays sounds.
When all are busy, a random active voice is reused. Sounds are generated once
at startup, at $48\,\mathrm{kHz}$ mono, 32-bit float.

## 8. Limitations

- Modal parameters are set by ear, not measured. Measuring the modes of real
  balls with a microphone and fitting $(f_k, a_k, \tau_k)$ is a natural next
  step.
- Reverberation is not modelled. The hall's character comes only from the
  crowd and the soft distance law. A small algorithmic reverb would add depth.
- Panning is stereo balance only. There is no head-related filtering, so
  sounds cannot come from behind.
- The cue strike does not depend on where the tip hits or on the tip's
  hardness. A miscue sound is missing.

## References

- Adrien, J.-M. *The Missing Link: Modal Synthesis.* In *Representations of Musical Signals*, MIT Press, 1991.
- van den Doel, K., Pai, D. K. *The Sounds of Physical Shapes.* Presence 7(4), 1998.
- Cook, P. R. *Real Sound Synthesis for Interactive Applications.* A K Peters, 2002.
- Stevens, S. S. *On the Psychophysical Law.* Psychological Review 64(3), 1957.
- Farnell, A. *Designing Sound.* MIT Press, 2010.
