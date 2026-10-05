#version 410 core

// 3x3 tent upsample, added onto the next larger level.
in vec2 vUv;

uniform sampler2D uSource;
uniform vec2 uSourceTexel;
uniform float uRadius;

out vec4 FragColor;

void main()
{
    vec2 t = uSourceTexel * uRadius;
    vec3 sum = texture(uSource, vUv).rgb * 4.0;
    sum += (texture(uSource, vUv + vec2(-t.x, 0.0)).rgb + texture(uSource, vUv + vec2(t.x, 0.0)).rgb +
            texture(uSource, vUv + vec2(0.0, -t.y)).rgb + texture(uSource, vUv + vec2(0.0, t.y)).rgb) * 2.0;
    sum += texture(uSource, vUv + vec2(-t.x, -t.y)).rgb + texture(uSource, vUv + vec2(t.x, -t.y)).rgb +
           texture(uSource, vUv + vec2(-t.x, t.y)).rgb + texture(uSource, vUv + vec2(t.x, t.y)).rgb;
    FragColor = vec4(sum / 16.0, 1.0);
}
