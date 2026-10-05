#pragma once

#include "ecs/entity.h"
#include "ecs/registry.h"
#include "render/light_rig.h"
#include "render/mesh.h"
#include "render/shader.h"
#include "scene/components.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <memory>

namespace BilliardsSaloon
{
    enum class RenderQualityPreset
    {
        Low,
        Balanced,
        High
    };

    [[nodiscard]] const char* renderQualityLabel(RenderQualityPreset quality);
    [[nodiscard]] RenderQualityPreset nextRenderQuality(RenderQualityPreset quality);

    // Camera basis and matrices for one rendered frame.
    struct FrameView
    {
        glm::vec3 position {0.0f};
        glm::vec3 forward {0.0f, 0.0f, -1.0f};
        glm::vec3 up {0.0f, 1.0f, 0.0f};
        glm::vec3 right {1.0f, 0.0f, 0.0f};
        glm::mat4 viewProjection {1.0f};
    };

    // Projects a world point to normalized device coordinates (x, y in -1..1).
    // Returns false for points behind the camera.
    [[nodiscard]] inline bool projectToNdc(const FrameView& view, const glm::vec3& point, glm::vec2& outNdc)
    {
        const glm::vec4 clip = view.viewProjection * glm::vec4(point, 1.0f);
        if (clip.w <= 1.0e-6f)
        {
            return false;
        }

        outNdc = glm::vec2(clip.x, clip.y) / clip.w;
        return true;
    }

    struct FrameSettings
    {
        glm::vec3 clearColor {0.0f};
        RenderQualityPreset quality {RenderQualityPreset::Balanced};
        PointLightRig lights {};
        int viewportWidth {1};
        int viewportHeight {1};

        // Moves the image sideways without turning the camera, in normalized
        // device units (+0.3 puts the scene 15% of the width to the right).
        float lensShiftX {0.0f};
    };

    // Optional emphasis on one ball (the cue ball while aiming).
    struct BallHighlight
    {
        Entity ball {};
        glm::vec3 emission {0.0f};
    };

    // Forward renderer for the prototype scene: one shader, procedural meshes.
    class SceneRenderer
    {
    public:
        SceneRenderer(float clothWidth, float clothDepth, float ballRadius);

        // Clears the frame and uploads camera, lights and quality uniforms.
        // Returns false if the camera entity is missing its components.
        [[nodiscard]] bool beginFrame(
            const Registry& registry,
            Entity camera,
            float alpha,
            const FrameSettings& settings,
            FrameView& outView);

        // Draws every entity with a mesh and material; pocketed balls are skipped.
        void drawWorld(Registry& registry, float alpha, const BallHighlight& highlight);

        // Draws the aim line and the cue-tip marker on the cue ball.
        void drawAimGuide(
            const glm::vec3& cueBallPosition,
            float ballRadius,
            const glm::vec3& aimDirection,
            float aimAngleRadians,
            float charge01,
            float strikeRight01,
            float strikeForward01);

        // A glowing marker: an ellipsoid with half-extents `size` in metres
        // (a flat y makes a disc) or, with box = true, a box with full extents
        // `size` (for lines such as the head string).
        void drawMarker(const glm::vec3& position, const glm::vec3& size, const glm::vec3& color, bool box = false);

        [[nodiscard]] Shader& shader() { return *m_shader; }
        [[nodiscard]] Mesh& cubeMesh() { return *m_cubeMesh; }

    private:
        void bindMaterial(const MaterialComponent& material, const glm::vec3& dynamicEmission, int ballVisualType);

        std::unique_ptr<Shader> m_shader;
        std::unique_ptr<Mesh> m_cubeMesh;
        std::unique_ptr<Mesh> m_planeMesh;
        std::unique_ptr<Mesh> m_sphereMesh;
        float m_sphereRadius {1.0f};
    };
}
