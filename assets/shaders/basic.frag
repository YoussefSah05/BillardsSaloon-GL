#version 410 core

in vec3 vWorldPosition;
in vec3 vWorldNormal;

uniform vec3 uViewPosition;

uniform vec3 uDirectionalLightDirection;
uniform vec3 uDirectionalLightColor;

uniform vec3 uPointLightPosition;
uniform vec3 uPointLightColor;

uniform vec3 uMaterialAlbedo;
uniform float uMaterialSpecularStrength;
uniform float uMaterialShininess;

uniform int uUseEmission;
uniform vec3 uEmissionColor;

out vec4 FragColor;

vec3 evaluateDirectionalLight(vec3 N, vec3 V)
{
    vec3 L = normalize(-uDirectionalLightDirection);
    vec3 H = normalize(L + V);

    float diffuseTerm = max(dot(N, L), 0.0);
    float specularTerm = 0.0;

    if (diffuseTerm > 0.0)
    {
        specularTerm = pow(max(dot(N, H), 0.0), uMaterialShininess) * uMaterialSpecularStrength;
    }

    vec3 diffuse = diffuseTerm * uMaterialAlbedo * uDirectionalLightColor;
    vec3 specular = specularTerm * uDirectionalLightColor;

    return diffuse + specular;
}

vec3 evaluatePointLight(vec3 N, vec3 V)
{
    vec3 lightVector = uPointLightPosition - vWorldPosition;
    float distanceToLight = length(lightVector);
    vec3 L = normalize(lightVector);
    vec3 H = normalize(L + V);

    float attenuation = 1.0 / (1.0 + 0.35 * distanceToLight + 0.20 * distanceToLight * distanceToLight);

    float diffuseTerm = max(dot(N, L), 0.0);
    float specularTerm = 0.0;

    if (diffuseTerm > 0.0)
    {
        specularTerm = pow(max(dot(N, H), 0.0), uMaterialShininess) * uMaterialSpecularStrength;
    }

    vec3 diffuse = diffuseTerm * uMaterialAlbedo * uPointLightColor;
    vec3 specular = specularTerm * uPointLightColor;

    return attenuation * (diffuse + specular);
}

void main()
{
    vec3 N = normalize(vWorldNormal);
    vec3 V = normalize(uViewPosition - vWorldPosition);

    vec3 ambient = 0.12 * uMaterialAlbedo;
    vec3 lighting =
        ambient +
        evaluateDirectionalLight(N, V) +
        evaluatePointLight(N, V);

    if (uUseEmission != 0)
    {
        lighting += uEmissionColor;
    }

    FragColor = vec4(lighting, 1.0);
}