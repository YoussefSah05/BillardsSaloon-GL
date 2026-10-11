#include "app/prop_loader.h"

#include "core/asset_paths.h"
#include "gameplay/match_session.h"
#include "scene/components.h"
#include "scene/hall_geometry.h"
#include "scene/table_geometry.h"

#include <glm/gtc/quaternion.hpp>

#include <chrono>
#include <iostream>

namespace Septhuit
{
    PropLoader::~PropLoader() = default;

    void PropLoader::update(MatchSession& session)
    {
        Registry& registry = session.registry();
        Entity volumeEntity {};
        bool found = false;
        int layout = -1;
        registry.view<ProbeVolumeComponent, HallTagComponent>().each(
            [&](Entity entity, ProbeVolumeComponent&, HallTagComponent& tag)
            {
                volumeEntity = entity;
                layout = tag.layout;
                found = true;
            });
        if (!found)
        {
            return;
        }

        // A rebuilt hall (new layout, or a new match's scene) has a new probe
        // volume entity, and none of the props: place them again.
        if (!m_requested || (&session != m_session) || !(volumeEntity == m_volume) || (layout != m_layout))
        {
            m_session = &session;
            m_volume = volumeEntity;
            m_layout = layout;
            m_requested = true;
            request(session, layout);
        }

        for (auto it = m_pending.begin(); it != m_pending.end();)
        {
            if (it->future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            {
                ++it;
                continue;
            }
            try
            {
                place(session, *it->future.get(), it->placements);
            }
            catch (const std::exception& error)
            {
                std::cerr << "Could not load " << it->model << ": " << error.what() << '\n';
            }
            it = m_pending.erase(it);

            // Capture the hall's light once its last prop has arrived.
            ProbeVolumeComponent& volume = registry.get<ProbeVolumeComponent>(volumeEntity);
            volume.pendingModels = static_cast<int>(m_pending.size());
            if (volume.pendingModels == 0)
            {
                ++volume.revision;
            }
        }
    }

    void PropLoader::request(MatchSession& session, int layout)
    {
        m_pending.clear();
        const std::string name = (layout == static_cast<int>(HallLayout::Saloon)) ? "saloon" : "arena";

        std::vector<PropPlacement> placements;
        try
        {
            placements = loadPropLayout(resolveAssetPath("data/halls/props.json"), name);
        }
        catch (const std::exception& error)
        {
            std::cerr << error.what() << '\n';
        }

        // One load per model, shared by all of its placements.
        std::map<std::string, std::vector<PropPlacement>> byModel;
        for (const PropPlacement& placement : placements)
        {
            byModel[placement.model].push_back(placement);
        }
        for (auto& [model, group] : byModel)
        {
            auto cached = m_cache.find(model);
            if (cached == m_cache.end())
            {
                std::filesystem::path path;
                try
                {
                    path = resolveAssetPath(model);
                }
                catch (const std::exception& error)
                {
                    std::cerr << error.what() << '\n';
                    continue;
                }
                auto future = std::async(std::launch::async, [path]()
                {
                    return std::shared_ptr<const ModelData>(std::make_shared<ModelData>(loadModel(path, 3)));
                }).share();
                cached = m_cache.emplace(model, std::move(future)).first;
            }
            m_pending.push_back(Pending{model, std::move(group), cached->second});
        }

        session.registry().get<ProbeVolumeComponent>(m_volume).pendingModels = static_cast<int>(m_pending.size());
    }

    void PropLoader::place(MatchSession& session, const ModelData& model, const std::vector<PropPlacement>& placements)
    {
        Registry& registry = session.registry();
        const float floorY = TableStyle{}.floorY;

        std::vector<std::shared_ptr<const ModelMaterial>> materials;
        for (const ModelMaterial& material : model.materials)
        {
            materials.push_back(std::make_shared<const ModelMaterial>(material));
        }

        for (const PropPlacement& placement : placements)
        {
            TransformComponent transform;
            transform.position = glm::vec3(placement.position.x, floorY + placement.position.y, placement.position.z);
            transform.rotation = glm::angleAxis(glm::radians(placement.yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
            transform.scale = glm::vec3(placement.scale);
            transform.syncPrevious();

            for (const ModelPart& part : model.parts)
            {
                MaterialComponent finish;
                finish.textured = materials[part.material];

                const Entity entity = registry.createEntity();
                registry.emplace<NameComponent>(entity, NameComponent{"Prop " + model.name});
                registry.emplace<TransformComponent>(entity, transform);
                registry.emplace<StaticMeshComponent>(entity, StaticMeshComponent{MeshPrimitive::Custom, part.lods.front(), true});
                registry.emplace<MaterialComponent>(entity, finish);
                registry.emplace<LodComponent>(entity, LodComponent{part.lods});
                registry.emplace<HallTagComponent>(entity, HallTagComponent{m_layout});
            }
        }
    }
}
