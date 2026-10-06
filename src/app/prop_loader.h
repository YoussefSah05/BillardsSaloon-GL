#pragma once

#include "ecs/entity.h"
#include "scene/model_loader.h"
#include "scene/prop_layout.h"

#include <future>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace BilliardsSaloon
{
    class MatchSession;

    // Streams the hall's furniture and decor: each model is loaded and its
    // levels of detail built on a worker thread, then placed in the scene as
    // it arrives, so the game never waits for them. Models stay cached, so a
    // new match or a return to a hall does not load them again.
    class PropLoader
    {
    public:
        PropLoader() = default;
        ~PropLoader();
        PropLoader(const PropLoader&) = delete;
        PropLoader& operator=(const PropLoader&) = delete;

        // Call once a frame: starts loading when the hall changed (a new
        // layout or a rebuilt scene) and adds the props that are ready.
        void update(MatchSession& session);

    private:
        struct Pending
        {
            std::string model;
            std::vector<PropPlacement> placements;
            std::shared_future<std::shared_ptr<const ModelData>> future;
        };

        void request(MatchSession& session, int layout);
        void place(MatchSession& session, const ModelData& model, const std::vector<PropPlacement>& placements);

        const MatchSession* m_session {nullptr};   // a new match has a new registry
        Entity m_volume {};          // the hall's probe volume when the props were requested
        bool m_requested {false};
        int m_layout {-1};
        std::vector<Pending> m_pending;
        std::map<std::string, std::shared_future<std::shared_ptr<const ModelData>>> m_cache;
    };
}
