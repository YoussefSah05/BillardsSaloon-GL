#version 410 core

// 13-tap downsample (Jimenez, "Next Generation Post Processing in Call of
// Duty: Advanced Warfare", 2014). The first pass weights by luminance (Karis
// average) so a single bright pixel cannot flicker into a bloom blob.
in vec2 vUv;

uniform sampler2D uSource;
uniform vec2 uSourceTexel;
uniform int uFirstPass;

out vec4 FragColor;

float karisWeight(vec3 c)
{
    return 1.0 / (1.0 + dot(c, vec3(0.2126, 0.7152, 0.0722)));
}

void main()
{
    vec2 t = uSourceTexel;
    vec3 a = texture(uSource, vUv + t * vec2(-2.0,  2.0)).rgb;
    vec3 b = texture(uSource, vUv + t * vec2( 0.0,  2.0)).rgb;
    vec3 c = texture(uSource, vUv + t * vec2( 2.0,  2.0)).rgb;
    vec3 d = texture(uSource, vUv + t * vec2(-2.0,  0.0)).rgb;
    vec3 e = texture(uSource, vUv).rgb;
    vec3 f = texture(uSource, vUv + t * vec2( 2.0,  0.0)).rgb;
    vec3 g = texture(uSource, vUv + t * vec2(-2.0, -2.0)).rgb;
    vec3 h = texture(uSource, vUv + t * vec2( 0.0, -2.0)).rgb;
    vec3 i = texture(uSource, vUv + t * vec2( 2.0, -2.0)).rgb;
    vec3 j = texture(uSource, vUv + t * vec2(-1.0,  1.0)).rgb;
    vec3 k = texture(uSource, vUv + t * vec2( 1.0,  1.0)).rgb;
    vec3 l = texture(uSource, vUv + t * vec2(-1.0, -1.0)).rgb;
    vec3 m = texture(uSource, vUv + t * vec2( 1.0, -1.0)).rgb;

    vec3 color;
    if (uFirstPass == 1)
    {
        vec3 g0 = (a + b + d + e) * 0.25;
        vec3 g1 = (b + c + e + f) * 0.25;
        vec3 g2 = (d + e + g + h) * 0.25;
        vec3 g3 = (e + f + h + i) * 0.25;
        vec3 g4 = (j + k + l + m) * 0.25;
        float w0 = karisWeight(g0), w1 = karisWeight(g1), w2 = karisWeight(g2), w3 = karisWeight(g3), w4 = karisWeight(g4);
        color = (g0 * w0 + g1 * w1 + g2 * w2 + g3 * w3 + g4 * w4) / (w0 + w1 + w2 + w3 + w4);
    }
    else
    {
        color = e * 0.125;
        color += (a + c + g + i) * 0.03125;
        color += (b + d + f + h) * 0.0625;
        color += (j + k + l + m) * 0.125;
    }

    FragColor = vec4(max(color, vec3(0.0)), 1.0);
}
