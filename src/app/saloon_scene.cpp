#include "app/saloon_scene.h"

#include "gameplay/match_session.h"
#include "gameplay/sim_bridge.h"
#include "sim/table.h"
#include "render/camera.h"
#include "scene/components.h"

#include <string>

namespace BilliardsSaloon
{
    namespace
    {
        TransformComponent makeTransform(
            const glm::vec3& position,
            const glm::quat& rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
            const glm::vec3& scale = glm::vec3(1.0f))
        {
            TransformComponent transform;
            transform.position = position;
            transform.rotation = rotation;
            transform.scale = scale;
            transform.syncPrevious();
            return transform;
        }

        Entity createSceneEntity(
            Registry& registry,
            const std::string& name,
            const TransformComponent& transform,
            const StaticMeshComponent& mesh,
            const MaterialComponent& material)
        {
            const Entity entity = registry.createEntity();
            registry.emplace<NameComponent>(entity, NameComponent{name});
            registry.emplace<TransformComponent>(entity, transform);
            registry.emplace<StaticMeshComponent>(entity, mesh);
            registry.emplace<MaterialComponent>(entity, material);
            return entity;
        }

        MaterialComponent ballMaterial(const BallSpawnDefinition& definition)
        {
            return MaterialComponent{
                .albedo = definition.albedo,
                .specularStrength = definition.specularStrength,
                .shininess = definition.shininess,
                .surfaceType = MaterialSurfaceType::BallResin,
                .roughness = definition.isCueBall ? 0.08f : 0.10f,
                .reflectivity = definition.isCueBall ? 0.08f : 0.075f,
                .clearcoatStrength = 1.0f,
                .emissionColor = glm::vec3(0.0f),
                .emissionIntensity = 0.0f
            };
        }

        void attachBallVisual(Registry& registry, Entity ball, const BallSpawnDefinition& definition)
        {
            registry.emplace<NameComponent>(ball, NameComponent{definition.name});
            registry.emplace<StaticMeshComponent>(ball, StaticMeshComponent{MeshPrimitive::Sphere});
            registry.emplace<MaterialComponent>(ball, ballMaterial(definition));
        }
    }

    PointLightRig saloonLightRig()
    {
        PointLightRig rig;
        rig.positions = {
            glm::vec3(-0.82f, 1.58f, -0.04f),
            glm::vec3( 0.00f, 1.66f,  0.00f),
            glm::vec3( 0.82f, 1.58f, -0.04f)
        };
        rig.colors = {
            glm::vec3(4.4f, 3.1f, 1.7f),
            glm::vec3(5.2f, 3.7f, 2.0f),
            glm::vec3(4.4f, 3.1f, 1.7f)
        };
        return rig;
    }

