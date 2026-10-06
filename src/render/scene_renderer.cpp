#include "render/scene_renderer.h"

#include "core/asset_paths.h"
#include "render/camera.h"

#include <glm/gtc/matrix_transform.hpp>

#include <glad/gl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>

namespace BilliardsSaloon
{
    namespace
    {
        // Shadow maps on unit 0, ball numbers on 1, probe SH on 2..8, the
        // room's reflection cube map on 9.
        constexpr int PROBE_TEXTURE_UNIT = 2;
        constexpr int REFLECTION_TEXTURE_UNIT = 9;
        // A model's base colour, normal and occlusion-roughness-metal maps.
        constexpr int MATERIAL_TEXTURE_UNIT = 10;

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
        m_prefilterShader = shader("shaders/fullscreen.vert", "shaders/prefilter.frag");
        m_numbers = std::make_unique<NumberAtlas>(resolveAssetPath("fonts/BarlowCondensed-Bold.ttf"));
        glGenVertexArrays(1, &m_emptyVertexArray);   // core profile: draws need a bound VAO
        glGenQueries(TIMER_QUERIES, m_timerQueries.data());

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
        glDeleteQueries(TIMER_QUERIES, m_timerQueries.data());
        for (const auto& [key, cached] : m_textures)
        {
            glDeleteTextures(1, &cached.texture);
        }
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
        const glm::mat4 lightProjection = glm::perspective(glm::radians(140.0f), 1.0f, 0.12f, 3.5f);
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
        m_viewFrustum = Frustum::fromViewProjection(outView.viewProjection);

        m_shader->bind();
        m_cameraView = viewMatrix;
        m_cameraProjection = projectionMatrix;
        m_cameraPosition = outView.position;
        setCamera(viewMatrix, projectionMatrix, outView.position);

        m_shader->setInt("uActivePointLightCount", static_cast<int>(std::min<std::size_t>(settings.lights.positions.size(), 3)));
        m_shader->setFloat("uEmissionScale", 1.0f);
        m_shader->setFloat("uLightIntensity", settings.lightIntensity);
        m_ambientScale = settings.ambient;
        m_shader->setFloat("uAmbientScale", settings.ambient);
        m_shader->setInt("uCapture", 0);
        for (int t = 0; t < LightProbes::SH_TEXTURES; ++t)
        {
            m_shader->setInt("uProbeSH" + std::to_string(t), PROBE_TEXTURE_UNIT + t);
        }
        m_shader->setInt("uReflection", REFLECTION_TEXTURE_UNIT);
        m_shader->setInt("uBaseColorMap", MATERIAL_TEXTURE_UNIT);
        m_shader->setInt("uNormalMap", MATERIAL_TEXTURE_UNIT + 1);
        m_shader->setInt("uOrmMap", MATERIAL_TEXTURE_UNIT + 2);
        m_shader->setFloat("uReflectionMaxLod", static_cast<float>(ReflectionProbe::LEVELS - 1));
        // Real reflections of the room carry their own brightness; the old
        // analytic environment was dimmed for a dark hall.
        m_shader->setFloat("uReflectionScale", m_reflection.ready() ? 1.0f : 0.45f);
        m_shader->setInt("uMeasleCueBall", m_measleCueBall ? 1 : 0);
        m_exposure = settings.exposure;
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

        // Statistics for this frame; GPU time from the oldest query in the ring.
        const RenderStats last = m_stats;
        m_stats = RenderStats{};
        m_stats.gpuMilliseconds = last.gpuMilliseconds;
        m_stats.probeCaptureMilliseconds = last.probeCaptureMilliseconds;
        m_stats.probes = last.probes;
        const unsigned int query = m_timerQueries[static_cast<std::size_t>(m_timerIndex)];
        if (m_timerPending[static_cast<std::size_t>(m_timerIndex)])
        {
            GLuint64 nanoseconds = 0;
            glGetQueryObjectui64v(query, GL_QUERY_RESULT, &nanoseconds);
            m_stats.gpuMilliseconds = static_cast<float>(static_cast<double>(nanoseconds) * 1.0e-6);
        }
        glBeginQuery(GL_TIME_ELAPSED, query);

