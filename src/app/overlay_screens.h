#pragma once

#include "gameplay/match_state.h"
#include "gameplay/shot_state.h"
#include "render/camera_rig.h"
#include "render/scene_renderer.h"

#include <string>
#include <vector>

namespace BilliardsSaloon
{
    // Everything the in-game HUD shows, gathered by the application each frame.
    struct GameplayHudModel
    {
        std::string disciplineName;
        int activePlayerIndex {0};
        PlayerTargetGroup activePlayerGroup {PlayerTargetGroup::None};
        ShotPhase shotPhase {ShotPhase::Aiming};
        MatchFlowPhase flowPhase {MatchFlowPhase::BreakShot};
        bool foulCommitted {false};
        bool ballInHand {false};
        int winnerPlayerIndex {-1};   // >= 0 once the frame is over
        float charge01 {0.0f};
        RenderQualityPreset quality {RenderQualityPreset::Balanced};
        CameraViewMode cameraMode {CameraViewMode::PlayerAim};
    };

    // A vertical list menu (main menu, pause menu).
    struct MenuScreenModel
    {
        std::string title;
        std::vector<std::string> entries;
        std::size_t selectedIndex {0};
        std::vector<std::string> hints;

        // Layout, in camera-space metres relative to the menu centre.
        float titleOffset {0.34f};
        float titleBoxWidth {0.52f};
        glm::vec3 titleBoxEmission {0.03f, 0.02f, 0.01f};
        float firstEntryOffset {0.12f};
        float entrySpacing {0.22f};
    };

    // Prototype voxel-text overlays; drawn without depth testing.
    void drawGameplayHud(SceneRenderer& renderer, const FrameView& view, const GameplayHudModel& model);
    void drawMenuScreen(SceneRenderer& renderer, const FrameView& view, const MenuScreenModel& model);
}