    void buildSaloonScene(MatchSession& session)
    {
        Registry& registry = session.registry();
        const GameVariantDefinition& variant = session.variant();

        const float halfWidth = 0.5f * variant.table.clothWidth;
        const float halfDepth = 0.5f * variant.table.clothDepth;
        const PointLightRig lightRig = saloonLightRig();

        const MaterialComponent clothMaterial{
            .albedo = glm::vec3(0.07f, 0.36f, 0.16f),
            .specularStrength = 0.30f,
            .shininess = 24.0f,
            .surfaceType = MaterialSurfaceType::Cloth,
            .roughness = 0.58f,
            .reflectivity = 0.035f,
            .clearcoatStrength = 0.0f,
            .emissionColor = glm::vec3(0.0f),
            .emissionIntensity = 0.0f
        };

        const MaterialComponent railMaterial{
            .albedo = glm::vec3(0.30f, 0.16f, 0.07f),
            .specularStrength = 0.62f,
            .shininess = 96.0f,
            .surfaceType = MaterialSurfaceType::Wood,
            .roughness = 0.34f,
            .reflectivity = 0.04f,
            .clearcoatStrength = 0.12f,
            .emissionColor = glm::vec3(0.0f),
            .emissionIntensity = 0.0f
        };

        const MaterialComponent legMaterial{
            .albedo = glm::vec3(0.18f, 0.09f, 0.04f),
            .specularStrength = 0.36f,
            .shininess = 48.0f,
            .surfaceType = MaterialSurfaceType::Wood,
            .roughness = 0.42f,
            .reflectivity = 0.04f,
            .clearcoatStrength = 0.12f,
            .emissionColor = glm::vec3(0.0f),
            .emissionIntensity = 0.0f
        };

        const MaterialComponent floorMaterial{
            .albedo = glm::vec3(0.26f, 0.16f, 0.09f),
            .specularStrength = 0.28f,
            .shininess = 32.0f,
            .surfaceType = MaterialSurfaceType::Wood,
            .roughness = 0.60f,
            .reflectivity = 0.035f,
            .clearcoatStrength = 0.08f,
            .emissionColor = glm::vec3(0.0f),
            .emissionIntensity = 0.0f
        };

        const MaterialComponent wallMaterial{
            .albedo = glm::vec3(0.20f, 0.12f, 0.08f),
            .specularStrength = 0.08f,
            .shininess = 8.0f,
            .surfaceType = MaterialSurfaceType::Generic,
            .roughness = 0.92f,
            .reflectivity = 0.02f,
            .clearcoatStrength = 0.0f,
            .emissionColor = glm::vec3(0.0f),
            .emissionIntensity = 0.0f
        };

        const MaterialComponent lampMaterial{
            .albedo = glm::vec3(0.98f, 0.86f, 0.62f),
            .specularStrength = 0.35f,
            .shininess = 96.0f,
            .surfaceType = MaterialSurfaceType::LampGlass,
            .roughness = 0.10f,
            .reflectivity = 0.10f,
            .clearcoatStrength = 0.18f,
            .emissionColor = glm::vec3(1.00f, 0.68f, 0.28f),
            .emissionIntensity = 1.80f
        };

        // The cloth is drawn on the session's table entity (which holds the bounds).
        registry.emplace<NameComponent>(session.tableEntity(), NameComponent{"Table Cloth"});
        registry.emplace<TransformComponent>(session.tableEntity(), makeTransform(glm::vec3(0.0f)));
        registry.emplace<StaticMeshComponent>(session.tableEntity(), StaticMeshComponent{MeshPrimitive::Plane});
        registry.emplace<MaterialComponent>(session.tableEntity(), clothMaterial);

        createSceneEntity(
            registry,
            "Saloon Floor",
            makeTransform(glm::vec3(0.0f, -0.46f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(4.8f, 1.0f, 4.8f)),
            StaticMeshComponent{MeshPrimitive::Plane},
            floorMaterial
        );

        createSceneEntity(
            registry,
            "Back Wall",
            makeTransform(
                glm::vec3(0.0f, 1.25f, -2.55f),
                glm::quat(glm::vec3(glm::radians(90.0f), 0.0f, 0.0f)),
                glm::vec3(5.6f, 1.0f, 2.6f)
            ),
            StaticMeshComponent{MeshPrimitive::Plane},
            wallMaterial
        );

        const float frameThickness = 0.16f;
        const float frameHeight = 0.12f;
        const float frameCenterY = -0.055f;

        createSceneEntity(
            registry,
            "North Rail",
            makeTransform(
                glm::vec3(0.0f, frameCenterY, -(halfDepth + 0.5f * frameThickness)),
                glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                glm::vec3(variant.table.clothWidth + 2.0f * frameThickness, frameHeight, frameThickness)
            ),
            StaticMeshComponent{MeshPrimitive::Cube},
            railMaterial
        );

        createSceneEntity(
            registry,
            "South Rail",
            makeTransform(
                glm::vec3(0.0f, frameCenterY, halfDepth + 0.5f * frameThickness),
                glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                glm::vec3(variant.table.clothWidth + 2.0f * frameThickness, frameHeight, frameThickness)
            ),
            StaticMeshComponent{MeshPrimitive::Cube},
            railMaterial
        );

        createSceneEntity(
            registry,
            "West Rail",
            makeTransform(
                glm::vec3(-(halfWidth + 0.5f * frameThickness), frameCenterY, 0.0f),
                glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                glm::vec3(frameThickness, frameHeight, variant.table.clothDepth)
            ),
            StaticMeshComponent{MeshPrimitive::Cube},
            railMaterial
        );

        createSceneEntity(
            registry,
            "East Rail",
            makeTransform(
                glm::vec3(halfWidth + 0.5f * frameThickness, frameCenterY, 0.0f),
                glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                glm::vec3(frameThickness, frameHeight, variant.table.clothDepth)
            ),
            StaticMeshComponent{MeshPrimitive::Cube},
            railMaterial
        );

        const float legOffsetX = halfWidth - 0.22f;
        const float legOffsetZ = halfDepth - 0.15f;
        const glm::vec3 legScale(0.16f, 0.84f, 0.16f);

        createSceneEntity(
            registry,
            "North West Leg",
            makeTransform(glm::vec3(-legOffsetX, -0.47f, -legOffsetZ), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), legScale),
            StaticMeshComponent{MeshPrimitive::Cube},
            legMaterial
        );

        createSceneEntity(
            registry,
            "North East Leg",
            makeTransform(glm::vec3(legOffsetX, -0.47f, -legOffsetZ), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), legScale),
            StaticMeshComponent{MeshPrimitive::Cube},
            legMaterial
        );

