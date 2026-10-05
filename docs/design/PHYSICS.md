# Billiards Saloon — Physics

Status: the event-based simulator is the game's physics (since 0.4.0); it
matches pooltool's reference results to within half a millimetre. Last revised
2026-10-06. Companion to [`RULES.md`](RULES.md), [`AI.md`](AI.md) and
[`INTELLIGENCE.md`](INTELLIGENCE.md).

This document explains the model behind the simulator: the assumptions, the
equations and where they come from, the numerical methods, and why each choice
was made. Code lives in `src/sim/` (library `bs_sim`: headless, double
precision, deterministic). It is a port of pooltool's event-based physics
(Kiefl, Apache-2.0), which follows Leckie & Greenspan.

## 1. Notation and frame

The simulator uses pooltool's frame: $z$ is up, the cloth is the plane $z=0$,
the origin is a corner of the playing surface, $x$ runs across the table
(width $W = 1.27\,\mathrm{m}$) and $y$ along it (length $L = 2.54\,\mathrm{m}$).
The game's own frame ($y$ up, $x$ along the table, centred) is converted in
`src/gameplay/sim_bridge.h`:

$$
(x_\text{game},\ y_\text{game},\ z_\text{game}) = \left(y_\text{sim} - \tfrac{L}{2},\ z_\text{sim},\ x_\text{sim} - \tfrac{W}{2}\right),
$$

a cyclic permutation of the axes, so orientation (handedness) is preserved.

| Symbol | Meaning | Value |
|---|---|---|
| $m$ | ball mass | $0.170097\,\mathrm{kg}$ |
| $R$ | ball radius | $0.028575\,\mathrm{m}$ |
| $I$ | moment of inertia of a solid sphere | $I = \tfrac{2}{5} m R^2$ |
| $\mathbf{r}, \mathbf{v}, \boldsymbol{\omega}$ | centre position, velocity, angular velocity | |
| $g$ | gravity | $9.81\,\mathrm{m\,s^{-2}}$ |
| $\mu_s$ | ball–cloth sliding friction | $0.2$ |
| $\mu_r$ | rolling resistance | $0.01$ |
| $\mu_{sp}$ | spinning (pivot) friction, written as $\kappa R$ | $\kappa = 10\cdot\tfrac{2}{5}\cdot\tfrac{1}{9} \approx 0.444$ |
| $e_b,\ e_c$ | restitution: ball–ball, ball–cushion | $0.95,\ 0.85$ |
| $\mu_c$ | ball–cushion friction | $0.2$ |
| $h$ | height of the cushion's nose above the cloth | $0.64 \cdot 2R \approx 0.0366\,\mathrm{m}$ |

Unit vectors carry a hat ($\hat{\mathbf{z}}$); $\lVert\cdot\rVert$ is the
Euclidean norm.

## 2. Why an event-based simulator

The prototype (up to 0.3.0) integrated the equations of motion with a fixed
time step $\Delta t = 1/120\,\mathrm{s}$. Fixed stepping has two problems that
matter for pool:

1. **Tunnelling.** A ball moves $\lVert\mathbf{v}\rVert\,\Delta t$ per step. At
   a break speed of $10\,\mathrm{m\,s^{-1}}$ that is $8.3\,\mathrm{cm}$, three
   times the radius: a ball can pass through another ball or a cushion between
   two steps. Any step-based scheme needs
   $\lVert\mathbf{v}\rVert\,\Delta t \ll R$, i.e. tiny steps.
2. **Step dependence.** Results change with $\Delta t$, so a replay, an aim
   preview and an AI's search of the same shot can disagree.

