# Billiards Saloon — Rendering

Status: physically based renderer with indirect light, reflections and
streamed models (after 0.8.0). Last revised 2026-10-06. Code: `src/render/`,
shaders in `assets/shaders/`, table, hall and model geometry in `src/scene/`.

This document explains the image formation model: what is computed per pixel
and why, the approximations made to run in real time on OpenGL 4.1 (the most
macOS offers), and how the frame is assembled. It covers the physics of light
the renderer approximates, the specific models chosen, and their trade-offs.

## 1. The frame

```text
once per hall      light probes: 189 cube faces ×2 bounces → SH textures;
                   reflection cube map, prefiltered by roughness       (Section 6)
shadow pass   ×3   depth from each lamp, looking down            → shadow map array
scene pass         visible objects only (Section 10), lit, into a
                   multisampled HDR buffer (RGBA16F)
resolve            MSAA samples averaged                           → HDR texture
bloom              downsample ×6, upsample and accumulate          → bloom texture
post               bloom mix, exposure, ACES tone curve, vignette, sRGB, dither
UI                 RmlUi menus and HUD drawn on top
```

The scene is computed in **linear, scene-referred** radiance: values are
proportional to physical light, and can exceed 1 (a lamp's reflection in a
ball is hundreds of times brighter than the cloth beside it). Only the final
pass maps that range to what a display can show. Lighting in linear space is
what makes adding lights and blending correct. Colours authored in sRGB
(material albedos) are linearised first with $c_\text{lin} \approx c^{2.2}$.

## 2. What is being approximated

The light leaving a surface point $\mathbf{x}$ towards the eye direction
$\mathbf{v}$ is given by the rendering equation (Kajiya 1986):

$$
L_o(\mathbf{x}, \mathbf{v}) = L_e(\mathbf{x}, \mathbf{v}) + \int_{\Omega} f_r(\mathbf{x}, \mathbf{l}, \mathbf{v})\, L_i(\mathbf{x}, \mathbf{l})\, (\mathbf{n}\cdot\mathbf{l})\, d\omega_l,
$$

where $f_r$ is the surface's BRDF (how much light arriving from $\mathbf{l}$ is
reflected towards $\mathbf{v}$). The renderer evaluates this integral in three
parts:

1. **Direct light** from the three table lamps, with visibility from shadow
   maps, and from up to 32 practical lights in the hall: sconces, ceiling
   fixtures, window light (Sections 3–5).
2. **Indirect light**, the light the room itself reflects: diffuse from a grid
   of spherical-harmonic light probes, specular from a prefiltered reflection
   cube map of the room (Section 6).
3. **Emission**, for lamp panels, shades and windows.

## 3. The microfacet BRDF

Surfaces are modelled as many tiny mirrors (microfacets) whose orientations
follow a distribution set by roughness. The Cook–Torrance specular term is

$$
f_\text{spec}(\mathbf{l}, \mathbf{v}) = \frac{D(\mathbf{h})\, G(\mathbf{l}, \mathbf{v})\, F(\mathbf{v}\cdot\mathbf{h})}{4\,(\mathbf{n}\cdot\mathbf{l})(\mathbf{n}\cdot\mathbf{v})},
\qquad \mathbf{h} = \frac{\mathbf{l} + \mathbf{v}}{\lVert\mathbf{l} + \mathbf{v}\rVert}.
$$

