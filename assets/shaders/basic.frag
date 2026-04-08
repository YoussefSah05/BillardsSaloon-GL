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

out vec4 FragColor;

const int SURFACE_CLOTH = 1;
const int SURFACE_BALL = 2;
const int SURFACE_WOOD = 3;
const int SURFACE_LAMP = 4;

const int BALL_VISUAL_CUE = 1;
const int BALL_VISUAL_SOLID = 2;
const int BALL_VISUAL_STRIPE = 3;
const int BALL_VISUAL_EIGHT = 4;

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
        return mix(ivory * 0.97, ivory * 1.03, 0.30 * chalkBloom);
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

vec3 evaluateLight(
    vec3 radiance,
    vec3 lightDirection,
    vec3 normal,
    vec3 viewDirection,
    vec3 baseColor,
    float roughness,
    vec3 fresnelBase)
{
    vec3 halfVector = normalize(lightDirection + viewDirection);
    float nDotL = max(dot(normal, lightDirection), 0.0);

    if (nDotL <= 0.0)
    {
        return vec3(0.0);
    }

    float hDotV = max(dot(halfVector, viewDirection), 0.0);
    float nDotH = max(dot(normal, halfVector), 0.0);
    float legacySpecularPower = clamp(uMaterialShininess, 4.0, 256.0);
    float roughnessDrivenPower = mix(220.0, 18.0, roughness);
    float specularPower = mix(roughnessDrivenPower, legacySpecularPower, 0.35);
    float specularTerm = pow(nDotH, specularPower) * uMaterialSpecularStrength;
    vec3 fresnel = fresnelSchlick(hDotV, fresnelBase);

    vec3 diffuse = baseColor * nDotL;
    vec3 specular = specularTerm * fresnel;

    return radiance * (diffuse + specular);
}

vec3 acesTonemap(vec3 color)
{
    vec3 a = color * (2.51 * color + 0.03);
    vec3 b = color * (2.43 * color + 0.59) + 0.14;
    return clamp(a / b, 0.0, 1.0);
}

void main()
{
    vec3 normal = normalize(vWorldNormal);
    vec3 viewDirection = normalize(uViewPosition - vWorldPosition);

    vec3 baseColor = uMaterialAlbedo;
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

    float legacyGlossRoughness = sqrt(2.0 / max(uMaterialShininess + 2.0, 2.0));
    float roughness = clamp(mix(uMaterialRoughness, legacyGlossRoughness, 0.30), 0.05, 1.0);
    vec3 fresnelBase = vec3(clamp(uMaterialReflectivity, 0.02, 0.9));

    if (uMaterialSurfaceType == SURFACE_CLOTH)
    {
        roughness = clamp(roughness + 0.16, 0.16, 1.0);
    }
    else if (uMaterialSurfaceType == SURFACE_BALL)
    {
        roughness = clamp(roughness, 0.05, 0.22);
        fresnelBase = max(fresnelBase, vec3(0.07));
    }

    vec3 lighting = vec3(0.0);

    vec3 directionalDirection = normalize(-uDirectionalLightDirection);
    lighting += evaluateLight(
        uDirectionalLightColor,
        directionalDirection,
        normal,
        viewDirection,
        baseColor,
        roughness,
        fresnelBase
    );

    for (int i = 0; i < 3; ++i)
    {
        if (i >= uActivePointLightCount)
        {
            break;
        }

        vec3 lightVector = uPointLightPositions[i] - vWorldPosition;
        float distanceToLight = length(lightVector);
        vec3 lightDirection = lightVector / max(distanceToLight, 1.0e-4);

        float attenuation =
            1.0 / (1.0 + 0.22 * distanceToLight + 0.12 * distanceToLight * distanceToLight);
        vec3 radiance = uPointLightColors[i] * attenuation;

        lighting += evaluateLight(
            radiance,
            lightDirection,
            normal,
            viewDirection,
            baseColor,
            roughness,
            fresnelBase
        );
    }

    vec3 ambient = mix(vec3(0.05, 0.04, 0.03), vec3(0.14, 0.12, 0.10), saturate(normal.y * 0.5 + 0.5));
    ambient *= baseColor;

    vec3 reflectionDirection = reflect(-viewDirection, normal);
    vec3 environment = sampleAnalyticEnvironment(reflectionDirection);
    vec3 reflectionFresnel =
        fresnelSchlick(max(dot(normal, viewDirection), 0.0), fresnelBase + vec3(0.04 * uMaterialClearcoatStrength));

    float reflectionStrength = (1.0 - roughness * 0.68) * uMaterialSpecularStrength;
    if (uMaterialSurfaceType == SURFACE_CLOTH)
    {
        reflectionStrength *= 0.22;
    }
    else if (uMaterialSurfaceType == SURFACE_WOOD)
    {
        reflectionStrength *= 0.60;
    }
    else if (uMaterialSurfaceType == SURFACE_LAMP)
    {
        reflectionStrength *= 1.05;
    }

    vec3 reflections = environment * reflectionFresnel * reflectionStrength;

    if (uMaterialSurfaceType == SURFACE_BALL)
    {
        float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 4.0);
        reflections += environment * rim * 0.06;
    }

    vec3 color = ambient + lighting + reflections * uReflectionScale + uEmissionColor * uEmissionScale;

    color = acesTonemap(color);
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
