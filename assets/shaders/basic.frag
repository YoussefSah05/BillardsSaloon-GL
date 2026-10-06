#version 410 core

in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec3 vLocalPosition;
in vec2 vTexCoord;

uniform vec3 uViewPosition;

uniform vec3 uDirectionalLightDirection;
uniform vec3 uDirectionalLightColor;

uniform vec3 uPointLightPositions[3];
uniform vec3 uPointLightColors[3];
uniform int uActivePointLightCount;

uniform vec3 uMaterialAlbedo;
uniform float uMaterialSpecularStrength;
uniform float uMaterialShininess;
uniform int uMaterialSurfaceType;
uniform float uMaterialRoughness;
uniform float uMaterialReflectivity;
uniform float uMaterialClearcoatStrength;
uniform float uReflectionScale;
uniform float uEmissionScale;

uniform vec3 uEmissionColor;

uniform int uBallVisualType;

uniform float uAlpha;

// Lamps: inverse-square point lights of a small radius (soft highlights),
// each with a shadow map layer seen from the lamp looking down.
uniform float uLightIntensity;
uniform float uAmbientScale;
uniform float uLightRadius;
uniform mat4 uLightMatrices[3];
uniform sampler2DArrayShadow uShadowMaps;
uniform int uShadowsEnabled;
uniform int uShadowKernel;          // PCF taps per side: 1, 3 or 5
uniform float uShadowTexel;

// Practical lights (sconces, ceiling and bar lights): all of the hall's are
// uploaded once a frame; each draw names the few that reach it.
const int MAX_LOCAL_LIGHTS = 32;
const int LIGHTS_PER_DRAW = 8;
uniform vec4 uLocalPositionRange[MAX_LOCAL_LIGHTS];   // xyz position, w range
uniform vec4 uLocalColor[MAX_LOCAL_LIGHTS];           // rgb colour x intensity, w spot inner cosine
uniform vec4 uLocalSpot[MAX_LOCAL_LIGHTS];            // xyz direction, w spot outer cosine (< -1: point light)
uniform int uLocalIndices[LIGHTS_PER_DRAW];
uniform int uLocalCount;

// Indirect light: SH irradiance probes on a grid (7 textures hold 27
// coefficients: 9 per colour), and the capture mode that fills them.
uniform sampler3D uProbeSH0;
uniform sampler3D uProbeSH1;
uniform sampler3D uProbeSH2;
uniform sampler3D uProbeSH3;
uniform sampler3D uProbeSH4;
uniform sampler3D uProbeSH5;
uniform sampler3D uProbeSH6;
uniform vec3 uProbeMin;
uniform vec3 uProbeMax;
uniform vec3 uProbeCounts;
uniform int uProbesEnabled;
uniform int uCapture;   // 1 while rendering probes: diffuse light and emission only

// Reflections: the room as a prefiltered cube map (mip = roughness), its
// parallax corrected against the room's box.
uniform samplerCube uReflection;
uniform int uReflectionEnabled;
uniform vec3 uRoomMin;
uniform vec3 uRoomMax;
uniform vec3 uReflectionCentre;
uniform float uReflectionMaxLod;

// Ball numbers: a 4 x 4 atlas, cell n holds number n.
uniform sampler2D uNumberAtlas;
uniform int uBallNumber;
uniform int uMeasleCueBall;   // red spots on the cue ball, as on broadcast tables

out vec4 FragColor;

const int SURFACE_CLOTH = 1;
const int SURFACE_BALL = 2;
const int SURFACE_WOOD = 3;
const int SURFACE_LAMP = 4;

const int BALL_VISUAL_CUE = 1;
const int BALL_VISUAL_SOLID = 2;
const int BALL_VISUAL_STRIPE = 3;
const int BALL_VISUAL_EIGHT = 4;

const float PI = 3.14159265359;

float saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec3 fresnelSchlick(float cosTheta, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
}

float spotMask(vec3 localDirection, vec3 centerDirection, float innerCosine, float outerCosine)
{
    return smoothstep(outerCosine, innerCosine, dot(localDirection, centerDirection));
}

vec3 applyClothFinish(vec3 baseColor)
{
    float weave = 0.5 + 0.5 * sin(vTexCoord.x * 260.0) * sin(vTexCoord.y * 180.0);
    float edgeDistance =
        min(min(vTexCoord.x, 1.0 - vTexCoord.x), min(vTexCoord.y, 1.0 - vTexCoord.y));
    float centerMask = smoothstep(0.01, 0.16, edgeDistance);

    vec3 color = baseColor;
    color *= mix(0.92, 1.04, weave);
    color *= mix(0.82, 1.02, centerMask);
    return color;
}