**Normal distribution: GGX** (Walter et al. 2007; Trowbridge–Reitz), with
$\alpha = \text{roughness}^2$ (Burley's perceptual remapping):

$$
D(\mathbf{h}) = \frac{\alpha^2}{\pi\left[(\mathbf{n}\cdot\mathbf{h})^2(\alpha^2 - 1) + 1\right]^2}.
$$

GGX has a long tail: highlights fade into a soft glow instead of ending
abruptly, which is how polished balls and lacquer look.

**Masking and shadowing: height-correlated Smith** (Heitz 2014), folded with
the $4(\mathbf{n}\cdot\mathbf{l})(\mathbf{n}\cdot\mathbf{v})$ denominator into a
visibility term $V = G / [4(\mathbf{n}\cdot\mathbf{l})(\mathbf{n}\cdot\mathbf{v})]$:

$$
V = \frac{0.5}{(\mathbf{n}\cdot\mathbf{l})\sqrt{(\mathbf{n}\cdot\mathbf{v})^2(1-\alpha^2) + \alpha^2} + (\mathbf{n}\cdot\mathbf{v})\sqrt{(\mathbf{n}\cdot\mathbf{l})^2(1-\alpha^2) + \alpha^2}}.
$$

**Fresnel: Schlick's approximation**, with $F_0$ the reflectance at normal
incidence:

$$
F(\mathbf{v}\cdot\mathbf{h}) = F_0 + (1 - F_0)\,(1 - \mathbf{v}\cdot\mathbf{h})^5,
\qquad F_0 = \left(\frac{n_1 - n_2}{n_1 + n_2}\right)^2.
$$

For dielectrics $F_0$ is small: $0.045$ for phenolic resin (refractive index
about $1.54$), $0.04$ for lacquer. Metals (the trim) use a large $F_0$.

**Diffuse**: Lambertian, weighted by the energy the specular layer did not
reflect:

$$
f_\text{diff} = (1 - F)\,\frac{\rho}{\pi},
$$

with $\rho$ the linear albedo.

## 4. Lamps as sphere lights

A point light gives an infinitely sharp highlight on a smooth ball. Real lamps
have size, so their reflection is a visible patch. The renderer uses Karis's
(2013) **roughness widening**: a sphere light of radius $r_L$ at distance $d$
subtends a half-angle of about $r_L/d$, which is approximated by widening the
lobe,

$$
\alpha' = \operatorname{clamp}\!\left(\alpha + \frac{r_L}{2d},\ 0,\ 1\right),
$$

and keeping the energy of the original lobe by scaling $D$ by
$(\alpha/\alpha')^2$. Radiance falls with the inverse square of distance:

$$
L_i = \frac{I\,\mathbf{c}}{d^2 + \varepsilon},
$$

with $\varepsilon$ a small constant that avoids a singularity right at the
light. The three lamps' colour and intensity come from the hall's lighting mood
(`assets/data/equipment/catalog.json`).

### Practical lights

A room lit only by the table lamps is a black box around a bright table. Real
saloons and arenas are lit by many fittings. Each hall declares them in its
geometry (`HallGeometry::lights`); they become `LocalLightComponent` entities,
scaled by the hall's `practical` brightness. A light has a finite **range**
$r$: its radiance falls with the inverse square, multiplied by a window that
reaches zero smoothly at $r$ (Karis 2013):

$$
L_i(d) = \frac{I\,\mathbf{c}}{d^2 + \varepsilon}\,\left[\operatorname{saturate}\!\left(1 - (d/r)^4\right)\right]^2 .
$$

The window changes the light little near it (at $d = r/2$ it is $0.88$) and
removes the long tail, where an inverse-square light contributes almost
nothing but still costs a shader evaluation. Spots multiply by a smooth cone,
$\operatorname{smoothstep}(\cos\theta_\text{outer}, \cos\theta_\text{inner}, -\mathbf{l}\cdot\hat{\mathbf{s}})$.

**Choosing the lights per draw.** A finite range makes the set of lights that
can reach an object exact: light $k$ reaches a bounding box $B$ when the
closest point of $B$ to the light, $\mathbf{q} = \operatorname{clamp}(\mathbf{p}_k, B_\text{min}, B_\text{max})$,
is within range, $\lVert\mathbf{p}_k - \mathbf{q}\rVert < r_k$. All of a hall's
lights are uploaded once a frame; each draw names the (at most 8) that reach
it, brightest first by $I_k / (d_k^2 + 0.25)$. This is *forward shading with
per-object light lists*, the simplest scheme that scales with the number of
lights without compute shaders, which OpenGL 4.1 does not have. With 20 lights
in the saloon a draw shades at most 8.

Practical lights cast no shadows. Their light is soft and comes from many
directions at once, the table lamps' shadows carry the contact shading that
matters, and the probes (Section 6) supply the occlusion of large shapes.

**Fittings show only their glow.** A lamp panel or a globe is drawn as an
emissive surface, and its light comes from the point light placed inside it.
If the fitting were also lit by that light, a few centimetres away, it would
reflect $\rho/\pi \cdot I/d^2$, which is enormous: the arena's canopy panels
became a second emitter about 60 times brighter than the room, flooded the
probes and washed out the cloth. Surfaces of the lamp type therefore output
their emission and nothing else.

## 5. Materials

**Balls: polished phenolic resin.** One hard dielectric layer: roughness
$0.06$, $F_0 = 0.045$. An early version layered a clear coat over a rougher
base. Each lamp then produced a broad, lamp-coloured glow that swamped the
ball's colour and made red balls look brown. A single smooth layer gives what
pool balls actually show: small, sharp lamp reflections over saturated colour.

**Ball decoration.** Stripes, spots and numbers are computed on the sphere in
its own frame, so they turn with the ball's rolling rotation. The white spot
is a cap of angular radius $20^\circ$ about the local $\pm z$ axis. A point
$\hat{\mathbf{p}}$ on the sphere maps into the spot's disc by orthographic
projection onto the tangent plane,
$\mathbf{s} = (p_x, p_y)/\sin 20^\circ$, which samples the number atlas: the
digits 1–15 rasterised at startup with FreeType from the HUD's typeface.

**Cloth: velvet sheen.** Baize is a mat of fibres that scatter light forward
and back at grazing angles. Microfacet models of smooth surfaces miss this.
The renderer uses the "Charlie" sheen distribution (Estevez & Kulla 2017) with
Neubelt & Pettineo's (2013) visibility:

$$
D_\text{sheen}(\mathbf{h}) = \frac{(2 + 1/\alpha)\,\sin(\theta_h)^{1/\alpha}}{2\pi},
\qquad
V_\text{sheen} = \frac{1}{4\left(\mathbf{n}\cdot\mathbf{l} + \mathbf{n}\cdot\mathbf{v} - (\mathbf{n}\cdot\mathbf{l})(\mathbf{n}\cdot\mathbf{v})\right)},
$$

where $\theta_h$ is the angle between $\mathbf{n}$ and $\mathbf{h}$. The weave
pattern modulates albedo procedurally.

**Lacquered wood and leather**: a base layer plus an optional clear coat, a
second GGX lobe with $F_0 = 0.04$ and low roughness, whose Fresnel factor
$F_c$ removes energy from the base:

$$
f = (1 - F_c)\,(f_\text{diff} + f_\text{spec}) + f_\text{coat}.
$$

## 6. Light from the room

The second term of the rendering equation needs the radiance arriving from
every direction: the walls, the ceiling, the floor, the cloth. That light has
itself been reflected, so it is the hard part of global illumination. A real
time renderer cannot trace it per pixel. Here it is precomputed when a hall
loads, in two parts that mirror the split of the BRDF into diffuse and
specular.

### Diffuse: light probes in spherical harmonics

For a Lambertian surface the reflected light depends only on the
**irradiance**, the cosine-weighted integral of incoming radiance:

$$
E(\mathbf{x}, \mathbf{n}) = \int_{\Omega} L_i(\mathbf{x}, \boldsymbol{\omega})\, \max(\mathbf{n}\cdot\boldsymbol{\omega}, 0)\, d\omega,
\qquad
L_o = \frac{\rho}{\pi}\, E .
$$

Irradiance varies slowly with $\mathbf{n}$: the clamped cosine is a low-pass
filter. Ramamoorthi and Hanrahan (2001) showed that if the radiance is
expanded in real spherical harmonics $Y_{lm}$,

$$
L(\boldsymbol{\omega}) \approx \sum_{l=0}^{2}\sum_{m=-l}^{l} c_{lm}\, Y_{lm}(\boldsymbol{\omega}),
\qquad
c_{lm} = \int_{S^2} L(\boldsymbol{\omega})\, Y_{lm}(\boldsymbol{\omega})\, d\omega,
$$

then convolution with the clamped cosine multiplies each band by a constant,
$\hat A_0 = \pi$, $\hat A_1 = 2\pi/3$, $\hat A_2 = \pi/4$, and higher bands
nearly vanish ($\hat A_3 = 0$, $\hat A_4 = -\pi/24$). Nine coefficients per
colour therefore give irradiance with an average error around 1%:

$$
E(\mathbf{n}) = \sum_{l=0}^{2}\sum_{m} \hat A_l\, c_{lm}\, Y_{lm}(\mathbf{n}).
$$

**Capture.** A grid of probes covers the room (9×3×7 in the saloon, about a
metre apart). From each probe the lit room is rendered into the six
$32\times32$ faces of a cube map, all faces of all probes as tiles of one
atlas, read back once. Each texel's direction $\boldsymbol{\omega}$ and solid
angle weight the projection. A texel at face coordinates $(u, v) \in [-1, 1]^2$
covers

$$
\Delta\omega \approx \frac{\Delta u\, \Delta v}{(1 + u^2 + v^2)^{3/2}},
$$

so $c_{lm} \approx \sum_\text{texels} L\, Y_{lm}(\boldsymbol{\omega})\, \Delta\omega$,
normalised so the weights sum to exactly $4\pi$. The coefficients (27 numbers
per probe) are packed into seven RGBA 3D textures. The shader samples them with
hardware trilinear filtering, so each pixel blends its eight nearest probes,
and evaluates $E(\mathbf{n})$. It samples a little in front of the surface,
$\mathbf{x} + 0.08\,\mathbf{n}$, so that a probe just behind a wall does not
leak light from the other side into it. The projection and its accuracy are
unit-tested (`tests/test_spherical_harmonics.cpp`): a uniform environment must
give $E = \pi L$ in every direction, and hemisphere lighting must light floors
and not ceilings.

**Two bounces.** The capture runs twice. The first time, the probes see only
direct light. The second time the room is drawn lit by the first result, so
the probes see light that has bounced once, and the final probes hold two
bounces. This is the first two terms of the Neumann series of the rendering
equation, $L = L_e + TL_e + T^2L_e + \dots$, where $T$ is the transport
operator. Each bounce off the room's surfaces, with albedos of 0.1–0.5, keeps
a fraction of the light, so the third bounce would add a few percent at most.
During capture, surfaces are shaded diffuse only (view-dependent highlights
would be noise to a probe), and emission is left out: every fitting's light
already enters through its lamp.

**When it runs.** Once per hall and lighting change, after the hall's models
have loaded (Section 10). It draws from a list of objects resolved once, each
with its coarsest level of detail and its lights already chosen, and skips
objects under about 20 cm. That brought the capture from 2.5 s to 0.8 s on the
development Mac.

### Specular: a parallax-corrected reflection cube map

Glossy and mirror surfaces need radiance from one direction, with detail. The
room is rendered once more into a $256^2$ cube map from above the table. The
split-sum approximation (Karis 2013) separates the specular integral into the
environment, pre-convolved with the GGX lobe, times the BRDF's response to a
white environment (the closed form given above, $F_0 A + B$). The
pre-convolution assumes $\mathbf{n} = \mathbf{v} = \mathbf{r}$ and is stored
as mip levels, roughness $k/5$ at level $k$:

$$
\tilde L(\mathbf{r}, \alpha) = \frac{\sum_j L(\mathbf{l}_j)\,(\mathbf{n}\cdot\mathbf{l}_j)}{\sum_j \mathbf{n}\cdot\mathbf{l}_j},
\qquad \mathbf{l}_j = 2(\mathbf{r}\cdot\mathbf{h}_j)\mathbf{h}_j - \mathbf{r},
\quad \mathbf{h}_j \sim D_\text{GGX}.
$$

The 64 half vectors per texel come from a Hammersley sequence. **Filtered
importance sampling** (Křivánek and Colbert 2008) removes the noise: a sample
represents the solid angle $\Omega_s = 1/(N\,p)$, with
$p = D(\mathbf{h})/4$ the pdf of $\mathbf{l}$ when $\mathbf{n} = \mathbf{v}$.
It reads the source mip whose texels cover that angle,

$$
\text{lod} = \tfrac12 \log_2\!\frac{\Omega_s}{\Omega_t} + 1,
\qquad \Omega_t = \frac{4\pi}{6\,W^2},
$$

where $W$ is the face width in texels.

A cube map treats everything it sees as infinitely far away, which is wrong
inside a room: the bar mirror would reflect the far wall as if seen from the
capture point. **Box projection** (Lagarde and Zanuttini 2012) corrects this.
The room is approximated by its box. The reflected ray from the shaded point
$\mathbf{x}$ is intersected with the box, and the cube map is looked up in the
direction from the capture point $\mathbf{c}$ to that hit:

$$
t = \min_{i \in \{x,y,z\}} \max\!\left(\frac{b^\text{max}_i - x_i}{r_i},\ \frac{b^\text{min}_i - x_i}{r_i}\right),
\qquad
\mathbf{d} = \mathbf{x} + t\,\mathbf{r} - \mathbf{c}.
$$

Rough lobes are wide, and a single corrected direction would make the box's
corners visible in them, so the lookup blends back towards $\mathbf{r}$ by
$\alpha^2$.

## 7. Shadows

Each lamp renders a **shadow map**: the depth of the nearest surface seen from
the lamp, through a $140^\circ$ perspective looking straight down. The three
maps are layers of one depth texture array. A point $\mathbf{x}$ is lit by lamp
$k$ if it is not behind that nearest surface:

$$
\text{lit}_k(\mathbf{x}) = \left[\, z_k(\mathbf{x}) \le z_\text{map}\big(\pi_k(\mathbf{x})\big) + b \,\right].
$$

Here $\pi_k$ projects into the lamp's view, $z_k$ is the point's depth from the
lamp, and $b$ is a bias.

**Acne and bias.** The depth map samples a surface at discrete texels, and a
surface compared against its own sampled depth flickers between lit and
shadowed ("shadow acne"). Two biases prevent this. In the shadow pass,
`glPolygonOffset` pushes depths back in proportion to the surface's slope. When
sampling, the point is offset along its normal by $4\,\mathrm{mm}$. Too much
bias detaches shadows from their casters ("peter panning"). These values keep
ball contact shadows attached.

**Soft edges: percentage-closer filtering.** The hardware compares and
bilinearly filters four texels per lookup; the shader averages a $k \times k$
grid of such lookups ($k = 1, 3, 5$ by quality):

$$
s(\mathbf{x}) = \frac{1}{k^2} \sum_{i,j} \text{lit}\big(\pi(\mathbf{x}) + (i, j)\,\Delta\big).
$$

**Resolution.** At $2048^2$ over a $140^\circ$ field from $1\,\mathrm{m}$ up, a
texel covers about $2\tan 70^\circ / 2048 \approx 2.7\,\mathrm{mm}$ of cloth, a
tenth of a ball radius. That is enough for crisp contact shadows.

## 8. From radiance to the screen

**Multisampling.** The scene buffer stores 4 or 8 samples per pixel (by
quality). Edges are covered by several samples, and the resolve averages them.
Averaging happens in linear HDR, before tone mapping. Very bright edges (a lamp
reflection against dark cloth) can therefore still look slightly aliased after
tone mapping, an accepted trade-off for using hardware MSAA.

**Bloom.** Bright light scatters in camera lenses and in the eye, so very
bright points glow. The bloom is energy-conserving: no brightness threshold.
The resolved image is filtered down a six-level pyramid with Jimenez's
(2014) 13-tap filter, then back up with a $3\times3$ tent, each level added to
the next larger one. The first downsample weights its sample groups by
$w = 1/(1 + Y)$, where $Y$ is the group's luminance (the "Karis average"), so
a single very bright pixel cannot flicker into a large blob from frame to
frame. The result is mixed in at a small fraction:
$\mathbf{c} \leftarrow (1 - \beta)\,\mathbf{c} + \beta\,\mathbf{c}_\text{bloom}$,
with $\beta = 0.045$.

**Exposure and tone mapping.** The HDR colour is scaled by the hall's exposure
and passed through the ACES filmic curve, in Stephen Hill's fit of the
reference rendering and output transforms. Colour is converted into the ACES
working space, compressed by a rational curve per channel, and converted back:

$$
f(x) = \frac{x\,(x + 0.0245786) - 0.000090537}{x\,(0.983729\,x + 0.4329510) + 0.238081}.
$$

The curve has a toe (gentle blacks), a straight middle, and a shoulder that
rolls highlights off smoothly instead of clipping them. That shoulder is what
lets lamp reflections read as very bright without turning into flat white
patches.

**Display encoding.** The tone-mapped value is encoded for the display with
the exact sRGB transfer function,

$$
c_\text{sRGB} =
\begin{cases}
12.92\,c & c \le 0.0031308, \\
1.055\,c^{1/2.4} - 0.055 & \text{otherwise,}
\end{cases}
$$

after a gentle vignette. Half a code value of random noise (dither) is added
last, so smooth dark gradients on the cloth and walls do not show banding at
8 bits.

## 9. Geometry

### Loaded models

Furniture and decor are glTF 2.0 models (`assets/models/props/`, CC0 from Poly
Haven). The loader (`src/scene/model_loader.cpp`, cgltf and stb_image) bakes
node transforms into the vertices, with the inverse transpose for normals, and
restores the winding under mirroring transforms ($\det < 0$). It decodes each
texture once, even when materials share it. glTF's **metallic-roughness**
model maps onto the same microfacet BRDF: a metal has no diffuse term and
tinted reflectance, a dielectric has 4%:

$$
\rho_\text{diffuse} = \mathbf{b}\,(1 - m), \qquad F_0 = \operatorname{mix}(0.04,\ \mathbf{b},\ m),
$$

with $\mathbf{b}$ the base colour (stored as sRGB textures, so the hardware
returns linear values), and $m$ and the roughness from the blue and green
channels of the packed map, whose red channel is ambient occlusion. Occlusion
scales the indirect terms only, since direct light has its own shadows.

**Normal maps without tangents.** Tangent-space normal maps need a tangent
frame per vertex. Instead, the shader builds one per pixel from screen-space
derivatives (Schüler 2006). The UV gradient with respect to surface position
is solved from $\partial\mathbf{p}/\partial x$, $\partial\mathbf{p}/\partial y$
and the matching UV derivatives. This gives the cotangent frame
$(\mathbf{T}, \mathbf{B}, \mathbf{n})$, scaled so its larger axis has unit
length. It costs a few instructions and no vertex data.

### Procedural geometry

The table is generated, not modelled by hand, from the simulator's own table
(`src/scene/table_geometry.cpp`):

- Each **cushion** between two pockets is a solid. In plan, it is outlined by
  the simulator's nose line, the two jaw segments and arcs around the rounded
  jaw tips (the arc between the two tangent points on each tip circle). It is
  extruded from the cloth to the nose height, with a top sloping up to the
  rail. The outline is convex, so its top is a fan of triangles from the
  centroid.
- The **rail** is a ring of $1\,\mathrm{cm}$ cells around the cushions, with the
  cells whose centres fall inside a pocket opening left out. A leather rim
  starting $12.5\,\mathrm{mm}$ inside the cut hides the steps.
- **Pocket drops** are cylinders whose walls face inward, so they are seen from
  inside through the opening.

Face orientation is decided geometrically, not by vertex order: each triangle
or wall is flipped if its normal points the wrong way (up for tops, away from
the solid's centre for walls). This makes the generated meshes immune to the
winding mistakes that are easy to make by hand. One such mistake, in the
prototype's sphere mesh, went unnoticed for months: culling showed the inside
of the far half of every ball. The physically based shading exposed it,
because the visible surface's normals pointed away from the lamps.

## 10. Visibility, levels of detail and streaming

**Frustum culling.** Each mesh keeps its local bounding box. A draw transforms
it to world space with Arvo's (1990) method: the centre moves with the matrix,
and each world half-extent is $h'_i = \sum_j \lvert M_{ij}\rvert\, h_j$, exact
for the transformed box. The six planes of the camera's frustum come straight
from the rows $\mathbf{m}_i$ of the view-projection matrix (Gribb and Hartmann
2001): $\mathbf{m}_4 \pm \mathbf{m}_1$, $\mathbf{m}_4 \pm \mathbf{m}_2$,
$\mathbf{m}_4 \pm \mathbf{m}_3$, normalised. A box with centre $\mathbf{c}$ and
half-extents $\mathbf{h}$ is outside plane $(\mathbf{n}, w)$ when

$$
\mathbf{n}\cdot\mathbf{c} + w < -\left(h_x\lvert n_x\rvert + h_y\lvert n_y\rvert + h_z\lvert n_z\rvert\right),
$$

the box's projected radius on the normal. A box outside any plane is skipped.
The test is conservative: it never culls a visible box. The shadow pass culls
against each lamp's frustum the same way. For culling to help, scenery is
built as spatially separate parts (each wall, each side of the stands, each
furniture group), never as one mesh spanning the room.

**Levels of detail.** Each model part gets three levels: the original, then
about 35% and 12% of its triangles. They are made with meshoptimizer's
simplifier, which collapses edges by the quadric error metric (Garland and
Heckbert 1997) within a bound on the geometric error. Exported meshes split
vertices along every UV and normal seam, and the simplifier will not collapse
across such borders. Each mesh is therefore first welded by position alone
(a "shadow" index buffer); the seams it blurs are invisible at distance. Parts
made of many small pieces (the bar stools' rods) still cannot shrink within
the error bound, so the coarsest levels fall back to vertex clustering on a
grid ("sloppy" simplification). The renderer picks a level by how large the
part's bounding sphere of radius $R$ looks at distance $d$, for a vertical
field of view $\phi$:

$$
s = \frac{R}{d\,\tan(\phi/2)}, \qquad
\text{level} = \begin{cases} 0 & s > 0.12 \\ 1 & 0.04 < s \le 0.12 \\ 2 & \text{otherwise.} \end{cases}
$$

Shadow maps and light probes always use the coarsest level: a shadow-map texel
is several millimetres wide, and a probe face is 32 texels across.

**Streaming.** Models load on worker threads (`std::async`). The game keeps
running, and each model's placements (`assets/data/halls/props.json`) appear
as it finishes. Parsing, texture decoding and LOD generation all happen off the
render thread; only the GPU upload happens on first draw. Loaded models stay
cached, so a new match or a return to a hall costs nothing. The light probes
wait for the last model, so they are captured once with all the furniture in
place.

**What was measured, and what was left out.** At 3110×1800 with 4× MSAA on
the development Mac (Apple GPU):

| Scene | GPU / frame | Draws | Triangles | Objects drawn / culled |
|---|---|---|---|---|
| Old hall, before this work | 6.5 ms | ~100 | ~200k | all |
| Furnished saloon, culling + LOD | 10.5 ms | ~300 | 330k | 57 / 125 |

The cost is dominated by shading pixels, not by geometry. **Occlusion
culling** (hardware queries or a depth pyramid) was therefore not
implemented: a single room seen from inside hides little behind its few large
occluders, and the queries' latency would cost more than it saves. **Asset
streaming by visibility**, keeping only what the camera sees in memory, would
pay off for open worlds, not for a room whose assets total under 6 MB. Both
remain options if halls grow beyond one room.

## 11. Quality presets

| Preset | MSAA | Shadow map | PCF | Bloom |
|---|---|---|---|---|
| Low | 1× | $1024^2$ | $1\times1$ | off |
| Balanced | 4× | $2048^2$ | $3\times3$ | on |
| High | 8× | $2048^2$ | $5\times5$ | on |

## References

- Kajiya, J. T. *The Rendering Equation.* SIGGRAPH 1986.
- Walter, B. et al. *Microfacet Models for Refraction through Rough Surfaces.* EGSR 2007.
- Burley, B. *Physically-Based Shading at Disney.* SIGGRAPH course notes, 2012.
- Karis, B. *Real Shading in Unreal Engine 4.* SIGGRAPH course notes, 2013.
- Heitz, E. *Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs.* JCGT 3(2), 2014.
- Estevez, A. C., Kulla, C. *Production Friendly Microfacet Sheen BRDF.* Sony Pictures Imageworks, 2017.
- Neubelt, D., Pettineo, M. *Crafting a Next-Gen Material Pipeline for The Order: 1886.* SIGGRAPH course notes, 2013.
- Jimenez, J. *Next Generation Post Processing in Call of Duty: Advanced Warfare.* SIGGRAPH course notes, 2014.
- Hill, S. *ACES fitted tone curve* (BakingLab), 2016; Academy Color Encoding System.
- Ramamoorthi, R., Hanrahan, P. *An Efficient Representation for Irradiance Environment Maps.* SIGGRAPH 2001.
- Sloan, P.-P. *Stupid Spherical Harmonics (SH) Tricks.* GDC 2008.
- Křivánek, J., Colbert, M. *Real-time Shading with Filtered Importance Sampling.* EGSR 2008.
- Lagarde, S., Zanuttini, A. *Local Image-based Lighting with Parallax-corrected Cubemap.* SIGGRAPH 2012 talk.
- Schüler, C. *Normal Mapping without Precomputed Tangents.* ShaderX5, 2006.
- Arvo, J. *Transforming Axis-Aligned Bounding Boxes.* Graphics Gems, 1990.
- Gribb, G., Hartmann, K. *Fast Extraction of Viewing Frustum Planes from the World-View-Projection Matrix.* 2001.
- Garland, M., Heckbert, P. *Surface Simplification Using Quadric Error Metrics.* SIGGRAPH 1997.
- Kapoulkine, A. *meshoptimizer.* https://github.com/zeux/meshoptimizer
- Williams, L. *Casting Curved Shadows on Curved Surfaces.* SIGGRAPH 1978 (shadow mapping); Reeves, W. et al., *Rendering Antialiased Shadows with Depth Maps*, SIGGRAPH 1987 (PCF).
