#include "render/scene_renderer.h"

#include "core/asset_paths.h"
#include "render/camera.h"

#include <glad/gl.h>

#include <algorithm>
#include <string>

namespace BilliardsSaloon
{
    namespace
    {
        // Must match BALL_VISUAL_* in basic.frag.
        int ballVisualType(const BallComponent& ball)
        {
            if (ball.isCueBall)
            {
                return 1;
            }

            switch (ball.ruleTag)
            {
                case BallRuleTag::Stripe:
                    return 3;

                case BallRuleTag::Eight:
                    return 4;

                case BallRuleTag::Solid:
                case BallRuleTag::Numbered:
                case BallRuleTag::Red:
                case BallRuleTag::Color:
                case BallRuleTag::Cue:
                    return 2;
            }

            return 0;
        }

        MaterialComponent flatMaterial(const glm::vec3& albedo, float specularStrength, float shininess, float roughness, float reflectivity)
        {
            return MaterialComponent{
                .albedo = albedo,
                .specularStrength = specularStrength,
                .shininess = shininess,
                .surfaceType = MaterialSurfaceType::Generic,
                .roughness = roughness,
                .reflectivity = reflectivity,
                .clearcoatStrength = 0.0f,
                .emissionColor = glm::vec3(0.0f),
                .emissionIntensity = 0.0f
            };
        }
    }

    const char* renderQualityLabel(RenderQualityPreset quality)
    {
        switch (quality)
        {
            case RenderQualityPreset::Low:
                return "Low";

            case RenderQualityPreset::Balanced:
                return "Balanced";

            case RenderQualityPreset::High:
                return "High";
        }

        return "Unknown";
    }

    RenderQualityPreset nextRenderQuality(RenderQualityPreset quality)
    {
        switch (quality)
        {
            case RenderQualityPreset::Low:
                return RenderQualityPreset::Balanced;

            case RenderQualityPreset::Balanced:
                return RenderQualityPreset::High;

            case RenderQualityPreset::High:
                return RenderQualityPreset::Low;
        }

        return RenderQualityPreset::Balanced;
    }

    SceneRenderer::SceneRenderer(float clothWidth, float clothDepth, float ballRadius)
        : m_shader(std::make_unique<Shader>(
              resolveAssetPath("shaders/basic.vert").string(),
              resolveAssetPath("shaders/basic.frag").string()))
        , m_cubeMesh(Mesh::createCube())
        , m_planeMesh(Mesh::createPlane(clothWidth, clothDepth))
        , m_sphereMesh(Mesh::createUVSphere(ballRadius, 40U, 20U))
    {
    }