vec3 applyWoodFinish(vec3 baseColor)
{
    float grain = 0.5 + 0.5 * sin((vTexCoord.x * 14.0 + vTexCoord.y * 1.8) * 12.0);
    float plank = 0.5 + 0.5 * sin(vTexCoord.y * 24.0);

    vec3 color = baseColor;
    color *= mix(0.86, 1.12, grain);
    color *= 0.94 + 0.08 * plank;
    return color;
}

vec3 applyBallFinish(vec3 baseColor)
{
    float localLength = max(length(vLocalPosition), 1.0e-5);
    vec3 localDirection = vLocalPosition / localLength;
    vec3 ivory = vec3(0.95, 0.95, 0.93);

    if (uBallVisualType == BALL_VISUAL_CUE)
    {
        float chalkBloom = 0.5 + 0.5 * sin(localDirection.x * 22.0) * sin(localDirection.z * 19.0);
        vec3 cue = mix(ivory * 0.97, ivory * 1.03, 0.30 * chalkBloom);
        if (uMeasleCueBall == 1)
        {
            // Spots at the twelve corners of an icosahedron: spin shows at any angle.
            const float g = 0.5257311;
            const float h = 0.8506508;
            vec3 spots[12] = vec3[12](
                vec3(-g, h, 0.0), vec3(g, h, 0.0), vec3(-g, -h, 0.0), vec3(g, -h, 0.0),
                vec3(0.0, -g, h), vec3(0.0, g, h), vec3(0.0, -g, -h), vec3(0.0, g, -h),
                vec3(h, 0.0, -g), vec3(h, 0.0, g), vec3(-h, 0.0, -g), vec3(-h, 0.0, g));
            float spot = 0.0;
            for (int i = 0; i < 12; ++i)
            {
                spot = max(spot, spotMask(localDirection, spots[i], cos(radians(6.5)), cos(radians(7.5))));
            }
            cue = mix(cue, vec3(0.80, 0.06, 0.05), spot);
        }
        return cue;
    }

    vec3 color = baseColor;

    if (uBallVisualType == BALL_VISUAL_STRIPE)
    {
        float stripeMask = 1.0 - smoothstep(0.22, 0.38, abs(localDirection.y));
        color = mix(ivory, baseColor, stripeMask);
    }

    float patchFront = spotMask(
        localDirection,
        normalize(vec3(0.0, 0.0, 1.0)),
        cos(radians(18.0)),
        cos(radians(22.0))
    );
    float patchBack = spotMask(
        localDirection,
        normalize(vec3(0.0, 0.0, -1.0)),
        cos(radians(18.0)),
        cos(radians(22.0))
    );
    float patchMask = max(patchFront, patchBack);

    if (uBallVisualType == BALL_VISUAL_SOLID ||
        uBallVisualType == BALL_VISUAL_STRIPE ||
        uBallVisualType == BALL_VISUAL_EIGHT)
    {
        color = mix(color, ivory, patchMask);
    }

    // The number, printed in the white spot on both sides of the ball.
    if ((uBallNumber > 0) && (patchMask > 0.0))
    {
        float spotRadius = sin(radians(20.0));
        vec2 p = localDirection.xy / spotRadius;
        if (localDirection.z < 0.0)
        {
            p.x = -p.x;
        }
        vec2 uv = p * 0.5 + 0.5;
        vec2 cell = vec2(float(uBallNumber % 4), float(uBallNumber / 4));
        float ink = texture(uNumberAtlas, (cell + vec2(uv.x, 1.0 - uv.y)) / 4.0).r;
        color = mix(color, vec3(0.03, 0.03, 0.035), ink * patchMask);
    }

    float polish = 0.5 + 0.5 * sin(atan(localDirection.z, localDirection.x) * 2.0 + localDirection.y * 6.0);
    color *= 0.98 + 0.02 * polish;

    return color;
}

vec3 sampleAnalyticEnvironment(vec3 direction)
{
    float up = saturate(direction.y * 0.5 + 0.5);

    vec3 floorColor = vec3(0.11, 0.06, 0.03);
    vec3 horizonColor = vec3(0.20, 0.12, 0.08);
    vec3 ceilingColor = vec3(0.08, 0.09, 0.10);

    vec3 environment = mix(floorColor, ceilingColor, up);
    environment = mix(environment, horizonColor, 1.0 - abs(direction.y));

    environment +=
        0.85 * vec3(1.00, 0.77, 0.44) * pow(max(direction.y, 0.0), 24.0);
    environment +=
        0.12 * vec3(0.35, 0.40, 0.46) *
        pow(max(dot(direction, normalize(vec3(-0.92, 0.12, -0.18))), 0.0), 6.0);

    return environment;
}

