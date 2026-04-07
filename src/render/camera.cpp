#include "render/camera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace BilliardsSaloon
{
    glm::mat4 Camera::viewMatrix(
        const glm::vec3& position,
        const glm::vec3& forward,
        const glm::vec3& up)
    {
        return glm::lookAt(position, position + forward, up);
    }

    glm::mat4 Camera::projectionMatrix(
        float verticalFieldOfViewRadians,
        float aspectRatio,
        float nearPlane,
        float farPlane)
    {
        return glm::perspective(verticalFieldOfViewRadians, aspectRatio, nearPlane, farPlane);
    }
}