Between collisions, however, a ball on cloth obeys simple equations with
closed-form solutions. The whole shot is therefore a **hybrid dynamical
system**: smooth, analytic flows interrupted by discrete events (collisions and
changes of motion state). The event-based method (Leckie & Greenspan 2005)
computes, for the current state, the exact time of every possible next event,
advances all balls analytically to the earliest one, resolves it, and repeats.
There is no time step, so nothing can tunnel, the result is independent of
frame rate, and the full trajectory is known the moment the cue strikes. That
last property is what makes the aim guides, the broadcast director (which cuts
to a pocket before the ball drops), exact replays and the AI's thousands of
trial shots possible.

## 3. The ball on the cloth

### 3.1 Contact and slip

The ball touches the cloth at $\mathbf{r} - R\hat{\mathbf{z}}$. The velocity of
the ball's material at that point, the **slip velocity**, is

$$
\mathbf{u} = \mathbf{v} + \boldsymbol{\omega} \times (-R\,\hat{\mathbf{z}})
= \mathbf{v} + R\,\hat{\mathbf{z}} \times \boldsymbol{\omega}.
$$

The ball **rolls** when $\mathbf{u} = \mathbf{0}$, i.e. when the horizontal spin
satisfies $\boldsymbol{\omega}_{xy} = \tfrac{1}{R}\,\hat{\mathbf{z}} \times \mathbf{v}$.
Otherwise it **slides**, and Coulomb friction acts at the contact, opposite to
the slip.

### 3.2 Motion states

A ball is always in exactly one state:

| State | Condition | Forces and torques |
|---|---|---|
| Sliding | $\lVert\mathbf{u}\rVert > 0$ | friction $-\mu_s m g\,\hat{\mathbf{u}}$ at the contact |
| Rolling | $\mathbf{u} = 0,\ \mathbf{v} \neq 0$ | rolling resistance $-\mu_r m g\,\hat{\mathbf{v}}$ |
| Spinning | $\mathbf{v} = 0,\ \omega_z \neq 0$ | pivot torque about $\hat{\mathbf{z}}$ |
| Stationary | at rest | none |
| Pocketed | in a pocket | out of play |

Spin about the vertical axis, $\omega_z$, is decoupled from the rolling
constraint and decays under the pivot torque in every state.

### 3.3 Sliding: derivation

With the friction force $\mathbf{F} = -\mu_s m g\,\hat{\mathbf{u}}$ applied at
$-R\hat{\mathbf{z}}$, Newton's laws give

$$
m\,\dot{\mathbf{v}} = -\mu_s m g\,\hat{\mathbf{u}},
\qquad
I\,\dot{\boldsymbol{\omega}} = (-R\hat{\mathbf{z}}) \times \mathbf{F} = \mu_s m g R\,(\hat{\mathbf{z}} \times \hat{\mathbf{u}}).
$$

Differentiating the slip velocity and using
$\hat{\mathbf{z}} \times (\hat{\mathbf{z}} \times \hat{\mathbf{u}}) = -\hat{\mathbf{u}}$
for a horizontal $\hat{\mathbf{u}}$:

$$
\dot{\mathbf{u}} = \dot{\mathbf{v}} + R\,\hat{\mathbf{z}} \times \dot{\boldsymbol{\omega}}
= -\mu_s g\,\hat{\mathbf{u}} - \tfrac{5}{2}\mu_s g\,\hat{\mathbf{u}}
= -\tfrac{7}{2}\mu_s g\,\hat{\mathbf{u}}.
$$

The slip shrinks along its own direction, so $\hat{\mathbf{u}}$ is constant
while the ball slides. That makes the equations integrable in closed form, with
$\hat{\mathbf{u}}_0$ the slip direction at the start of the state:

$$
\begin{aligned}
\mathbf{r}(t) &= \mathbf{r}_0 + \mathbf{v}_0 t - \tfrac{1}{2}\mu_s g\,t^2\,\hat{\mathbf{u}}_0, \\
\mathbf{v}(t) &= \mathbf{v}_0 - \mu_s g\,t\,\hat{\mathbf{u}}_0, \\
\boldsymbol{\omega}_{xy}(t) &= \boldsymbol{\omega}_{xy,0} + \frac{5\mu_s g}{2R}\,t\,(\hat{\mathbf{z}} \times \hat{\mathbf{u}}_0).
\end{aligned}
$$