// ---- Physically based shading ------------------------------------------------

float distributionGgx(float nDotH, float alpha)
{
    float a2 = alpha * alpha;
    float d = nDotH * nDotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

// Height-correlated Smith visibility (includes the 1 / (4 NoL NoV) term).
float visibilitySmith(float nDotV, float nDotL, float alpha)
{
    float a2 = alpha * alpha;
    float gv = nDotL * sqrt(nDotV * nDotV * (1.0 - a2) + a2);
    float gl = nDotV * sqrt(nDotL * nDotL * (1.0 - a2) + a2);
    return 0.5 / max(gv + gl, 1.0e-5);
}

// Cloth sheen (Estevez & Kulla "Charlie" distribution, Neubelt visibility).
float distributionCharlie(float nDotH, float alpha)
{
    float inverse = 1.0 / max(alpha, 1.0e-3);
    float sin2 = max(1.0 - nDotH * nDotH, 0.0078125);
    return (2.0 + inverse) * pow(sin2, inverse * 0.5) / (2.0 * PI);
}

float visibilityNeubelt(float nDotV, float nDotL)
{
    return 1.0 / (4.0 * (nDotL + nDotV - nDotL * nDotV));
}

// Split-sum environment response without a lookup table (Karis, mobile).
vec3 environmentBrdf(vec3 f0, float roughness, float nDotV)
{
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * nDotV)) * r.x + r.y;
    vec2 ab = vec2(-1.04, 1.04) * a004 + r.zw;
    return f0 * ab.x + ab.y;
}

float shadowVisibility(int light, vec3 normal)
{
    if (uShadowsEnabled == 0)
    {
        return 1.0;
    }

    // Offset along the normal against acne, then project into the lamp's view.
    vec4 lightSpace = uLightMatrices[light] * vec4(vWorldPosition + normal * 0.004, 1.0);
    vec3 p = lightSpace.xyz / lightSpace.w * 0.5 + 0.5;
    if (any(lessThan(p.xy, vec2(0.0))) || any(greaterThan(p.xy, vec2(1.0))) || (p.z > 1.0))
    {
        return 1.0;
    }

    float depth = p.z - 0.0004;
    int halfKernel = uShadowKernel / 2;
    float sum = 0.0;
    for (int y = -halfKernel; y <= halfKernel; ++y)
    {
        for (int x = -halfKernel; x <= halfKernel; ++x)
        {
            vec2 offset = vec2(float(x), float(y)) * uShadowTexel * 1.5;
            sum += texture(uShadowMaps, vec4(p.xy + offset, float(light), depth));
        }
    }
    return sum / float(uShadowKernel * uShadowKernel);
}

// The nine SH coefficients (per colour) interpolated at a point.
void probeCoefficients(vec3 position, out vec3 c[9])
{
    vec3 t = clamp((position - uProbeMin) / max(uProbeMax - uProbeMin, vec3(1.0e-3)), 0.0, 1.0);
    vec3 uvw = (t * (uProbeCounts - 1.0) + 0.5) / uProbeCounts;   // probe centres are texel centres
    vec4 p0 = texture(uProbeSH0, uvw);
    vec4 p1 = texture(uProbeSH1, uvw);
    vec4 p2 = texture(uProbeSH2, uvw);
    vec4 p3 = texture(uProbeSH3, uvw);
    vec4 p4 = texture(uProbeSH4, uvw);
    vec4 p5 = texture(uProbeSH5, uvw);
    vec4 p6 = texture(uProbeSH6, uvw);
    c[0] = p0.rgb;
    c[1] = vec3(p0.a, p1.rg);
    c[2] = vec3(p1.ba, p2.r);
    c[3] = p2.gba;
    c[4] = p3.rgb;
    c[5] = vec3(p3.a, p4.rg);
    c[6] = vec3(p4.ba, p5.r);
    c[7] = p5.gba;
    c[8] = p6.rgb;
}

// Sum of c_i * w_i * Y_i(n); with w the clamped-cosine bands this is
// irradiance (Ramamoorthi & Hanrahan 2001), with w = 1 radiance.
vec3 evaluateSh(vec3 c[9], vec3 n, vec3 band)
{
    vec3 result = c[0] * (0.282095 * band.x);
    result += (c[1] * n.y + c[2] * n.z + c[3] * n.x) * (0.488603 * band.y);
    result += (c[4] * (n.x * n.y) * 1.092548 + c[5] * (n.y * n.z) * 1.092548 +
               c[6] * (3.0 * n.z * n.z - 1.0) * 0.315392 + c[7] * (n.x * n.z) * 1.092548 +
               c[8] * (n.x * n.x - n.y * n.y) * 0.546274) * band.z;
    return max(result, vec3(0.0));
}

