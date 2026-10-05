#include "render/scene_renderer.h"

#include "core/asset_paths.h"
#include "render/camera.h"

#include <glm/gtc/matrix_transform.hpp>

#include <glad/gl.h>

#include <algorithm>
#include <cmath>
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
        , m_sphereRadius(ballRadius)
    {
        const auto shader = [](const char* vertex, const char* fragment)
        {
            return std::make_unique<Shader>(resolveAssetPath(vertex).string(), resolveAssetPath(fragment).string());
        };
        m_shadowShader = shader("shaders/shadow.vert", "shaders/shadow.frag");
        m_bloomDownShader = shader("shaders/fullscreen.vert", "shaders/bloom_down.frag");
        m_bloomUpShader = shader("shaders/fullscreen.vert", "shaders/bloom_up.frag");
        m_postShader = shader("shaders/fullscreen.vert", "shaders/post.frag");
        m_numbers = std::make_unique<NumberAtlas>(resolveAssetPath("fonts/BarlowCondensed-Bold.ttf"));
        glGenVertexArrays(1, &m_emptyVertexArray);   // core profile: draws need a bound VAO

        // A two-piece playing cue, from the tip back: leather tip, ferrule,
        // tapered maple shaft, joint collar, forearm, wrap, butt sleeve, bumper.
        // Lengths and radii in metres (58 in / 1.47 m overall).
        struct Piece { float length; float r0; float r1; glm::vec3 color; float specular; };
        const Piece pieces[] = {
            {0.010f, 0.0065f, 0.0065f, glm::vec3(0.10f, 0.16f, 0.32f), 0.10f},
            {0.020f, 0.0065f, 0.0066f, glm::vec3(0.93f, 0.91f, 0.86f), 0.60f},
            {0.690f, 0.0066f, 0.0106f, glm::vec3(0.86f, 0.74f, 0.55f), 0.45f},
            {0.022f, 0.0108f, 0.0108f, glm::vec3(0.78f, 0.78f, 0.80f), 0.90f},
            {0.260f, 0.0110f, 0.0124f, glm::vec3(0.24f, 0.10f, 0.05f), 0.70f},
            {0.280f, 0.0125f, 0.0136f, glm::vec3(0.05f, 0.05f, 0.06f), 0.15f},
            {0.180f, 0.0137f, 0.0150f, glm::vec3(0.24f, 0.10f, 0.05f), 0.70f},
            {0.012f, 0.0150f, 0.0150f, glm::vec3(0.03f, 0.03f, 0.03f), 0.10f},
        };
        float start = 0.0f;
        for (const Piece& piece : pieces)
        {
            m_cue.push_back(CueSegment{Mesh::createFrustum(piece.r0, piece.r1, piece.length, 24U), start, piece.color, piece.specular});
            start += piece.length;
        }

    }

    SceneRenderer::~SceneRenderer()
    {
        glDeleteVertexArrays(1, &m_emptyVertexArray);
    }

    bool SceneRenderer::beginFrame(
        const Registry& registry,
        Entity camera,
        float alpha,
        const FrameSettings& settings,
        FrameView& outView)
    {
        // Quality: multisampling, bloom and shadow detail.
        int samples = 4;
        int shadowSize = 2048;
        m_bloomEnabled = true;
        m_shadowsEnabled = true;
        m_shadowKernel = 3;
        switch (settings.quality)
        {
            case RenderQualityPreset::Low:
                samples = 1;
                shadowSize = 1024;
                m_bloomEnabled = false;
                m_shadowKernel = 1;
                break;
            case RenderQualityPreset::Balanced:
                break;
            case RenderQualityPreset::High:
                samples = 8;
                m_shadowKernel = 5;
                break;
        }

        m_sceneTarget.ensure(settings.viewportWidth, settings.viewportHeight, samples);
        m_bloom.ensure(settings.viewportWidth, settings.viewportHeight, m_bloomEnabled ? 6 : 0);
        m_shadows.ensure(shadowSize, static_cast<int>(settings.lights.positions.size()));

        // Each lamp looks straight down over the whole table.
        m_lightMatrices.clear();
        const glm::mat4 lightProjection = glm::perspective(glm::radians(125.0f), 1.0f, 0.15f, 3.5f);
        for (const glm::vec3& lamp : settings.lights.positions)
        {
            const glm::mat4 lightView = glm::lookAt(lamp, lamp - glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
            m_lightMatrices.push_back(lightProjection * lightView);
        }

        m_sceneTarget.bindForDrawing();
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        // The scene buffer is linear light; the clear colour is given in sRGB.
        const glm::vec3 clear = glm::pow(settings.clearColor, glm::vec3(2.2f));
        glClearColor(clear.r, clear.g, clear.b, 1.0f);
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
        const float aspectRatio =
            static_cast<float>(settings.viewportWidth) / static_cast<float>(std::max(settings.viewportHeight, 1));

        const glm::mat4 viewMatrix = Camera::viewMatrix(outView.position, outView.forward, outView.up);
        glm::mat4 projectionMatrix = Camera::projectionMatrix(
            cameraLens.verticalFieldOfViewRadians,
            aspectRatio,
            cameraLens.nearPlane,
            cameraLens.farPlane
        );
        // Off-axis projection: clip.x gains -shift * z_view, i.e. +shift in NDC.
        projectionMatrix[2][0] -= settings.lensShiftX;
        outView.viewProjection = projectionMatrix * viewMatrix;

        m_shader->bind();
        m_shader->setMat4("uView", viewMatrix);
        m_shader->setMat4("uProjection", projectionMatrix);
        m_shader->setVec3("uViewPosition", outView.position);

        m_shader->setInt("uActivePointLightCount", static_cast<int>(std::min<std::size_t>(settings.lights.positions.size(), 3)));
        m_shader->setFloat("uReflectionScale", 0.45f);   // a dark hall: the lamps light the scene
        m_shader->setFloat("uEmissionScale", 1.0f);
        m_shader->setFloat("uLightIntensity", 2.6f);
        m_shader->setFloat("uLightRadius", 0.05f);
        m_shader->setInt("uShadowsEnabled", m_shadowsEnabled ? 1 : 0);
        m_shader->setInt("uShadowKernel", m_shadowKernel);
        m_shader->setFloat("uShadowTexel", 1.0f / static_cast<float>(shadowSize));
        m_shader->setInt("uShadowMaps", 0);
        m_shader->setInt("uNumberAtlas", 1);
        m_shader->setInt("uBallNumber", 0);
        for (std::size_t i = 0; i < m_lightMatrices.size(); ++i)
        {
            m_shader->setMat4("uLightMatrices[" + std::to_string(i) + "]", m_lightMatrices[i]);
        }
        bindSceneShader();
        m_shader->setFloat("uAlpha", 1.0f);

        for (std::size_t lightIndex = 0; lightIndex < settings.lights.positions.size(); ++lightIndex)
        {
            const std::string index = std::to_string(lightIndex);
            m_shader->setVec3("uPointLightPositions[" + index + "]", settings.lights.positions[lightIndex]);
            m_shader->setVec3("uPointLightColors[" + index + "]", settings.lights.colors[lightIndex]);
        }

        return true;
    }

    void SceneRenderer::bindSceneShader()
    {
        m_shader->bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_shadows.texture());
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_numbers->texture());
        glActiveTexture(GL_TEXTURE0);
    }

    void SceneRenderer::renderShadows(Registry& registry, float alpha)
    {
        if (!m_shadowsEnabled)
        {
            return;
        }

        m_shadowShader->bind();
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.5f, 2.0f);
        glDisable(GL_CULL_FACE);   // planes (cloth, floor) are one-sided; casters are closed anyway

        for (std::size_t layer = 0; layer < m_lightMatrices.size(); ++layer)
        {
            m_shadows.bindLayerForDrawing(static_cast<int>(layer));
            glClear(GL_DEPTH_BUFFER_BIT);
            m_shadowShader->setMat4("uLightMatrix", m_lightMatrices[layer]);

            registry.view<TransformComponent, StaticMeshComponent, MaterialComponent>().each(
                [&](Entity entity, TransformComponent& transform, StaticMeshComponent& meshComponent, MaterialComponent& material)
                {
                    // Lamps hold the lights; floors and walls only receive.
                    if ((material.surfaceType == MaterialSurfaceType::LampGlass) ||
                        (meshComponent.primitive == MeshPrimitive::Plane))
                    {
                        return;
                    }
                    const BallComponent* ball = registry.tryGet<BallComponent>(entity);
                    if ((ball != nullptr) && ball->pocketed)
                    {
                        return;
                    }
                    Mesh* mesh = (meshComponent.primitive == MeshPrimitive::Sphere) ? m_sphereMesh.get() : m_cubeMesh.get();
                    m_shadowShader->setMat4("uModel", composeInterpolatedMatrix(transform, alpha));
                    mesh->draw();
                });
        }

        glDisable(GL_POLYGON_OFFSET_FILL);
        glEnable(GL_CULL_FACE);
        m_sceneTarget.bindForDrawing();
        bindSceneShader();
    }

    void SceneRenderer::endFrame(int framebufferWidth, int framebufferHeight)
    {
        m_sceneTarget.resolve();
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glBindVertexArray(m_emptyVertexArray);

        const std::vector<BloomChain::Level>& levels = m_bloom.levels();
        if (m_bloomEnabled && !levels.empty())
        {
            // Down the chain from the scene...
            m_bloomDownShader->bind();
            m_bloomDownShader->setInt("uSource", 0);
            unsigned int source = m_sceneTarget.colorTexture();
            glm::vec2 sourceTexel(1.0f / static_cast<float>(m_sceneTarget.width()), 1.0f / static_cast<float>(m_sceneTarget.height()));
            for (std::size_t i = 0; i < levels.size(); ++i)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, levels[i].framebuffer);
                glViewport(0, 0, levels[i].width, levels[i].height);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, source);
                m_bloomDownShader->setVec2("uSourceTexel", sourceTexel);
                m_bloomDownShader->setInt("uFirstPass", (i == 0) ? 1 : 0);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                source = levels[i].texture;
                sourceTexel = glm::vec2(1.0f / static_cast<float>(levels[i].width), 1.0f / static_cast<float>(levels[i].height));
            }

            // ...and back up, adding each smaller level onto the next larger one.
            m_bloomUpShader->bind();
            m_bloomUpShader->setInt("uSource", 0);
            m_bloomUpShader->setFloat("uRadius", 1.0f);
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            for (std::size_t i = levels.size() - 1; i > 0; --i)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, levels[i - 1].framebuffer);
                glViewport(0, 0, levels[i - 1].width, levels[i - 1].height);
                glBindTexture(GL_TEXTURE_2D, levels[i].texture);
                m_bloomUpShader->setVec2("uSourceTexel", glm::vec2(1.0f / static_cast<float>(levels[i].width), 1.0f / static_cast<float>(levels[i].height)));
                glDrawArrays(GL_TRIANGLES, 0, 3);
            }
            glDisable(GL_BLEND);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, framebufferWidth, framebufferHeight);
        m_postShader->bind();
        m_postShader->setInt("uScene", 0);
        m_postShader->setInt("uBloom", 1);
        m_postShader->setInt("uBloomEnabled", (m_bloomEnabled && !levels.empty()) ? 1 : 0);
        m_postShader->setFloat("uBloomStrength", 0.045f);
        m_postShader->setFloat("uExposure", 1.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_sceneTarget.colorTexture());
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, levels.empty() ? 0U : levels.front().texture);
        glActiveTexture(GL_TEXTURE0);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
    }

    void SceneRenderer::bindMaterial(const MaterialComponent& material, const glm::vec3& dynamicEmission, int visualType, int ballNumber)
    {
        m_shader->setInt("uBallNumber", ballNumber);
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
        renderShadows(registry, alpha);

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
                const int number = ((ball != nullptr) && !ball->isCueBall) ? ball->number : 0;
                bindMaterial(material, dynamicEmission, (ball != nullptr) ? ballVisualType(*ball) : 0, number);
                mesh->draw();
            }
        );
    }

    void SceneRenderer::beginTranslucent()
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }

    void SceneRenderer::endTranslucent()
    {
        m_shader->setFloat("uAlpha", 1.0f);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    void SceneRenderer::drawCue(const glm::vec3& tipPosition, const glm::vec3& buttDirection)
    {
        const glm::vec3 axis = glm::normalize(buttDirection);
        const glm::quat rotation(glm::vec3(0.0f, 1.0f, 0.0f), axis);   // shortest arc from +y

        glDisable(GL_CULL_FACE);
        for (const CueSegment& segment : m_cue)
        {
            m_shader->setMat4("uModel", composeMatrix(tipPosition + axis * segment.start, rotation, glm::vec3(1.0f)));
            bindMaterial(flatMaterial(segment.color, segment.specular, 48.0f, 0.35f, 0.05f), glm::vec3(0.0f), 0);
            segment.mesh->draw();
        }
        glEnable(GL_CULL_FACE);
    }

    void SceneRenderer::drawPath(const std::vector<glm::vec3>& points, const glm::vec3& color, float width, float alpha)
    {
        if (points.size() < 2)
        {
            return;
        }

        beginTranslucent();
        m_shader->setFloat("uAlpha", alpha);
        bindMaterial(flatMaterial(color, 0.0f, 4.0f, 1.0f, 0.0f), color * 0.8f, 0);
        for (std::size_t i = 1; i < points.size(); ++i)
        {
            glm::vec3 a = points[i - 1];
            glm::vec3 b = points[i];
            a.y = b.y = 0.0015f;   // just above the cloth
            const glm::vec3 delta = b - a;
            const float length = glm::length(delta);
            if (length < 1.0e-4f)
            {
                continue;
            }
            const float yaw = std::atan2(delta.x, delta.z);
            m_shader->setMat4("uModel", composeMatrix(
                0.5f * (a + b), glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f)), glm::vec3(width, 0.0008f, length + 0.5f * width)));
            m_cubeMesh->draw();
        }
        endTranslucent();
    }

    void SceneRenderer::drawGhostBall(const glm::vec3& position, const glm::vec3& color, float alpha)
    {
        beginTranslucent();
        m_shader->setFloat("uAlpha", alpha);
        m_shader->setMat4("uModel", composeMatrix(position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f)));
        bindMaterial(flatMaterial(color, 0.3f, 32.0f, 0.5f, 0.0f), color * 0.25f, 0);
        m_sphereMesh->draw();
        endTranslucent();
    }

    void SceneRenderer::drawMarker(const glm::vec3& position, const glm::vec3& size, const glm::vec3& color, bool box)
    {
        const MaterialComponent material = flatMaterial(color, 0.10f, 8.0f, 0.60f, 0.0f);
        const glm::vec3 scale = box ? size : size / m_sphereRadius;
        m_shader->setMat4("uModel", composeMatrix(position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), scale));
        bindMaterial(material, color * 0.55f, 0);
        (box ? m_cubeMesh : m_sphereMesh)->draw();
    }

}