The slip reaches zero, and the ball starts to roll, after

$$
\tau_\text{slide} = \frac{2\,\lVert\mathbf{u}_0\rVert}{7\,\mu_s g}.
$$

The factor $\tfrac{2}{7}$ is the familiar result that a ball struck without spin
loses $\tfrac{2}{7}$ of its speed before rolling ($v_\text{roll} = \tfrac{5}{7}v_0$):
friction converts part of the translation into rotation.

### 3.4 Rolling

Rolling resistance (deformation of cloth and ball) decelerates the ball along
its velocity with a small coefficient $\mu_r$. The rolling constraint fixes the
horizontal spin:

$$
\mathbf{r}(t) = \mathbf{r}_0 + \mathbf{v}_0 t - \tfrac{1}{2}\mu_r g\,t^2\,\hat{\mathbf{v}}_0,
\qquad
\boldsymbol{\omega}_{xy}(t) = \tfrac{1}{R}\,\hat{\mathbf{z}} \times \mathbf{v}(t),
\qquad
\tau_\text{roll} = \frac{\lVert\mathbf{v}_0\rVert}{\mu_r g}.
$$

### 3.5 Vertical spin

The pivot torque decelerates $\omega_z$ at a constant rate until it stops:

$$
\dot\omega_z = -\operatorname{sgn}(\omega_z)\,\frac{5\,\mu_{sp}\,g}{2R} = -\operatorname{sgn}(\omega_z)\,\tfrac{5}{2}\kappa g,
\qquad
\tau_\text{spin} = \frac{2R\,\lvert\omega_{z,0}\rvert}{5\,\mu_{sp}\,g}.
$$

(pooltool specifies the pivot coefficient as proportional to $R$; with
$\mu_{sp} = \kappa R$ the deceleration is $\tfrac{5}{2}\kappa g \approx 10.9\,\mathrm{rad\,s^{-2}}$.)

### 3.6 Trajectories as polynomials

In every state the position is a quadratic in time,

$$
\mathbf{r}(t) = \mathbf{c}_0 + \mathbf{c}_1 t + \mathbf{c}_2 t^2,
$$

with $\mathbf{c}_0 = \mathbf{r}_0$, $\mathbf{c}_1 = \mathbf{v}_0$ and
$\mathbf{c}_2 = -\tfrac{1}{2}\mu g\,\hat{\mathbf{d}}$ for the state's friction
coefficient and direction (zero at rest). This is the key fact behind event
detection: every geometric condition becomes a polynomial equation in $t$.

## 4. Event detection

The next event is the earliest of:

| Event | Condition | Degree in $t$ |
|---|---|---|
| state transition | $t = \tau_\text{slide}, \tau_\text{roll}, \tau_\text{spin}$ | linear |
| ball–ball | $\lVert \mathbf{r}_a(t) - \mathbf{r}_b(t) \rVert_{xy} = 2R$ | 4 |
| straight cushion | distance to the cushion's nose line $= d_c$ | 2 |
| jaw tip (circular cushion) | distance to the circle's centre $= \rho + R$ | 4 |
| pocket | distance to the pocket's centre $= r_p$ | 4 |

### 4.1 Ball–ball

