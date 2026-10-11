#pragma once

#include <glm/glm.hpp>

#include <array>
#include <cstddef>
#include <vector>

namespace Septhuit
{
    // Radiance around a point, projected onto the first nine real spherical
    // harmonics (bands 0-2), one coefficient vector per RGB channel.
    struct ShRgb9
    {
        std::array<glm::vec3, 9> coefficients {};
    };

    // The nine real SH basis functions at unit direction n (Ramamoorthi &
    // Hanrahan 2001 ordering: Y00, Y1-1, Y10, Y11, Y2-2, Y2-1, Y20, Y21, Y22).
    [[nodiscard]] std::array<float, 9> shBasis(const glm::vec3& n);

    // One face of a cube map: size x size linear RGB texels, row by row from
    // the bottom (as glReadPixels returns them).
    struct CubeFace
    {
        std::vector<glm::vec3> texels;
    };

    // OpenGL cube map conventions: for face f (+X, -X, +Y, -Y, +Z, -Z) and
    // face coordinates u, v in [-1, 1], the direction through that texel.
    [[nodiscard]] glm::vec3 cubeDirection(int face, float u, float v);

    // Projects six faces of `size` x `size` texels onto SH, weighting each
    // texel by the solid angle it covers.
    [[nodiscard]] ShRgb9 projectCube(const std::array<CubeFace, 6>& faces, int size);

    // Irradiance E(n) on a surface with normal n from the SH radiance:
    // the convolution with the clamped cosine, which only bands 0-2 survive
    // (A0 = pi, A1 = 2 pi / 3, A2 = pi / 4).
    [[nodiscard]] glm::vec3 shIrradiance(const ShRgb9& radiance, const glm::vec3& n);
}
