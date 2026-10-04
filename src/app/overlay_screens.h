#pragma once

#include "gameplay/match_state.h"
#include "gameplay/shot_state.h"
#include "render/camera_rig.h"
#include "render/scene_renderer.h"

#include <algorithm>
#include <optional>
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

    // Menu placement in front of the camera, shared by drawing and mouse
    // hit testing so the two can never disagree.
    inline constexpr glm::vec3 MENU_CARD_SCALE {0.72f, 0.08f, 0.035f};

    [[nodiscard]] inline glm::vec3 menuOrigin(const FrameView& view)
    {
        return view.position + view.forward * 1.45f + view.up * 0.08f;
    }

    [[nodiscard]] inline glm::vec3 menuCardCenter(const FrameView& view, const MenuScreenModel& model, std::size_t index)
    {
        return menuOrigin(view) + view.up * (model.firstEntryOffset - model.entrySpacing * static_cast<float>(index));
    }

    // The menu entry under a cursor given in normalized device coordinates.
    [[nodiscard]] inline std::optional<std::size_t> menuEntryAt(
        const FrameView& view,
        const MenuScreenModel& model,
        const glm::vec2& cursorNdc)
    {
        const glm::vec3 halfRight = view.right * (0.5f * MENU_CARD_SCALE.x);
        const glm::vec3 halfUp = view.up * (0.5f * MENU_CARD_SCALE.y);

        for (std::size_t i = 0; i < model.entries.size(); ++i)
        {
            const glm::vec3 center = menuCardCenter(view, model, i);
            glm::vec2 lower;
            glm::vec2 upper;
            if (!projectToNdc(view, center - halfRight - halfUp, lower) ||
                !projectToNdc(view, center + halfRight + halfUp, upper))
            {
                continue;
            }

            const glm::vec2 minCorner = glm::min(lower, upper);
            const glm::vec2 maxCorner = glm::max(lower, upper);
            if (glm::all(glm::greaterThanEqual(cursorNdc, minCorner)) &&
                glm::all(glm::lessThanEqual(cursorNdc, maxCorner)))
            {
                return i;
            }
        }

        return std::nullopt;
    }

    // Prototype voxel-text overlays; drawn without depth testing.
    void drawGameplayHud(SceneRenderer& renderer, const FrameView& view, const GameplayHudModel& model);
    void drawMenuScreen(SceneRenderer& renderer, const FrameView& view, const MenuScreenModel& model);
}
