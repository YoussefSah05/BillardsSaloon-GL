#version 410 core

// Prefilters the room's reflection cube map for one roughness: each texel is
// the GGX-weighted average of the radiance around its direction (the first
// factor of the split-sum approximation, with n = v = r; Karis 2013).
// Samples read a blurrier source mip where their pdf is low ("filtered
// importance sampling", Krivanek & Colbert 2008), so 64 samples are enough.

in vec2 vUv;
out vec4 FragColor;

uniform samplerCube uSource;
uniform int uFace;
uniform float uRoughness;
uniform float uSourceSize;   // texels along a face of mip 0

const float PI = 3.14159265359;
const int SAMPLES = 64;

vec3 faceDirection(int face, vec2 uv)
{
    vec2 p = uv * 2.0 - 1.0;   // u, v in [-1, 1], as in scene/spherical_harmonics.cpp
    if (face == 0) return normalize(vec3(1.0, -p.y, -p.x));
    if (face == 1) return normalize(vec3(-1.0, -p.y, p.x));
    if (face == 2) return normalize(vec3(p.x, 1.0, p.y));
    if (face == 3) return normalize(vec3(p.x, -1.0, -p.y));
    if (face == 4) return normalize(vec3(p.x, -p.y, 1.0));
    return normalize(vec3(-p.x, -p.y, -1.0));
}

// Low-discrepancy points (Hammersley).
vec2 hammersley(int i)
{
    uint bits = uint(i);
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return vec2(float(i) / float(SAMPLES), float(bits) * 2.3283064365386963e-10);
}

void main()
{
    vec3 n = faceDirection(uFace, vUv);
    float alpha = uRoughness * uRoughness;

    vec3 up = abs(n.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, n));
    vec3 bitangent = cross(n, tangent);

    vec3 sum = vec3(0.0);
    float weight = 0.0;
    for (int i = 0; i < SAMPLES; ++i)
    {
        // Sample a half vector from the GGX distribution, reflect about it.
        vec2 xi = hammersley(i);
        float phi = 2.0 * PI * xi.x;
        float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (alpha * alpha - 1.0) * xi.y));
        float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
        vec3 h = normalize(tangent * (sinTheta * cos(phi)) + bitangent * (sinTheta * sin(phi)) + n * cosTheta);
        vec3 l = reflect(-n, h);
        float nDotL = dot(n, l);
        if (nDotL <= 0.0)
        {
            continue;
        }

        // pdf of l is D(h) (n.h) / (4 v.h) = D / 4 with n = v; compare the
        // sample's solid angle with a source texel's to choose the mip.
        float nDotH = max(dot(n, h), 0.0);
        float d = alpha * alpha / (PI * pow(nDotH * nDotH * (alpha * alpha - 1.0) + 1.0, 2.0));
        float pdf = d / 4.0 + 1.0e-4;
        float sampleAngle = 1.0 / (float(SAMPLES) * pdf);
        float texelAngle = 4.0 * PI / (6.0 * uSourceSize * uSourceSize);
        float lod = (uRoughness == 0.0) ? 0.0 : max(0.5 * log2(sampleAngle / texelAngle) + 1.0, 0.0);

        sum += textureLod(uSource, l, lod).rgb * nDotL;
        weight += nDotL;
    }
    FragColor = vec4(sum / max(weight, 1.0e-4), 1.0);
}
