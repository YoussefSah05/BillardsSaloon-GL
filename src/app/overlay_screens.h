#pragma once

#include "gameplay/match_state.h"
#include "gameplay/shot_state.h"
#include "render/camera_rig.h"
#include "render/scene_renderer.h"

#include <string>

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

    // Prototype voxel-text HUD; drawn without depth testing. Replaced by RmlUi later in M2.
    void drawGameplayHud(SceneRenderer& renderer, const FrameView& view, const GameplayHudModel& model);
}
