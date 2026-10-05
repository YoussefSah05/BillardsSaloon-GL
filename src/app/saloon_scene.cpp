#include "app/saloon_scene.h"

#include "core/settings.h"
#include "gameplay/equipment.h"
#include "gameplay/match_session.h"
#include "gameplay/sim_bridge.h"
#include "sim/table.h"
#include "render/camera.h"
#include "scene/table_geometry.h"
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

        StaticMeshComponent primitive(MeshPrimitive shape)
        {
            return StaticMeshComponent{shape, nullptr, true};
        }

        StaticMeshComponent custom(MeshData data, bool castsShadow = true)
        {
            return StaticMeshComponent{MeshPrimitive::Custom, std::make_shared<const MeshData>(std::move(data)), castsShadow};
        }

        MaterialComponent material(const glm::vec3& albedo, MaterialSurfaceType surface, float roughness, float reflectivity, float clearcoat)
        {
            return MaterialComponent{
                .albedo = albedo,
                .specularStrength = 0.5f,
                .shininess = 32.0f,
                .surfaceType = surface,
                .roughness = roughness,
                .reflectivity = reflectivity,
                .clearcoatStrength = clearcoat,
                .emissionColor = glm::vec3(0.0f),
                .emissionIntensity = 0.0f
            };
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
            registry.emplace<StaticMeshComponent>(ball, primitive(MeshPrimitive::Sphere));
            registry.emplace<MaterialComponent>(ball, ballMaterial(definition));
        }
    }

    PointLightRig saloonLightRig(const glm::vec3& lampColor)
    {
        PointLightRig rig;
        rig.positions = {
            glm::vec3(-0.82f, 1.58f, -0.04f),
            glm::vec3( 0.00f, 1.66f,  0.00f),
            glm::vec3( 0.82f, 1.58f, -0.04f)
        };
        // The centre lamp is a little stronger; the hall's intensity scales all three.
        rig.colors = {lampColor, lampColor * 1.18f, lampColor};
        return rig;
    }

    void buildSaloonScene(MatchSession& session)
    {
        Registry& registry = session.registry();
        const GameVariantDefinition& variant = session.variant();

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

        const TableStyle style;
        const TableGeometry table = buildTableGeometry(Sim::buildPocketTable(variant.table.pocketGeometry), style);

        // The cloth is drawn on the session's table entity.
        registry.emplace<NameComponent>(session.tableEntity(), NameComponent{"Table Cloth"});
        registry.emplace<TransformComponent>(session.tableEntity(), makeTransform(glm::vec3(0.0f)));
        registry.emplace<StaticMeshComponent>(session.tableEntity(), primitive(MeshPrimitive::Plane));
        registry.emplace<MaterialComponent>(session.tableEntity(), clothMaterial);

        // A tournament table: cushions from the simulator's own geometry,
        // dark wood rails with sights, leather pocket rims, a metal trim
        // line, a slim apron and square legs. Original design, no brand.
        MaterialComponent cushionMaterial = clothMaterial;
        cushionMaterial.albedo *= 0.92f;
        const MaterialComponent tableWood = material(glm::vec3(0.20f, 0.10f, 0.055f), MaterialSurfaceType::Wood, 0.40f, 0.035f, 0.06f);
        const MaterialComponent leather = material(glm::vec3(0.035f, 0.032f, 0.030f), MaterialSurfaceType::Generic, 0.42f, 0.04f, 0.15f);
        const MaterialComponent pocketDark = material(glm::vec3(0.012f, 0.012f, 0.014f), MaterialSurfaceType::Generic, 0.9f, 0.02f, 0.0f);
        const MaterialComponent pearl = material(glm::vec3(0.93f, 0.91f, 0.86f), MaterialSurfaceType::Generic, 0.12f, 0.08f, 0.6f);
        const MaterialComponent aluminium = material(glm::vec3(0.78f, 0.78f, 0.80f), MaterialSurfaceType::Generic, 0.28f, 0.55f, 0.0f);
        const MaterialComponent apronFinish = material(glm::vec3(0.05f, 0.045f, 0.045f), MaterialSurfaceType::Generic, 0.35f, 0.04f, 0.3f);

        const TransformComponent atOrigin = makeTransform(glm::vec3(0.0f));
        createSceneEntity(registry, "Cushions", atOrigin, custom(table.cushions), cushionMaterial);
        createSceneEntity(registry, "Rails", atOrigin, custom(table.rails), tableWood);
        createSceneEntity(registry, "Pocket Rims", atOrigin, custom(table.pocketRims, false), leather);
        createSceneEntity(registry, "Pocket Drops", atOrigin, custom(table.pocketCups, false), pocketDark);
        createSceneEntity(registry, "Sights", atOrigin, custom(table.diamonds, false), pearl);
        createSceneEntity(registry, "Trim", atOrigin, custom(table.trim), aluminium);
        createSceneEntity(registry, "Apron", atOrigin, custom(table.apron), apronFinish);
        createSceneEntity(registry, "Legs", atOrigin, custom(table.legs), apronFinish);

        createSceneEntity(
            registry,
            "Saloon Floor",
            makeTransform(glm::vec3(0.0f, style.floorY, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(4.8f, 1.0f, 4.8f)),
            primitive(MeshPrimitive::Plane),
            floorMaterial
        );

        createSceneEntity(
            registry,
            "Back Wall",
            makeTransform(
                glm::vec3(0.0f, style.floorY + 1.6f, -2.55f),
                glm::quat(glm::vec3(glm::radians(90.0f), 0.0f, 0.0f)),
                glm::vec3(5.6f, 1.0f, 2.6f)
            ),
            primitive(MeshPrimitive::Plane),
            wallMaterial
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
                primitive(MeshPrimitive::Sphere),
                lampMaterial
            );
        }

        attachBallVisual(registry, session.cueBallEntity(), variant.cueBall);

        for (std::size_t i = 0; i < session.objectBallEntities().size(); ++i)
        {
            attachBallVisual(registry, session.objectBallEntities()[i], variant.objectBalls[i]);
        }
    }

    void applyEquipment(MatchSession& session, const EquipmentChoice& choice)
    {
        const EquipmentCatalog& catalog = equipmentCatalog();
        const FinishOption& cloth = findOption(catalog.cloth, choice.cloth);
        const FinishOption& rails = findOption(catalog.rails, choice.rails);
        const FinishOption& trim = findOption(catalog.trim, choice.trim);
        const FinishOption& pockets = findOption(catalog.pockets, choice.pockets);
        const BallSetOption& balls = findOption(catalog.balls, choice.balls);

        Registry& registry = session.registry();
        registry.view<NameComponent, MaterialComponent>().each(
            [&](Entity, NameComponent& name, MaterialComponent& material)
            {
                if (name.value == "Table Cloth")
                {
                    material.albedo = cloth.color;
                }
                else if (name.value == "Cushions")
                {
                    material.albedo = cloth.color * 0.92f;
                }
                else if (name.value == "Rails")
                {
                    material.albedo = rails.color;
                    material.roughness = rails.roughness;
                    material.clearcoatStrength = rails.clearcoat;
                }
                else if (name.value == "Trim")
                {
                    material.albedo = trim.color;
                    material.roughness = trim.roughness;
                    material.reflectivity = trim.metal;
                }
                else if (name.value == "Pocket Rims")
                {
                    material.albedo = pockets.color;
                }
            });

        // Ball colours: the set's, or the variant's where the set has none.
        const GameVariantDefinition& variant = session.variant();
        for (std::size_t i = 0; i < session.objectBallEntities().size(); ++i)
        {
            const BallSpawnDefinition& definition = variant.objectBalls[i];
            const auto colour = balls.colors.find(definition.number);
            registry.get<MaterialComponent>(session.objectBallEntities()[i]).albedo =
                (colour != balls.colors.end()) ? colour->second : definition.albedo;
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
