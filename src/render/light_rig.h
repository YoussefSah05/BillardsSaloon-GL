#pragma once

#include <glm/glm.hpp>

#include <array>

namespace BilliardsSaloon
{
    // Must match the uPointLight* array sizes in basic.frag.
    constexpr int POINT_LIGHT_COUNT = 3;

    struct PointLightRig
    {
        std::array<glm::vec3, POINT_LIGHT_COUNT> positions {};
        std::array<glm::vec3, POINT_LIGHT_COUNT> colors {};
    };
}
