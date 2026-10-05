# Billiards Saloon — Rendering

Status: physically based renderer, milestone M6 (0.6.0). Last revised
2026-10-06. Code: `src/render/`, shaders in `assets/shaders/`, table and hall
geometry in `src/scene/`.

This document explains the image formation model: what is computed per pixel
and why, the approximations made to run in real time on OpenGL 4.1 (the most
macOS offers), and how the frame is assembled. It covers the physics of light
the renderer approximates, the specific models chosen, and their trade-offs.

## 1. The frame

```text
shadow pass   ×3   depth from each lamp, looking down            → shadow map array
scene pass         every object, lit, into a multisampled HDR buffer (RGBA16F)
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

1. **Direct light from the three lamps**, the dominant term in a darkened hall,
   as a sum over lights with visibility from shadow maps (Sections 3–5).
2. **Indirect light from the room**, as a pre-integrated analytic environment
   (Section 6).
3. **Emission**, for lamp panels and highlights.

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

Each pixel cannot integrate the whole environment. The environment term is
split into two pre-integrated factors (the "split-sum" approximation): the
environment's radiance in the reflected direction, blurred by roughness, times
the BRDF's integral against a white environment. The second factor depends only
on $F_0$, roughness and $\mathbf{n}\cdot\mathbf{v}$. It uses Karis's
closed-form fit instead of a lookup texture:

$$
\int f_r\,(\mathbf{n}\cdot\mathbf{l})\,d\omega \approx F_0\,A(\alpha, \mathbf{n}\cdot\mathbf{v}) + B(\alpha, \mathbf{n}\cdot\mathbf{v}),
$$

with $A$ and $B$ small polynomial and exponential expressions. The environment
itself is analytic: a dark floor, horizon and ceiling, plus a warm lobe above
for the lamps. The halls are dark, so it is kept weak (scaled by $0.45$), and
the lamps dominate as they do on television.

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

## 10. Quality presets

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
- Williams, L. *Casting Curved Shadows on Curved Surfaces.* SIGGRAPH 1978 (shadow mapping); Reeves, W. et al., *Rendering Antialiased Shadows with Depth Maps*, SIGGRAPH 1987 (PCF).
