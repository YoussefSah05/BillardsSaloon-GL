#include "scene/spherical_harmonics.h"

#include <cmath>

namespace Septhuit
{
    namespace
    {
        constexpr float PI = 3.14159265358979323846f;
    }

    std::array<float, 9> shBasis(const glm::vec3& n)
    {
        return {
            0.282095f,
            0.488603f * n.y,
            0.488603f * n.z,
            0.488603f * n.x,
            1.092548f * n.x * n.y,
            1.092548f * n.y * n.z,
            0.315392f * (3.0f * n.z * n.z - 1.0f),
            1.092548f * n.x * n.z,
            0.546274f * (n.x * n.x - n.y * n.y)};
    }

    glm::vec3 cubeDirection(int face, float u, float v)
    {
        // The OpenGL specification's table of major axis and (sc, tc), with
        // s = (sc / |ma| + 1) / 2; here u = sc / |ma| and v = tc / |ma|.
        glm::vec3 d(0.0f);
        switch (face)
        {
            case 0: d = glm::vec3(1.0f, -v, -u); break;    // +X
            case 1: d = glm::vec3(-1.0f, -v, u); break;    // -X
            case 2: d = glm::vec3(u, 1.0f, v); break;      // +Y
            case 3: d = glm::vec3(u, -1.0f, -v); break;    // -Y
            case 4: d = glm::vec3(u, -v, 1.0f); break;     // +Z
            default: d = glm::vec3(-u, -v, -1.0f); break;  // -Z
        }
        return glm::normalize(d);
    }

    ShRgb9 projectCube(const std::array<CubeFace, 6>& faces, int size)
    {
        ShRgb9 sh;
        float totalWeight = 0.0f;
        const float texel = 2.0f / static_cast<float>(size);
        for (int face = 0; face < 6; ++face)
        {
            const std::vector<glm::vec3>& texels = faces[static_cast<std::size_t>(face)].texels;
            for (int y = 0; y < size; ++y)
            {
                for (int x = 0; x < size; ++x)
                {
                    const float u = -1.0f + (static_cast<float>(x) + 0.5f) * texel;
                    const float v = -1.0f + (static_cast<float>(y) + 0.5f) * texel;
                    // Solid angle of a texel on the unit cube face: dA cos / r^2.
                    const float r2 = 1.0f + u * u + v * v;
                    const float weight = texel * texel / (r2 * std::sqrt(r2));
                    const std::array<float, 9> basis = shBasis(cubeDirection(face, u, v));
                    const glm::vec3& radiance = texels[static_cast<std::size_t>(y * size + x)];
                    for (std::size_t i = 0; i < 9; ++i)
                    {
                        sh.coefficients[i] += radiance * (basis[i] * weight);
                    }
                    totalWeight += weight;
                }
            }
        }
        // The weights sum to 4 pi up to discretisation; normalise exactly.
        const float scale = 4.0f * PI / totalWeight;
        for (glm::vec3& c : sh.coefficients)
        {
            c *= scale;
        }
        return sh;
    }

    glm::vec3 shIrradiance(const ShRgb9& radiance, const glm::vec3& n)
    {
        constexpr float A0 = PI;
        constexpr float A1 = 2.0f * PI / 3.0f;
        constexpr float A2 = PI / 4.0f;
        constexpr std::array<float, 9> band = {A0, A1, A1, A1, A2, A2, A2, A2, A2};
        const std::array<float, 9> basis = shBasis(n);
        glm::vec3 e(0.0f);
        for (std::size_t i = 0; i < 9; ++i)
        {
            e += radiance.coefficients[i] * (band[i] * basis[i]);
        }
        return glm::max(e, glm::vec3(0.0f));
    }
}