// Where the reflected ray from this point leaves the room's box, as seen
// from the capture point (Lagarde & Zanuttini 2012, "local cube maps").
vec3 boxProjected(vec3 position, vec3 direction)
{
    vec3 safe = direction + vec3(equal(direction, vec3(0.0))) * 1.0e-5;
    vec3 far = max((uRoomMax - position) / safe, (uRoomMin - position) / safe);
    float t = min(min(far.x, far.y), far.z);
    return position + direction * t - uReflectionCentre;
}

vec3 roomReflection(vec3 direction, float roughness)
{
    vec3 position = clamp(vWorldPosition, uRoomMin + 0.01, uRoomMax - 0.01);
    // Rough lobes are wide: blend back towards the plain direction, which
    // hides the box's corners in blurry reflections.
    vec3 lookup = mix(boxProjected(position, direction), direction, roughness * roughness);
    return textureLod(uReflection, lookup, roughness * uReflectionMaxLod).rgb;
}

struct Surface
{
    vec3 baseColor;
    float roughness;
    vec3 f0;
    float clearcoat;
    bool cloth;
};

vec3 shadeLight(Surface surface, vec3 radiance, vec3 lightDirection, float lightDistance, vec3 normal, vec3 viewDirection)
{
    float nDotL = dot(normal, lightDirection);
    if (nDotL <= 0.0)
    {
        return vec3(0.0);
    }

    vec3 halfVector = normalize(lightDirection + viewDirection);
    float nDotV = max(dot(normal, viewDirection), 1.0e-4);
    float nDotH = saturate(dot(normal, halfVector));
    float lDotH = saturate(dot(lightDirection, halfVector));

    // A lamp is not a point: widen the lobe by its angular size (Karis 2013).
    float alpha = surface.roughness * surface.roughness;
    float lightAlpha = saturate(alpha + uLightRadius / (2.0 * max(lightDistance, 0.1)));

    vec3 fresnel = fresnelSchlick(lDotH, surface.f0);
    vec3 specular = distributionGgx(nDotH, lightAlpha) * visibilitySmith(nDotV, nDotL, alpha) * fresnel;
    specular *= (alpha / lightAlpha) * (alpha / lightAlpha);   // keep the energy of the widened lobe
    vec3 diffuse = (1.0 - fresnel) * surface.baseColor / PI;

    if (surface.cloth)
    {
        // Velvet-like sheen at grazing angles; the baize looks soft, not plastic.
        vec3 sheenColor = mix(surface.baseColor, vec3(1.0), 0.15) * 0.3;
        specular = sheenColor * distributionCharlie(nDotH, 0.65) * visibilityNeubelt(nDotV, nDotL);
        diffuse = surface.baseColor / PI;
    }

    if (uCapture == 1)
    {
        // Probes integrate light over all directions: view-dependent
        // highlights would only add noise.
        specular = vec3(0.0);
    }
    vec3 color = diffuse + specular;

    if (surface.clearcoat > 0.0)
    {
        // A thin glossy lacquer over the base: resin balls, varnished wood.
        float coatFresnel = fresnelSchlick(lDotH, vec3(0.04)).x * surface.clearcoat;
        float coatAlpha = saturate(0.0025 + uLightRadius / (2.0 * max(lightDistance, 0.1)));
        float coat = distributionGgx(nDotH, coatAlpha) * visibilitySmith(nDotV, nDotL, 0.05) * coatFresnel;
        coat *= (0.0025 / coatAlpha) * (0.0025 / coatAlpha);
        color = color * (1.0 - coatFresnel) + coat;
    }

    return color * radiance * nDotL;
}