        return true;
    }

    void SceneRenderer::gatherLocalLights(Registry& registry)
    {
        m_localLights.clear();
        registry.view<TransformComponent, LocalLightComponent>().each(
            [&](Entity, TransformComponent& transform, LocalLightComponent& light)
            {
                if (static_cast<int>(m_localLights.size()) >= MAX_LOCAL_LIGHTS)
                {
                    return;
                }
                const std::string index = "[" + std::to_string(m_localLights.size()) + "]";
                const glm::vec3 radiance = light.color * light.intensity;
                m_shader->setVec4("uLocalPositionRange" + index, glm::vec4(transform.position, light.range));
                m_shader->setVec4("uLocalColor" + index, glm::vec4(radiance, light.spotInnerCos));
                m_shader->setVec4("uLocalSpot" + index, glm::vec4(glm::normalize(light.direction), light.spotOuterCos));
                m_localLights.push_back(LocalLight{transform.position, light.range,
                                                   std::max({radiance.r, radiance.g, radiance.b})});
            });
        m_stats.localLights = static_cast<int>(m_localLights.size());
        m_shader->setInt("uLocalCount", 0);
    }

    SceneRenderer::LightChoice SceneRenderer::chooseLocalLights(const Bounds& world) const
    {
        // Lights whose sphere of influence touches the box, brightest at the
        // box first (by intensity over squared distance to it).
        std::array<std::pair<float, int>, MAX_LOCAL_LIGHTS> reaching {};
        int count = 0;
        for (std::size_t i = 0; i < m_localLights.size(); ++i)
        {
            const LocalLight& light = m_localLights[i];
            const glm::vec3 nearest = glm::clamp(light.position, world.min, world.max);
            const glm::vec3 offset = light.position - nearest;
            const float distanceSquared = glm::dot(offset, offset);
            if (distanceSquared < light.range * light.range)
            {
                reaching[static_cast<std::size_t>(count++)] = {light.intensity / (distanceSquared + 0.25f), static_cast<int>(i)};
            }
        }
        LightChoice choice;
        choice.count = std::min(count, LIGHTS_PER_DRAW);
        std::partial_sort(reaching.begin(), reaching.begin() + choice.count, reaching.begin() + count,
                          [](const auto& a, const auto& b) { return a.first > b.first; });
        for (int i = 0; i < choice.count; ++i)
        {
            choice.indices[static_cast<std::size_t>(i)] = reaching[static_cast<std::size_t>(i)].second;
        }
        return choice;
    }

    void SceneRenderer::uploadLocalLights(const LightChoice& choice)
    {
        m_shader->setInts("uLocalIndices", choice.indices.data(), LIGHTS_PER_DRAW);
        m_shader->setInt("uLocalCount", choice.count);
        m_stats.lightsPerDrawMax = std::max(m_stats.lightsPerDrawMax, choice.count);
    }

    std::vector<SceneRenderer::CaptureItem> SceneRenderer::captureItems(Registry& registry, float alpha)
    {
        // Everything a probe sees, resolved once: the coarsest mesh, its
        // transform and bounds, material and lights. Small things (balls,
        // bottles) hardly change the light a room bounces and are left out.
        std::vector<CaptureItem> items;
        registry.view<TransformComponent, StaticMeshComponent, MaterialComponent>().each(
            [&](Entity entity, TransformComponent& transform, StaticMeshComponent& meshComponent, MaterialComponent& material)
            {
                const BallComponent* ball = registry.tryGet<BallComponent>(entity);
                if ((ball != nullptr) && ball->pocketed)
                {
                    return;
                }
                Mesh* mesh = meshFor(meshComponent);
                if (mesh == nullptr)
                {
                    return;
                }
                CaptureItem item;
                item.model = composeInterpolatedMatrix(transform, alpha);
                item.world = transformBounds(mesh->bounds(), item.model);
                if (glm::length(item.world.extent()) < 0.1f)
                {
                    return;
                }
                if (const LodComponent* lod = registry.tryGet<LodComponent>(entity); (lod != nullptr) && !lod->lods.empty())
                {
                    if (Mesh* coarse = meshForData(lod->lods.back()))
                    {
                        mesh = coarse;
                    }
                }
                item.mesh = mesh;
                item.material = &material;
                item.lights = chooseLocalLights(item.world);
                item.ballVisual = (ball != nullptr) ? ballVisualType(*ball) : 0;
                item.ballNumber = ((ball != nullptr) && !ball->isCueBall) ? ball->number : 0;
                items.push_back(item);
            });
        return items;
    }