    bool SceneRenderer::beginFrame(
        const Registry& registry,
        Entity camera,
        float alpha,
        const FrameSettings& settings,
        FrameView& outView)
    {
        glViewport(0, 0, settings.viewportWidth, settings.viewportHeight);
        glClearColor(settings.clearColor.r, settings.clearColor.g, settings.clearColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (!registry.has<TransformComponent>(camera) || !registry.has<CameraComponent>(camera))
        {
            return false;
        }

        const TransformComponent& cameraTransform = registry.get<TransformComponent>(camera);
        const CameraComponent& cameraLens = registry.get<CameraComponent>(camera);

        const InterpolatedTransform interpolated = interpolateTransform(cameraTransform, alpha);

        outView.position = interpolated.position;
        outView.forward = interpolated.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        outView.up = interpolated.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        outView.right = interpolated.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
        outView.overlayFrame = UiOverlayFrame{
            .rotation = interpolated.rotation,
            .right = outView.right,
            .up = outView.up
        };

        const float aspectRatio =
            static_cast<float>(settings.viewportWidth) / static_cast<float>(std::max(settings.viewportHeight, 1));

        m_shader->bind();
        m_shader->setMat4("uView", Camera::viewMatrix(outView.position, outView.forward, outView.up));
        m_shader->setMat4("uProjection", Camera::projectionMatrix(
            cameraLens.verticalFieldOfViewRadians,
            aspectRatio,
            cameraLens.nearPlane,
            cameraLens.farPlane
        ));
        m_shader->setVec3("uViewPosition", outView.position);

        m_shader->setVec3("uDirectionalLightDirection", glm::normalize(glm::vec3(-0.35f, -1.0f, -0.18f)));
        m_shader->setVec3("uDirectionalLightColor", glm::vec3(0.22f, 0.24f, 0.28f));

        int activePointLightCount = 2;
        float reflectionScale = 0.55f;
        float emissionScale = 0.85f;

        switch (settings.quality)
        {
            case RenderQualityPreset::Low:
                activePointLightCount = 1;
                reflectionScale = 0.15f;
                emissionScale = 0.70f;
                break;

            case RenderQualityPreset::Balanced:
                activePointLightCount = 2;
                reflectionScale = 0.55f;
                emissionScale = 0.85f;
                break;

            case RenderQualityPreset::High:
                activePointLightCount = 3;
                reflectionScale = 1.0f;
                emissionScale = 1.0f;
                break;
        }

        m_shader->setInt("uActivePointLightCount", activePointLightCount);
        m_shader->setFloat("uReflectionScale", reflectionScale);
        m_shader->setFloat("uEmissionScale", emissionScale);

        for (std::size_t lightIndex = 0; lightIndex < settings.lights.positions.size(); ++lightIndex)
        {
            const std::string index = std::to_string(lightIndex);
            m_shader->setVec3("uPointLightPositions[" + index + "]", settings.lights.positions[lightIndex]);
            m_shader->setVec3("uPointLightColors[" + index + "]", settings.lights.colors[lightIndex]);
        }

        return true;
    }

    void SceneRenderer::bindMaterial(const MaterialComponent& material, const glm::vec3& dynamicEmission, int visualType)
    {
        m_shader->setVec3("uMaterialAlbedo", material.albedo);
        m_shader->setFloat("uMaterialSpecularStrength", material.specularStrength);
        m_shader->setFloat("uMaterialShininess", material.shininess);
        m_shader->setInt("uMaterialSurfaceType", static_cast<int>(material.surfaceType));
        m_shader->setFloat("uMaterialRoughness", material.roughness);
        m_shader->setFloat("uMaterialReflectivity", material.reflectivity);
        m_shader->setFloat("uMaterialClearcoatStrength", material.clearcoatStrength);
        m_shader->setVec3("uEmissionColor", material.emissionColor * material.emissionIntensity + dynamicEmission);
        m_shader->setInt("uBallVisualType", visualType);
    }

    void SceneRenderer::drawWorld(Registry& registry, float alpha, const BallHighlight& highlight)
    {
        registry.view<TransformComponent, StaticMeshComponent, MaterialComponent>().each(
            [&](Entity entity, TransformComponent& transform, StaticMeshComponent& meshComponent, MaterialComponent& material)
            {
                const BallComponent* ball = registry.tryGet<BallComponent>(entity);
                if ((ball != nullptr) && ball->pocketed)
                {
                    return;
                }

                Mesh* mesh = nullptr;

                switch (meshComponent.primitive)
                {
                    case MeshPrimitive::Cube:
                        mesh = m_cubeMesh.get();
                        break;
                    case MeshPrimitive::Plane:
                        mesh = m_planeMesh.get();
                        break;
                    case MeshPrimitive::Sphere:
                        mesh = m_sphereMesh.get();
                        break;
                }

                if (mesh == nullptr)
                {
                    return;
                }

                const glm::vec3 dynamicEmission =
                    (entity == highlight.ball) ? highlight.emission : glm::vec3(0.0f);

                m_shader->setMat4("uModel", composeInterpolatedMatrix(transform, alpha));
                bindMaterial(material, dynamicEmission, (ball != nullptr) ? ballVisualType(*ball) : 0);
                mesh->draw();
            }
        );
    }

    void SceneRenderer::drawAimGuide(
        const glm::vec3& cueBallPosition,
        float ballRadius,
        const glm::vec3& aimDirection,
        float aimAngleRadians,
        float charge01,
        float strikeRight01,
        float strikeForward01)
    {
        const MaterialComponent guideMaterial =
            flatMaterial(glm::vec3(0.92f, 0.82f, 0.42f), 0.20f, 16.0f, 0.38f, 0.04f);

        m_shader->setMat4("uModel", composeMatrix(
            cueBallPosition + aimDirection * 0.18f,
            glm::quat(glm::vec3(0.0f, -aimAngleRadians, 0.0f)),
            glm::vec3(0.03f, 0.03f, 0.18f + 0.35f * charge01)
        ));
        bindMaterial(
            guideMaterial,
            glm::vec3(0.10f, 0.08f, 0.02f) + glm::vec3(0.10f, 0.06f, 0.01f) * charge01,
            0
        );
        m_cubeMesh->draw();

        const glm::vec3 up(0.0f, 1.0f, 0.0f);
        const glm::vec3 right = glm::normalize(glm::cross(up, aimDirection));
        const float markerRadius = ballRadius * 0.72f;

        const glm::vec3 markerPosition =
            cueBallPosition +
            right * (strikeRight01 * markerRadius) +
            aimDirection * (strikeForward01 * markerRadius) +
            glm::vec3(0.0f, ballRadius * 0.25f, 0.0f);

        const MaterialComponent markerMaterial =
            flatMaterial(glm::vec3(0.10f, 0.10f, 0.12f), 0.10f, 12.0f, 0.52f, 0.03f);

        m_shader->setMat4("uModel", composeMatrix(
            markerPosition,
            glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
            glm::vec3(0.012f)
        ));
        bindMaterial(markerMaterial, glm::vec3(0.18f, 0.12f, 0.02f), 0);
        m_cubeMesh->draw();
    }
}
