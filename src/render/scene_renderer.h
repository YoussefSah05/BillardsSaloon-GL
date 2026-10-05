#pragma once

#include "ecs/entity.h"
#include "ecs/registry.h"
#include "render/light_rig.h"
#include "render/mesh.h"
#include "render/number_atlas.h"
#include "render/render_targets.h"
#include "render/shader.h"
#include "scene/components.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <memory>
#include <vector>

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
        ~SceneRenderer();

        // Clears the frame and uploads camera, lights and quality uniforms.
        // Returns false if the camera entity is missing its components.
        [[nodiscard]] bool beginFrame(
            const Registry& registry,
            Entity camera,
            float alpha,
            const FrameSettings& settings,
            FrameView& outView);

        // Renders the lamps' shadow maps, then every entity with a mesh and
        // material into the HDR scene buffer; pocketed balls are skipped.
        void drawWorld(Registry& registry, float alpha, const BallHighlight& highlight);

        // Resolves the scene, adds bloom, tone maps and writes the result to the
        // window (framebuffer 0), ready for the UI to draw on top.
        void endFrame(int framebufferWidth, int framebufferHeight);

        // The cue stick: tip at tipPosition, the butt along buttDirection.
        void drawCue(const glm::vec3& tipPosition, const glm::vec3& buttDirection);

        // A translucent polyline on the cloth (aim and path guides).
        void drawPath(const std::vector<glm::vec3>& points, const glm::vec3& color, float width, float alpha);

        // A translucent ball, e.g. the ghost ball where the cue ball will make contact.
        void drawGhostBall(const glm::vec3& position, const glm::vec3& color, float alpha);

        // A glowing marker: an ellipsoid with half-extents `size` in metres
        // (a flat y makes a disc) or, with box = true, a box with full extents
        // `size` (for lines such as the head string).
        void drawMarker(const glm::vec3& position, const glm::vec3& size, const glm::vec3& color, bool box = false);

        [[nodiscard]] Shader& shader() { return *m_shader; }
        [[nodiscard]] Mesh& cubeMesh() { return *m_cubeMesh; }

    private:
        void bindMaterial(const MaterialComponent& material, const glm::vec3& dynamicEmission, int ballVisualType, int ballNumber = 0);
        void renderShadows(Registry& registry, float alpha);
        void bindSceneShader();

        std::unique_ptr<Shader> m_shader;
        std::unique_ptr<Shader> m_shadowShader;
        std::unique_ptr<Shader> m_bloomDownShader;
        std::unique_ptr<Shader> m_bloomUpShader;
        std::unique_ptr<Shader> m_postShader;
        unsigned int m_emptyVertexArray {0};

        SceneTarget m_sceneTarget;
        BloomChain m_bloom;
        ShadowMaps m_shadows;
        std::unique_ptr<NumberAtlas> m_numbers;

        // Per-frame choices from the quality preset and the light rig.
        bool m_bloomEnabled {true};
        bool m_shadowsEnabled {true};
        int m_shadowKernel {3};
        std::vector<glm::mat4> m_lightMatrices;
        std::unique_ptr<Mesh> m_cubeMesh;
        std::unique_ptr<Mesh> m_planeMesh;
        std::unique_ptr<Mesh> m_sphereMesh;
        float m_sphereRadius {1.0f};

        struct CueSegment
        {
            std::unique_ptr<Mesh> mesh;
            float start {0.0f};        // distance from the tip
            glm::vec3 color {1.0f};
            float specular {0.4f};
        };
        std::vector<CueSegment> m_cue;

        void beginTranslucent();
        void endTranslucent();
    };
}