    void SceneRenderer::drawCaptureItems(const std::vector<CaptureItem>& items, const Frustum& frustum)
    {
        for (const CaptureItem& item : items)
        {
            if (!frustum.intersects(item.world))
            {
                continue;
            }
            uploadLocalLights(item.lights);
            m_shader->setMat4("uModel", item.model);
            glEnable(GL_CULL_FACE);
            bindMaterial(*item.material, glm::vec3(0.0f), item.ballVisual, item.ballNumber);
            item.mesh->draw();
        }
        glEnable(GL_CULL_FACE);
    }

    void SceneRenderer::countDraw(const Mesh& mesh)
    {
        ++m_stats.drawCalls;
        m_stats.triangles += mesh.indexCount() / 3;
    }

    Mesh* SceneRenderer::meshFor(const StaticMeshComponent& component)
    {
        switch (component.primitive)
        {
            case MeshPrimitive::Cube:
                return m_cubeMesh.get();
            case MeshPrimitive::Plane:
                return m_planeMesh.get();
            case MeshPrimitive::Sphere:
                return m_sphereMesh.get();
            case MeshPrimitive::Custom:
                break;
        }
        return meshForData(component.custom);
    }

    Mesh* SceneRenderer::meshForData(const std::shared_ptr<const MeshData>& data)
    {
        if (!data)
        {
            return nullptr;
        }

        const MeshData* key = data.get();
        auto found = m_customMeshes.find(key);
        if ((found != m_customMeshes.end()) && found->second.data.expired())
        {
            m_customMeshes.erase(found);   // the address was reused by new data
            found = m_customMeshes.end();
        }
        if (found == m_customMeshes.end())
        {
            // Drop meshes whose scenes are gone, then upload this one.
            for (auto it = m_customMeshes.begin(); it != m_customMeshes.end();)
            {
                it = it->second.data.expired() ? m_customMeshes.erase(it) : std::next(it);
            }
            found = m_customMeshes.emplace(key, CachedMesh{data, Mesh::fromData(*data)}).first;
        }
        return found->second.mesh.get();
    }

