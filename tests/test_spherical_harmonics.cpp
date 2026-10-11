#include "scene/spherical_harmonics.h"

#include <doctest/doctest.h>

#include <functional>

using namespace Septhuit;

namespace
{
    constexpr float PI = 3.14159265358979323846f;
    constexpr int SIZE = 32;

    std::array<CubeFace, 6> cubeOf(const std::function<glm::vec3(const glm::vec3&)>& radiance)
    {
        std::array<CubeFace, 6> faces;
        const float texel = 2.0f / SIZE;
        for (int f = 0; f < 6; ++f)
        {
            for (int y = 0; y < SIZE; ++y)
            {
                for (int x = 0; x < SIZE; ++x)
                {
                    const float u = -1.0f + (static_cast<float>(x) + 0.5f) * texel;
                    const float v = -1.0f + (static_cast<float>(y) + 0.5f) * texel;
                    faces[static_cast<std::size_t>(f)].texels.push_back(radiance(cubeDirection(f, u, v)));
                }
            }
        }
        return faces;
    }
}

TEST_CASE("cube faces follow the OpenGL conventions")
{
    CHECK(cubeDirection(0, 0.0f, 0.0f).x == doctest::Approx(1.0f));
    CHECK(cubeDirection(1, 0.0f, 0.0f).x == doctest::Approx(-1.0f));
    CHECK(cubeDirection(2, 0.0f, 0.0f).y == doctest::Approx(1.0f));
    CHECK(cubeDirection(3, 0.0f, 0.0f).y == doctest::Approx(-1.0f));
    CHECK(cubeDirection(4, 0.0f, 0.0f).z == doctest::Approx(1.0f));
    CHECK(cubeDirection(5, 0.0f, 0.0f).z == doctest::Approx(-1.0f));
    // +X: s grows towards -z, t grows towards -y.
    CHECK(cubeDirection(0, 1.0f, 0.0f).z < 0.0f);
    CHECK(cubeDirection(0, 0.0f, 1.0f).y < 0.0f);
}

TEST_CASE("a uniform environment gives irradiance pi L in every direction")
{
    const ShRgb9 sh = projectCube(cubeOf([](const glm::vec3&) { return glm::vec3(1.0f, 0.5f, 0.25f); }), SIZE);
    for (const glm::vec3& n : {glm::vec3(1, 0, 0), glm::vec3(0, -1, 0), glm::vec3(0.6f, 0.0f, 0.8f)})
    {
        const glm::vec3 e = shIrradiance(sh, n);
        CHECK(e.r == doctest::Approx(PI).epsilon(0.005));
        CHECK(e.g == doctest::Approx(0.5f * PI).epsilon(0.005));
        CHECK(e.b == doctest::Approx(0.25f * PI).epsilon(0.005));
    }
}

TEST_CASE("light from the upper hemisphere lights floors, not ceilings")
{
    const ShRgb9 sh = projectCube(cubeOf([](const glm::vec3& d) { return glm::vec3(d.y > 0.0f ? 1.0f : 0.0f); }), SIZE);
    // Exact: pi facing up, pi / 2 sideways, 0 facing down. Bands 0-2 are
    // within a few percent of pi (Ramamoorthi & Hanrahan's 3% average error).
    CHECK(shIrradiance(sh, glm::vec3(0, 1, 0)).r == doctest::Approx(PI).epsilon(0.05));
    CHECK(shIrradiance(sh, glm::vec3(1, 0, 0)).r == doctest::Approx(0.5f * PI).epsilon(0.02));
    CHECK(shIrradiance(sh, glm::vec3(0, -1, 0)).r < 0.05f * PI);
}