        createSceneEntity(
            registry,
            "South West Leg",
            makeTransform(glm::vec3(-legOffsetX, -0.47f, legOffsetZ), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), legScale),
            StaticMeshComponent{MeshPrimitive::Cube},
            legMaterial
        );

        createSceneEntity(
            registry,
            "South East Leg",
            makeTransform(glm::vec3(legOffsetX, -0.47f, legOffsetZ), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), legScale),
            StaticMeshComponent{MeshPrimitive::Cube},
            legMaterial
        );

        for (std::size_t lightIndex = 0; lightIndex < lightRig.positions.size(); ++lightIndex)
        {
            createSceneEntity(
                registry,
                "Lamp Globe " + std::to_string(lightIndex + 1),
                makeTransform(
                    lightRig.positions[lightIndex],
                    glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                    glm::vec3(1.75f)
                ),
                StaticMeshComponent{MeshPrimitive::Sphere},
                lampMaterial
            );
        }

        // Dark wells where the simulator's pockets are, drawn on top of the
        // prototype's solid rails until the hall-visuals milestone models real pockets.
        const MaterialComponent pocketMaterial{
            .albedo = glm::vec3(0.015f, 0.015f, 0.017f),
            .specularStrength = 0.02f,
            .shininess = 4.0f,
            .surfaceType = MaterialSurfaceType::Generic,
            .roughness = 1.0f,
            .reflectivity = 0.0f,
            .clearcoatStrength = 0.0f,
            .emissionColor = glm::vec3(0.0f),
            .emissionIntensity = 0.0f
        };
        const Sim::Table pockets = Sim::buildPocketTable(variant.table.pocketGeometry);
        for (const Sim::Pocket& pocket : pockets.pockets)
        {
            const glm::vec3 center = SimBridge::toGamePosition(pocket.center, variant.table.clothWidth, variant.table.clothDepth);
            const float scale = static_cast<float>(pocket.radius) / variant.table.ballRadius;
            createSceneEntity(
                registry,
                "Pocket",
                makeTransform(glm::vec3(center.x, frameCenterY + 0.5f * frameHeight + 0.0015f, center.z), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(scale, 0.02f, scale)),
                StaticMeshComponent{MeshPrimitive::Sphere},
                pocketMaterial
            );
        }

        attachBallVisual(registry, session.cueBallEntity(), variant.cueBall);

        for (std::size_t i = 0; i < session.objectBallEntities().size(); ++i)
        {
            attachBallVisual(registry, session.objectBallEntities()[i], variant.objectBalls[i]);
        }
    }

    Entity createMainCamera(Registry& registry)
    {
        const Entity camera = registry.createEntity();
        registry.emplace<NameComponent>(camera, NameComponent{"Main Camera"});
        registry.emplace<TransformComponent>(
            camera,
            makeTransform(
                glm::vec3(0.0f, 1.14f, 2.10f),
                glm::quat(glm::vec3(glm::radians(-27.0f), 0.0f, 0.0f))
            )
        );
        registry.emplace<CameraComponent>(camera, CameraComponent{
            .verticalFieldOfViewRadians = glm::radians(52.0f),
            .nearPlane = 0.05f,
            .farPlane = 60.0f
        });
        registry.emplace<CameraTagComponent>(camera, CameraTagComponent{});
        return camera;
    }
}