void main()
{
    vec3 normal = normalize(vWorldNormal);
    vec3 viewDirection = normalize(uViewPosition - vWorldPosition);
    float nDotV = max(dot(normal, viewDirection), 1.0e-4);

    // Material colours are authored in sRGB; light them in linear space.
    vec3 baseColor = pow(max(uMaterialAlbedo, vec3(0.0)), vec3(2.2));
    if (uMaterialSurfaceType == SURFACE_CLOTH)
    {
        baseColor = applyClothFinish(baseColor);
    }
    else if (uMaterialSurfaceType == SURFACE_WOOD)
    {
        baseColor = applyWoodFinish(baseColor);
    }
    else if (uMaterialSurfaceType == SURFACE_BALL)
    {
        baseColor = applyBallFinish(baseColor);
    }

    Surface surface;
    surface.baseColor = baseColor;
    surface.roughness = clamp(uMaterialRoughness, 0.04, 1.0);
    surface.f0 = vec3(clamp(uMaterialReflectivity, 0.02, 0.9));
    surface.clearcoat = clamp(uMaterialClearcoatStrength, 0.0, 1.0);
    surface.cloth = uMaterialSurfaceType == SURFACE_CLOTH;
    if (uMaterialSurfaceType == SURFACE_BALL)
    {
        // Phenolic resin is one hard, polished dielectric: a small sharp
        // highlight over saturated colour (a separate coat would double it).
        surface.roughness = 0.06;
        surface.f0 = vec3(0.045);
        surface.clearcoat = 0.0;
    }

    vec3 lighting = vec3(0.0);
    for (int i = 0; i < 3; ++i)
    {
        if (i >= uActivePointLightCount)
        {
            break;
        }

        vec3 lightVector = uPointLightPositions[i] - vWorldPosition;
        float distanceToLight = length(lightVector);
        vec3 lightDirection = lightVector / max(distanceToLight, 1.0e-4);
        vec3 radiance = uPointLightColors[i] * uLightIntensity / (distanceToLight * distanceToLight + 0.02);
        radiance *= shadowVisibility(i, normal);

        lighting += shadeLight(surface, radiance, lightDirection, distanceToLight, normal, viewDirection);
    }

    for (int i = 0; i < LIGHTS_PER_DRAW; ++i)
    {
        if (i >= uLocalCount)
        {
            break;
        }
        int light = uLocalIndices[i];
        vec3 lightVector = uLocalPositionRange[light].xyz - vWorldPosition;
        float distanceToLight = length(lightVector);
        vec3 lightDirection = lightVector / max(distanceToLight, 1.0e-4);

        // Inverse square, windowed to reach zero at the range (Karis 2013).
        float ratio = distanceToLight / uLocalPositionRange[light].w;
        float window = saturate(1.0 - ratio * ratio * ratio * ratio);
        float falloff = window * window / (distanceToLight * distanceToLight + 0.02);

        vec4 spot = uLocalSpot[light];
        if (spot.w > -1.0)
        {
            falloff *= smoothstep(spot.w, uLocalColor[light].w, dot(-lightDirection, spot.xyz));
        }
        if (falloff > 0.0)
        {
            lighting += shadeLight(surface, uLocalColor[light].rgb * falloff, lightDirection, distanceToLight, normal, viewDirection);
        }
    }

    vec3 reflectionDirection = reflect(-viewDirection, normal);
    vec3 ambient;
    vec3 environment;
    if (uProbesEnabled == 1)
    {
        // Indirect light from the probes: irradiance for diffuse, and the
        // (blurry) radiance in the mirror direction for reflections. A short
        // step along the normal keeps surfaces from sampling behind themselves.
        vec3 c[9];
        probeCoefficients(vWorldPosition + normal * 0.08, c);
        vec3 irradiance = evaluateSh(c, normal, vec3(PI, 2.0 * PI / 3.0, PI / 4.0));
        ambient = baseColor / PI * irradiance * uAmbientScale;
        environment = (uReflectionEnabled == 1)
            ? roomReflection(reflectionDirection, surface.roughness)
            : evaluateSh(c, reflectionDirection, vec3(1.0)) * uAmbientScale;
    }
    else
    {
        // No probes yet: a warm floor bounce and a little sky from the ceiling.
        ambient = mix(vec3(0.030, 0.022, 0.016), vec3(0.075, 0.064, 0.055), saturate(normal.y * 0.5 + 0.5));
        ambient *= baseColor * uAmbientScale;
        environment = sampleAnalyticEnvironment(reflectionDirection);
        environment = mix(environment, vec3(0.06, 0.045, 0.035), surface.roughness * 0.8);
    }
    vec3 reflections = environment * environmentBrdf(surface.f0, surface.roughness, nDotV);
    if (surface.clearcoat > 0.0)
    {
        vec3 coatEnvironment = (uReflectionEnabled == 1) ? roomReflection(reflectionDirection, 0.05) : environment;
        reflections += 0.5 * coatEnvironment * fresnelSchlick(nDotV, vec3(0.04)) * surface.clearcoat;
    }
    if (surface.cloth)
    {
        reflections *= 0.25;
    }

    if (uCapture == 1)
    {
        reflections = vec3(0.0);
    }
    vec3 color = ambient + lighting + reflections * uReflectionScale + uEmissionColor * uEmissionScale;

    // Linear HDR out; the post pass tone maps and encodes for the display.
    FragColor = vec4(color, uAlpha);
}
