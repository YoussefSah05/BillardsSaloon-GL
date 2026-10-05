#version 410 core

// HDR scene -> display: bloom, exposure, ACES filmic tone curve, vignette,
// sRGB encoding and a little dither against banding in dark gradients.
in vec2 vUv;

uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform int uBloomEnabled;
uniform float uBloomStrength;
uniform float uExposure;

out vec4 FragColor;

// ACES fitted (Stephen Hill): sRGB -> ACES AP1, RRT + ODT fit, back to sRGB.
const mat3 ACES_INPUT = mat3(
    0.59719, 0.07600, 0.02840,
    0.35458, 0.90834, 0.13383,
    0.04823, 0.01566, 0.83777);
const mat3 ACES_OUTPUT = mat3(
     1.60475, -0.10208, -0.00327,
    -0.53108,  1.10813, -0.07276,
    -0.07367, -0.00605,  1.07602);

vec3 rrtAndOdtFit(vec3 v)
{
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

vec3 acesFitted(vec3 color)
{
    color = ACES_INPUT * color;
    color = rrtAndOdtFit(color);
    color = ACES_OUTPUT * color;
    return clamp(color, 0.0, 1.0);
}

vec3 linearToSrgb(vec3 c)
{
    vec3 low = c * 12.92;
    vec3 high = 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), c));
}

void main()
{
    vec3 color = texture(uScene, vUv).rgb;
    if (uBloomEnabled == 1)
    {
        color = mix(color, texture(uBloom, vUv).rgb, uBloomStrength);
    }

    color = acesFitted(color * uExposure);

    vec2 centred = vUv - 0.5;
    float vignette = 1.0 - 0.22 * pow(clamp(length(centred) * 1.3, 0.0, 1.0), 2.4);
    color *= vignette;

    color = linearToSrgb(color);
    float noise = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
    color += (noise - 0.5) / 255.0;

    FragColor = vec4(color, 1.0);
}