Write the relative position as
$\Delta\mathbf{r}(t) = \mathbf{a} + \mathbf{b}t + \mathbf{c}t^2$ (horizontal
components only, from the difference of the two balls' polynomials). Contact
happens when

$$
f(t) = \lVert\Delta\mathbf{r}(t)\rVert^2 - (2R)^2
= (\mathbf{c}\cdot\mathbf{c})\,t^4 + 2(\mathbf{b}\cdot\mathbf{c})\,t^3
+ (\mathbf{b}\cdot\mathbf{b} + 2\,\mathbf{a}\cdot\mathbf{c})\,t^2
+ 2(\mathbf{a}\cdot\mathbf{b})\,t + \mathbf{a}\cdot\mathbf{a} - 4R^2 = 0.
$$

Only a **closing** root counts: one where the balls approach,
$f(t^\ast) = 0$ with $f'(t^\ast) < 0$. A root where $f$ only touches zero
($f' = 0$, a graze) or rises through it (the balls separating after a contact)
is not a collision. Ignoring this distinction is exactly the defect we found
in pooltool 0.6.0 (Section 8).

### 4.2 Cushions at the nose height

A real cushion does not touch the ball at its equator. Its nose, modelled as a
cylinder of radius $\rho = 1\,\mathrm{mm}$ along the cushion at height $h$,
meets the ball above the centre (the ball's centre is at height $R$). The ball
touches the nose when the distance between the ball's centre and the nose axis
is $R + \rho$. Since the vertical offset is $h - R$, the horizontal distance at
contact is

$$
d_c = \sqrt{(R + \rho)^2 - (h - R)^2}.
$$

With the cushion line through $\mathbf{p}_1$ with in-plane unit normal
$\hat{\mathbf{n}}$, the signed horizontal gap is linear in $\mathbf{r}$, hence
quadratic in $t$:

$$
g(t) = s\,\hat{\mathbf{n}}\cdot(\mathbf{r}(t) - \mathbf{p}_1) - d_c = 0,
$$

where $s = \pm 1$ puts the ball on the positive side. A root counts only if
the contact point lies within the segment and the ball is approaching.
Contacting above the equator is what couples a cushion's rebound with the
ball's spin (Section 5.3).

### 4.3 Jaw tips and pockets

Rounded jaw tips are vertical cylinders of radius $\rho_j$; the ball touches
one when $\lVert\mathbf{r}(t) - \mathbf{c}\rVert_{xy} = \rho_j + R$. A ball
drops when its centre enters the pocket circle,
$\lVert\mathbf{r}(t) - \mathbf{c}_p\rVert_{xy} = r_p$. Both are quartics of the
same form as 4.1.

### 4.4 Finding the first closing root

General closed-form quartic formulas (Ferrari) lose accuracy badly near double
roots, which are common here (two balls barely touching, a ball rolling to a
stop against a cushion). The simulator instead isolates roots with a method
that is robust by construction (`sim/roots.cpp`):

1. **Split into monotone pieces.** The roots of $f'$ (a cubic, found the same
   way, recursively down to a linear equation) split $[t_0, t_1]$ into
   intervals on which $f$ is monotone, so each contains at most one root.
2. **Bracket.** An interval holds a root exactly when $f$ changes sign across
   it.
3. **Safeguarded Newton.** Newton's iteration
   $t_{k+1} = t_k - f(t_k)/f'(t_k)$ converges quadratically; whenever it would
   leave the bracket, a bisection step is taken instead. This keeps Newton's
   speed with bisection's guarantee.
4. **Classify.** Return the earliest root with $f' < 0$ (closing); skip
   grazes and opening crossings.

Collisions are resolved with the balls exactly in contact plus
$10^{-6}\,\mathrm{m}$ (`MIN_DIST`), so round-off can never leave two balls
interpenetrating and immediately "colliding" again.

## 5. Collision models

### 5.1 The cue strike

The cue (mass $M = 0.567\,\mathrm{kg}$) hits the ball at speed $V_0$, in
direction $\phi$, elevated by $\theta$, at a point offset by $a$ (side) and
$b$ (height) in units of $R$ from the centre. The model is an instantaneous,
point-like impulse (Alciatore, technical proof A-30). The contact point in the
ball's frame, rotated by the elevation, is

$$
\mathbf{Q} = R\,(a,\ c\cos\theta - b\sin\theta,\ c\sin\theta + b\cos\theta),
\qquad c = \sqrt{1 - a^2 - b^2}.
$$

Conservation of momentum and angular momentum through the impulse, with an
elastic tip, gives the ball's speed

$$
v = \frac{2V_0}{1 + \dfrac{m}{M} + \dfrac{Q_x^2 + (Q_z\cos\theta)^2 + (Q_y\sin\theta)^2 - 2Q_yQ_z\cos\theta\sin\theta}{\tfrac{2}{5}R^2}},
$$

and an angular velocity proportional to $\mathbf{Q}\times$ (impulse direction):

$$
\boldsymbol{\omega} = \frac{v}{\tfrac{2}{5}R^2}\,\big(-Q_y\sin\theta + Q_z\cos\theta,\ Q_x\sin\theta,\ -Q_x\cos\theta\big),
$$

both expressed in the strike frame and then rotated by $\phi$. Off-centre hits
cost speed (the denominator grows) and add spin: follow and draw from $b$,
side spin from $a$, and with elevation a tilted spin axis, which produces swerve
and massé.

**Squirt.** A side-spin hit pushes the cue ball slightly off the line of the
cue, away from the spin. With the cue's effective end mass $m_e = m/30$, the
deflection angle is

$$
\alpha = -\arctan\!\left(\frac{\tfrac{5}{2}\,a\,\sqrt{1 - a^2}}{1 + \dfrac{m}{m_e} + \tfrac{5}{2}(1 - a^2)}\right),
$$

about $1.4^\circ$ for a $0.35R$ offset. Low end-mass shafts squirt less, which
is why they exist.

Vertical velocity is discarded, so a steeply elevated cue does not yet make
the ball jump (Section 9).

### 5.2 Ball–ball: frictional, inelastic

Work in the frame of the line of centres $\hat{\mathbf{n}}$ at contact. Along
the normal, momentum is conserved and restitution $e_b$ scales the relative
speed. For equal masses:

$$
v_{1n}' = \tfrac{1}{2}\left[(1 - e_b)\,v_{1n} + (1 + e_b)\,v_{2n}\right],
\qquad
v_{2n}' = \tfrac{1}{2}\left[(1 + e_b)\,v_{1n} + (1 - e_b)\,v_{2n}\right].
$$

Tangentially, the balls' surfaces rub. Their relative surface velocity at the
contact is

$$
\mathbf{s} = \left(\mathbf{v}_1 + \boldsymbol{\omega}_1 \times R\hat{\mathbf{n}}\right) - \left(\mathbf{v}_2 + \boldsymbol{\omega}_2 \times (-R\hat{\mathbf{n}})\right)
$$

(tangential parts). Friction acts against $\mathbf{s}$ with an impulse
proportional to the normal impulse, $\mu\,\Delta v_n$, changing each ball's
tangential velocity by $\mp\mu\,\Delta v_n\,\hat{\mathbf{s}}$ and its spin by
$\tfrac{5}{2R}\,\hat{\mathbf{n}} \times \Delta\mathbf{v}_t$. If that much
friction would reverse the slip, the surfaces instead stop slipping during the
impact (**rolling contact**), and the tangential changes are the ones that make
$\mathbf{s} = 0$:

$$
\Delta\mathbf{v}_{1t} = -\tfrac{1}{7}\left(\mathbf{v}_1 - \mathbf{v}_2 + R\,(\boldsymbol{\omega}_1 + \boldsymbol{\omega}_2) \times \hat{\mathbf{n}}\right),
\qquad
\Delta\boldsymbol{\omega} = -\tfrac{5}{14}\left(\frac{\hat{\mathbf{n}} \times (\mathbf{v}_1 - \mathbf{v}_2)}{R} + \boldsymbol{\omega}_1 + \boldsymbol{\omega}_2\right).
$$

The friction coefficient falls with sliding speed, following Alciatore's fit to
measurements (technical proof A-14):

$$
\mu(\lVert\mathbf{s}\rVert) = 9.951\times10^{-3} + 0.108\,e^{-1.088\,\lVert\mathbf{s}\rVert}.
$$

This tangential friction is what makes **throw**: a cut shot's object ball
leaves slightly off the line of centres, more at low speed (where $\mu$ is
larger) and with outside spin cancelling it.

### 5.3 Ball–cushion: Han (2005)

Han's model treats the cushion impact as a rigid-body impulse at the nose
contact, which sits above the ball's equator at angle

$$
\theta_a = \arcsin\!\left(\frac{h}{R} - 1\right)
$$

(about $16^\circ$ here). In the cushion's frame ($x$ towards the cushion), the
slip velocities at the contact point and the compression speed are

$$
s_x = v_x\sin\theta_a - v_z\cos\theta_a + R\,\omega_y,
\qquad
s_y = -v_y - R\,\omega_z\cos\theta_a + R\,\omega_x\sin\theta_a,
\qquad
c = -v_x\cos\theta_a.
$$

With $A = \tfrac{7}{2m}$ and $B = \tfrac{1}{m}$, the normal impulse at the end
of restitution is $P_z^E = -(1 + e_c)\,c / B$. The impulse needed to stop the
contact slipping is $P_z^S = \lVert(s_x, s_y)\rVert / A$. If
$P_z^S \le \mu_c P_z^E$ the contact sticks before the end of the impact and the
tangential impulse is $(s_x, s_y)/A$; otherwise it slides throughout and the
tangential impulse is $\mu_c P_z^E\,(s_x, s_y)/\lVert(s_x, s_y)\rVert$. Rotating
the impulses back into the ball's frame,

$$
\begin{aligned}
P_X &= -P_x^E \sin\theta_a - P_z^E \cos\theta_a, \qquad P_Y = P_y^E, \qquad P_Z = P_x^E\cos\theta_a - P_z^E\sin\theta_a,\\
\Delta\mathbf{v} &= \tfrac{1}{m}(P_X,\ P_Y,\ 0),\\
\Delta\boldsymbol{\omega} &= \tfrac{R}{I}\left(-P_Y\sin\theta_a,\ \ P_X\sin\theta_a - P_Z\cos\theta_a,\ \ P_Y\cos\theta_a\right).
\end{aligned}
$$

Because the contact is above the equator, the rebound angle and speed depend
on the ball's spin: running English lengthens the angle, reverse English
shortens it, topspin and draw change how the ball comes off. A cushion
contacted at the equator, as in the prototype, cannot do any of that.

### 5.4 Pockets

A pocket captures a ball whose centre crosses its circle. The mouths are made
of cushion segments angled into the pocket and rounded jaw tips (Section 6), so
a ball can hit a jaw, rattle and stay out, as on a real table.

## 6. Table geometry

The playing surface is $2.54 \times 1.27\,\mathrm{m}$ (WPA 9 ft). Each pocket
mouth is built from:

- the straight cushions between pockets (6 runs),
- two **jaw** segments per pocket, angled back at the pocket angle
  ($5.3^\circ$ past $45^\circ$ for corners, $7.14^\circ$ for sides),
- a rounded **tip** where each jaw meets its cushion (radius $21\,\mathrm{mm}$
  corners, $8\,\mathrm{mm}$ sides), placed tangent to both lines,
- a **pocket circle**, offset behind the mouth by the pocket depth.

In total: 18 straight segments, 12 tips, 6 pockets. The visual table
(`src/scene/table_geometry.cpp`) is built from the same segments and tips, so
the cushions the player sees are exactly the ones the balls hit.

## 7. The event loop

```text
state ← balls after the strike
loop:
    for each ball: t_transition ← its next state change          (linear)
    for each pair: t_pair ← first closing root of the quartic      (O(n²) pairs)
    for each ball × cushion, tip, pocket: t ← first closing root
    t* ← the smallest of all; if none: everything is at rest — stop
    advance every ball analytically by t*                          (Section 3)
    resolve the event (transition, collision or pocket)            (Section 5)
    record (event, state of every ball)
```

The output, a `ShotTrajectory`, is the list of events and the state of every
ball after each one. Because the motion between events is analytic,
`stateAt(t)` evaluates any time exactly; playback, slow motion and replays are
just evaluations of this function. A break (16 balls, a few hundred events)
simulates in 5–15 ms. The loop is deterministic: the same inputs always give
the same trajectory, bit for bit. Training data, replays and network play all
depend on that.

## 8. Validation

**Analytic checks.** Every state's evolution and transition time is tested
against the closed forms above, including the $\tfrac{5}{7}$ rolling speed and
energy that never increases along a trajectory. The root finder is tested on
random polynomials with known roots, including double roots.

**Golden shots against pooltool.** `tools/golden/generate_golden_shots.py` runs
twelve reference shots through pooltool 0.6.0, configured like the game. They
cover stop, follow, draw, a thin cut, side spin into a cushion, a
running-English bank, a pot, a jaw hit, a three-ball cluster, a length-of-table
draw, a swerve ($15^\circ$) and a massé ($50^\circ$).
`tests/test_sim_golden.cpp` replays them through `bs_sim`. Both simulators
produce the same collisions and pockets in the same order. Final positions
agree within $0.43\,\mathrm{mm}$ (typically below $0.1\,\mathrm{mm}$), and event
times within about $0.03\,\%$. The swerve shot curves $0.70\,\mathrm{m}$ off its
line and the massé $0.47\,\mathrm{m}$ in both.

**A defect in the reference.** The comparison exposed a bug in pooltool 0.6.0.
Right after a ball changes motion state, pooltool can report a collision
between two balls that are apart and **separating**. In the stop shot, at
$t = 2.283\,\mathrm{s}$, the balls are $78\,\mathrm{mm}$ apart with
$f'(t) > 0$ (opening at $0.34\,\mathrm{m\,s^{-1}}$). Its kiss step then moves
them into contact and swaps their velocities, repeating every ~15 ms. `bs_sim`
accepts only closing roots (Section 4.1) and does not have the defect. The
generator detects such events, and the fixtures keep pooltool's reference only
up to the first one.

## 9. Limitations and next steps

- **No airborne motion.** The strike's vertical velocity is dropped, so jump
  shots are not possible and balls never leave the cloth. Supporting them means
  adding a ballistic state, $\mathbf{r}(t) = \mathbf{r}_0 + \mathbf{v}_0 t - \tfrac{1}{2}g t^2\hat{\mathbf{z}}$
  (still quadratic, so event detection carries over), and a ball–cloth bounce
  model.
- **Simplified cushion compliance.** Han's model is rigid. Mathavan et al.
  (2010) integrate the impact over time with a compliant cushion and match
  high-speed footage better; pooltool's Stronge compliant model is an
  alternative.
- **Cloth anisotropy.** The nap of the cloth (a slight drift of slow balls
  rolling with or against it) is not modelled.

## References

- Leckie, W., Greenspan, M. *An Event-Based Pool Physics Simulator.* Advances in Computer Games, LNCS 4250, 2005.
- Leckie, W., Greenspan, M. *Pool Physics Simulation by Event Prediction 1: Motion Transitions.* ICGA Journal 28(4), 2005.
- Kiefl, E. *Pooltool: A Python package for realistic billiards simulation.* JOSS, 2024. https://pooltool.readthedocs.io
- Han, I. *Dynamics in Carom and Three Cushion Billiards.* Journal of Mechanical Science and Technology 19(4), 2005.
- Mathavan, S., Jackson, M. R., Parkin, R. M. *A theoretical analysis of billiard ball dynamics under cushion impacts.* Proc. IMechE Part C 224, 2010.
- Alciatore, D. *The Illustrated Principles of Pool and Billiards*, and technical proofs A-14, A-30, A-31. https://drdavepoolinfo.com/physics/
- World Pool-Billiard Association. *Equipment Specifications.* https://wpapool.com
