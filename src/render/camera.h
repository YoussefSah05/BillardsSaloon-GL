#pragma once

#include <glm/glm.hpp>

namespace BilliardsSaloon
{
    struct CameraComponent
    {
        float verticalFieldOfViewRadians {glm::radians(60.0f)};
        float nearPlane {0.1f};
        float farPlane {100.0f};
    };

    class Camera
    {
    public:
        static glm::mat4 viewMatrix(
            const glm::vec3& position,
            const glm::vec3& forward,
            const glm::vec3& up);

        static glm::mat4 projectionMatrix(
            float verticalFieldOfViewRadians,
            float aspectRatio,
            float nearPlane,
            float farPlane);
    };
}