    unsigned int SceneRenderer::textureFor(const std::shared_ptr<const ModelTexture>& texture, bool srgb)
    {
        if (!texture || texture->rgba.empty())
        {
            return 0;
        }
        auto found = m_textures.find(texture.get());
        if ((found != m_textures.end()) && found->second.data.expired())
        {
            glDeleteTextures(1, &found->second.texture);
            m_textures.erase(found);
            found = m_textures.end();
        }
        if (found != m_textures.end())
        {
            return found->second.texture;
        }

        unsigned int name = 0;
        glGenTextures(1, &name);
        glBindTexture(GL_TEXTURE_2D, name);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        // Colour maps are stored in sRGB and sampled as linear; data maps raw.
        glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, texture->width, texture->height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, texture->rgba.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);
        m_textures.emplace(texture.get(), CachedTexture{texture, name});
        return name;
    }

    void SceneRenderer::bindSceneShader()
    {
        m_shader->bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_shadows.texture());
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_numbers->texture());
        glActiveTexture(GL_TEXTURE0);
        if (m_probes.ready())
        {
            m_probes.bindTextures(PROBE_TEXTURE_UNIT);
        }
        if (m_reflection.ready())
        {
            m_reflection.bind(REFLECTION_TEXTURE_UNIT);
        }
    }

    void SceneRenderer::setCamera(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& position)
    {
        m_eye = position;
        m_tanHalfFov = 1.0f / std::max(projection[1][1], 1.0e-4f);   // projection[1][1] = cot(fov / 2)
        m_shader->setMat4("uView", view);
        m_shader->setMat4("uProjection", projection);
        m_shader->setVec3("uViewPosition", position);
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
            const Frustum lampView = Frustum::fromViewProjection(m_lightMatrices[layer]);

            registry.view<TransformComponent, StaticMeshComponent, MaterialComponent>().each(
                [&](Entity entity, TransformComponent& transform, StaticMeshComponent& meshComponent, MaterialComponent& material)
                {
                    // Lamps hold the lights; floors and walls only receive.
                    if ((material.surfaceType == MaterialSurfaceType::LampGlass) ||
                        (meshComponent.primitive == MeshPrimitive::Plane) || !meshComponent.castsShadow)
                    {
                        return;
                    }
                    const BallComponent* ball = registry.tryGet<BallComponent>(entity);
                    if ((ball != nullptr) && ball->pocketed)
                    {
                        return;
                    }
                    Mesh* mesh = meshFor(meshComponent);
                    if (mesh == nullptr)
                    {
                        return;
                    }
                    const glm::mat4 model = composeInterpolatedMatrix(transform, alpha);
                    if (!lampView.intersects(transformBounds(mesh->bounds(), model)))
                    {
                        ++m_stats.shadowCulled;
                        return;
                    }
                    ++m_stats.shadowCasters;
                    // Props cast shadows at their coarsest level of detail: the
                    // shadow map's texels are millimetres wide at best.
                    if (const LodComponent* lod = registry.tryGet<LodComponent>(entity); (lod != nullptr) && !lod->lods.empty())
                    {
                        if (Mesh* coarse = meshForData(lod->lods.back()))
                        {
                            mesh = coarse;
                        }
                    }
                    m_shadowShader->setMat4("uModel", model);
                    mesh->draw();
                    countDraw(*mesh);
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
        m_postShader->setFloat("uExposure", m_exposure);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_sceneTarget.colorTexture());
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, levels.empty() ? 0U : levels.front().texture);
        glActiveTexture(GL_TEXTURE0);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        glEndQuery(GL_TIME_ELAPSED);
        m_timerPending[static_cast<std::size_t>(m_timerIndex)] = true;
        m_timerIndex = (m_timerIndex + 1) % TIMER_QUERIES;
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

        const ModelMaterial* model = material.textured.get();
        m_shader->setInt("uTextured", (model != nullptr) ? 1 : 0);
        if (model != nullptr)
        {
            const unsigned int maps[3] = {textureFor(model->baseColorMap, true), textureFor(model->normalMap, false), textureFor(model->ormMap, false)};
            for (int i = 0; i < 3; ++i)
            {
                glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + MATERIAL_TEXTURE_UNIT + i));
                glBindTexture(GL_TEXTURE_2D, maps[i]);
            }
            glActiveTexture(GL_TEXTURE0);
            m_shader->setVec4("uBaseColorFactor", model->baseColor);
            m_shader->setFloat("uMetallicFactor", model->metallic);
            m_shader->setFloat("uRoughnessFactor", model->roughness);
            m_shader->setInt("uHasBaseColorMap", maps[0] != 0 ? 1 : 0);
            m_shader->setInt("uHasNormalMap", maps[1] != 0 ? 1 : 0);
            m_shader->setInt("uHasOrmMap", maps[2] != 0 ? 1 : 0);
            m_shader->setInt("uOcclusionInOrm", model->occlusionInOrm ? 1 : 0);
            m_shader->setInt("uAlphaMask", model->alphaMask ? 1 : 0);
        }
        // Thin double-sided model surfaces (leaves) are drawn from both
        // sides. Other draws keep the caller's culling (the cue turns it off).
        if (model != nullptr)
        {
            if (model->doubleSided)
            {
                glDisable(GL_CULL_FACE);
            }
            else
            {
                glEnable(GL_CULL_FACE);
            }
        }
    }

    void SceneRenderer::drawWorld(Registry& registry, float alpha, const BallHighlight& highlight)
    {
        renderShadows(registry, alpha);
        gatherLocalLights(registry);
        updateProbes(registry, alpha);
        drawObjects(registry, alpha, m_viewFrustum, &highlight, true);
    }

    void SceneRenderer::drawObjects(Registry& registry, float alpha, const Frustum& frustum, const BallHighlight* highlight, bool count)
    {
        registry.view<TransformComponent, StaticMeshComponent, MaterialComponent>().each(
            [&](Entity entity, TransformComponent& transform, StaticMeshComponent& meshComponent, MaterialComponent& material)
            {
                const BallComponent* ball = registry.tryGet<BallComponent>(entity);
                if ((ball != nullptr) && ball->pocketed)
                {
                    return;
                }

                Mesh* mesh = meshFor(meshComponent);

                if (mesh == nullptr)
                {
                    return;
                }

                const glm::mat4 model = composeInterpolatedMatrix(transform, alpha);
                const Bounds world = transformBounds(mesh->bounds(), model);
                if (!frustum.intersects(world))
                {
                    m_stats.objectsCulled += count ? 1 : 0;
                    return;
                }
                bindLocalLights(world);

                // A coarser level of detail when the part is small on screen.
                if (const LodComponent* lod = registry.tryGet<LodComponent>(entity); (lod != nullptr) && (lod->lods.size() > 1))
                {
                    const float coverage = glm::length(world.extent()) /
                                           (std::max(glm::length(world.centre() - m_eye), 1.0e-3f) * m_tanHalfFov);
                    // Probe captures are 32 texels a face: the coarsest level is plenty.
                    const std::size_t level = !count ? lod->lods.size() - 1
                                            : (coverage > 0.12f) ? 0U : ((coverage > 0.04f) ? 1U : 2U);
                    const std::size_t chosen = std::min(level, lod->lods.size() - 1);
                    if (Mesh* coarser = meshForData(lod->lods[chosen]))
                    {
                        mesh = coarser;
                    }
                    m_stats.lodReduced += (count && (chosen > 0)) ? 1 : 0;
                }

                const glm::vec3 dynamicEmission =
                    ((highlight != nullptr) && (entity == highlight->ball)) ? highlight->emission : glm::vec3(0.0f);

                m_shader->setMat4("uModel", model);
                glEnable(GL_CULL_FACE);   // bindMaterial turns it off for double-sided models
                const int number = ((ball != nullptr) && !ball->isCueBall) ? ball->number : 0;
                bindMaterial(material, dynamicEmission, (ball != nullptr) ? ballVisualType(*ball) : 0, number);
                mesh->draw();
                if (count)
                {
                    ++m_stats.objectsDrawn;
                    countDraw(*mesh);
                }
            }
        );
        glEnable(GL_CULL_FACE);
    }

    void SceneRenderer::updateProbes(Registry& registry, float alpha)
    {
        const ProbeVolumeComponent* volume = nullptr;
        registry.view<ProbeVolumeComponent>().each([&](Entity, ProbeVolumeComponent& v) { volume = &v; });
        if (volume == nullptr)
        {
            m_shader->setInt("uProbesEnabled", 0);
            m_shader->setInt("uReflectionEnabled", 0);
            return;
        }
        if (volume->revision == m_probeRevision)
        {
            return;
        }
        if (volume->pendingModels > 0)
        {
            // The hall's furniture is still loading: capture once it is all
            // in. Meanwhile, no indirect light from the previous hall.
            m_shader->setInt("uProbesEnabled", 0);
            m_shader->setInt("uReflectionEnabled", 0);
            m_shader->setFloat("uReflectionScale", 0.45f);
            return;
        }
        m_probeRevision = volume->revision;
        m_probeMin = volume->min;
        m_probeMax = volume->max;
        m_probeCounts = glm::max(volume->counts, glm::ivec3(2));
        const int probeCount = m_probeCounts.x * m_probeCounts.y * m_probeCounts.z;

        const auto started = std::chrono::steady_clock::now();
        const glm::mat4 faceProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.02f, 40.0f);
        m_shader->setVec3("uProbeMin", m_probeMin);
        m_shader->setVec3("uProbeMax", m_probeMax);
        m_shader->setVec3("uProbeCounts", glm::vec3(m_probeCounts));
        m_shader->setInt("uCapture", 1);

        const std::vector<CaptureItem> items = captureItems(registry, alpha);

        // Two bounces: first the probes see direct light and emitters only;
        // then again, lit by the first result, so light bounces twice.
        for (int bounce = 0; bounce < 2; ++bounce)
        {
            m_shader->setInt("uProbesEnabled", (bounce > 0) ? 1 : 0);
            m_shader->setFloat("uAmbientScale", (bounce > 0) ? m_ambientScale : 0.0f);
            m_probes.beginCapture(probeCount);
            for (int z = 0; z < m_probeCounts.z; ++z)
            {
                for (int y = 0; y < m_probeCounts.y; ++y)
                {
                    for (int x = 0; x < m_probeCounts.x; ++x)
                    {
                        const int probe = (z * m_probeCounts.y + y) * m_probeCounts.x + x;
                        const glm::vec3 t = glm::vec3(x, y, z) / glm::vec3(m_probeCounts - 1);
                        const glm::vec3 eye = glm::mix(m_probeMin, m_probeMax, t);
                        for (int face = 0; face < 6; ++face)
                        {
                            m_probes.bindTile(probe, face);
                            const glm::mat4 view = LightProbes::faceView(eye, face);
                            setCamera(view, faceProjection, eye);
                            drawCaptureItems(items, Frustum::fromViewProjection(faceProjection * view));
                        }
                    }
                }
            }
            const std::vector<ShRgb9> captured = m_probes.finishCapture(probeCount);
            m_probes.upload(captured, m_probeCounts);
            bindSceneShader();
        }

        // The reflection cube map, seen from above the table with the probes'
        // indirect light, then prefiltered by roughness.
        {
            const glm::mat4 faceProjection60 = glm::perspective(glm::radians(90.0f), 1.0f, 0.03f, 60.0f);
            for (int face = 0; face < 6; ++face)
            {
                m_reflection.bindCaptureFace(face);
                const glm::mat4 view = LightProbes::faceView(volume->reflectionCentre, face);
                setCamera(view, faceProjection60, volume->reflectionCentre);
                drawObjects(registry, alpha, Frustum::fromViewProjection(faceProjection60 * view), nullptr, false);
            }
            glBindVertexArray(m_emptyVertexArray);
            m_reflection.prefilter(*m_prefilterShader);
            glBindVertexArray(0);
            bindSceneShader();
            m_shader->setVec3("uRoomMin", volume->roomMin);
            m_shader->setVec3("uRoomMax", volume->roomMax);
            m_shader->setVec3("uReflectionCentre", volume->reflectionCentre);
            m_shader->setInt("uReflectionEnabled", 1);
            m_shader->setFloat("uReflectionScale", 1.0f);
        }

        m_shader->setInt("uCapture", 0);
        m_shader->setInt("uProbesEnabled", 1);
        m_shader->setFloat("uAmbientScale", m_ambientScale);
        setCamera(m_cameraView, m_cameraProjection, m_cameraPosition);
        m_sceneTarget.bindForDrawing();
        m_stats.probes = probeCount;
        m_stats.probeCaptureMilliseconds = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - started).count();
    }

    void SceneRenderer::setCueStyle(const glm::vec3& shaft, const glm::vec3& forearm, const glm::vec3& wrap, const glm::vec3& joint)
    {
        // Segments from the tip: tip, ferrule, shaft, joint, forearm, wrap, sleeve, bumper.
        if (m_cue.size() >= 7)
        {
            m_cue[2].color = shaft;
            m_cue[3].color = joint;
            m_cue[4].color = forearm;
            m_cue[5].color = wrap;
            m_cue[6].color = forearm;
        }
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

        Bounds reach;
        reach.add(tipPosition);
        reach.add(tipPosition + axis * 1.5f);
        bindLocalLights(reach);

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
