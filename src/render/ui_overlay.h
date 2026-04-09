#pragma once

#include "scene/components.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>

namespace BilliardsSaloon
{
    class Mesh;
    class Shader;

    enum class UiTextAlignment
    {
        Left,
        Center,
        Right
    };

    struct UiOverlayFrame
    {
        glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 right {1.0f, 0.0f, 0.0f};
        glm::vec3 up {0.0f, 1.0f, 0.0f};
    };

    struct UiTextStyle
    {
        MaterialComponent material {};
        glm::vec3 dynamicEmission {0.0f, 0.0f, 0.0f};
        float cellSize {0.01f};
        float depth {0.01f};
        UiTextAlignment alignment {UiTextAlignment::Center};
    };

    [[nodiscard]] float measureUiOverlayTextWidth(const std::string& text, float cellSize);

    void renderUiOverlayBox(
        Shader& shader,
        Mesh& cubeMesh,
        const UiOverlayFrame& frame,
        const glm::vec3& position,
        const glm::vec3& scale,
        const MaterialComponent& material,
        const glm::vec3& dynamicEmission);

    void renderUiOverlayText(
        Shader& shader,
        Mesh& cubeMesh,
        const UiOverlayFrame& frame,
        const std::string& text,
        const glm::vec3& anchorPosition,
        const UiTextStyle& style);
}
