#version 410 core

in vec3 vWorldPosition;
in vec3 vWorldNormal;

uniform vec3 uLightDirection;
uniform vec3 uAlbedo;

out vec4 FragColor;

void main()
{
    vec3 N = normalize(vWorldNormal);
    vec3 L = normalize(-uLightDirection);

    float ndotl = max(dot(N, L), 0.0);
    vec3 ambient = 0.18 * uAlbedo;
    vec3 diffuse = ndotl * uAlbedo;

    vec3 color = ambient + diffuse;
    FragColor = vec4(color, 1.0);
}