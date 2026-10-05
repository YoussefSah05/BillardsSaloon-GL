#include "app/overlay_screens.h"

#include "render/ui_overlay.h"
#include "scene/components.h"

#include <glad/gl.h>

#include <algorithm>
#include <string>

namespace BilliardsSaloon
{
    namespace
    {
        const char* shotPhaseLabel(ShotPhase phase)
        {
            switch (phase)
            {
                case ShotPhase::Aiming:
                    return "AIMING";

                case ShotPhase::Charging:
                    return "CHARGING";

                case ShotPhase::BallsInMotion:
                    return "BALLS IN MOTION";
            }

            return "UNKNOWN";
        }

        const char* matchFlowPhaseLabel(MatchFlowPhase phase)
        {
            switch (phase)
            {
                case MatchFlowPhase::BreakShot:
                    return "BREAK SHOT";

                case MatchFlowPhase::TableOpen:
                    return "TABLE OPEN";

                case MatchFlowPhase::GroupsAssigned:
                    return "GROUPS ASSIGNED";

                case MatchFlowPhase::FrameOver:
                    return "FRAME OVER";
            }

            return "UNKNOWN";
        }

        const char* playerTargetGroupLabel(PlayerTargetGroup group)
        {
            switch (group)
            {
                case PlayerTargetGroup::None:
                    return "OPEN";

                case PlayerTargetGroup::Solids:
                    return "SOLIDS";

                case PlayerTargetGroup::Stripes:
                    return "STRIPES";
            }

            return "UNKNOWN";
        }
    }

    void drawGameplayHud(SceneRenderer& renderer, const FrameView& view, const GameplayHudModel& model)
    {
        Shader& shader = renderer.shader();
        Mesh& cube = renderer.cubeMesh();

        const glm::vec3 hudLeft =
            view.position +
            view.forward * 1.10f +
            view.up * 0.50f -
            view.right * 0.82f;

        const MaterialComponent panelMaterial{
            .albedo = glm::vec3(0.05f, 0.045f, 0.040f),
            .specularStrength = 0.04f,
            .shininess = 8.0f,
            .surfaceType = MaterialSurfaceType::Generic,
            .roughness = 0.95f,
            .reflectivity = 0.02f,
            .clearcoatStrength = 0.0f,
            .emissionColor = glm::vec3(0.0f),
            .emissionIntensity = 0.0f
        };

        const MaterialComponent titleTextMaterial{
            .albedo = glm::vec3(0.95f, 0.90f, 0.76f),
            .specularStrength = 0.10f,
            .shininess = 12.0f,
            .surfaceType = MaterialSurfaceType::Generic,
            .roughness = 0.32f,
            .reflectivity = 0.04f,
            .clearcoatStrength = 0.0f,
            .emissionColor = glm::vec3(0.0f),
            .emissionIntensity = 0.0f
        };

        const MaterialComponent bodyTextMaterial{
            .albedo = glm::vec3(0.90f, 0.86f, 0.80f),
            .specularStrength = 0.07f,
            .shininess = 10.0f,
            .surfaceType = MaterialSurfaceType::Generic,
            .roughness = 0.50f,
            .reflectivity = 0.03f,
            .clearcoatStrength = 0.0f,
            .emissionColor = glm::vec3(0.0f),
            .emissionIntensity = 0.0f
        };

        const MaterialComponent accentTextMaterial{
            .albedo = glm::vec3(0.84f, 0.68f, 0.36f),
            .specularStrength = 0.08f,
            .shininess = 12.0f,
            .surfaceType = MaterialSurfaceType::Generic,
            .roughness = 0.42f,
            .reflectivity = 0.03f,
            .clearcoatStrength = 0.0f,
            .emissionColor = glm::vec3(0.0f),
            .emissionIntensity = 0.0f
        };

        const MaterialComponent hintTextMaterial{
            .albedo = glm::vec3(0.74f, 0.67f, 0.58f),
            .specularStrength = 0.04f,
            .shininess = 8.0f,
            .surfaceType = MaterialSurfaceType::Generic,
            .roughness = 0.88f,
            .reflectivity = 0.02f,
            .clearcoatStrength = 0.0f,
            .emissionColor = glm::vec3(0.0f),
            .emissionIntensity = 0.0f
        };

        const MaterialComponent chargeTrackMaterial{
            .albedo = glm::vec3(0.14f, 0.10f, 0.08f),
            .specularStrength = 0.04f,
            .shininess = 8.0f,
            .surfaceType = MaterialSurfaceType::Generic,
            .roughness = 0.88f,
            .reflectivity = 0.02f,
            .clearcoatStrength = 0.0f,
            .emissionColor = glm::vec3(0.0f),
            .emissionIntensity = 0.0f
        };

        const MaterialComponent chargeFillMaterial{
            .albedo = glm::vec3(0.82f, 0.56f, 0.20f),
            .specularStrength = 0.10f,
            .shininess = 14.0f,
            .surfaceType = MaterialSurfaceType::Generic,
            .roughness = 0.34f,
            .reflectivity = 0.04f,
            .clearcoatStrength = 0.0f,
            .emissionColor = glm::vec3(0.0f),
            .emissionIntensity = 0.0f
        };

        const UiTextStyle titleStyle{
            .material = titleTextMaterial,
            .dynamicEmission = glm::vec3(0.030f, 0.020f, 0.008f),
            .cellSize = 0.0085f,
            .depth = 0.010f,
            .alignment = UiTextAlignment::Left
        };

        const UiTextStyle bodyStyle{
            .material = bodyTextMaterial,
            .dynamicEmission = glm::vec3(0.012f, 0.010f, 0.006f),
            .cellSize = 0.0065f,
            .depth = 0.008f,
            .alignment = UiTextAlignment::Left
        };

        const UiTextStyle accentStyle{
            .material = accentTextMaterial,
            .dynamicEmission = glm::vec3(0.022f, 0.014f, 0.006f),
            .cellSize = 0.0065f,
            .depth = 0.008f,
            .alignment = UiTextAlignment::Left
        };

        const UiTextStyle hintStyle{
            .material = hintTextMaterial,
            .dynamicEmission = glm::vec3(0.010f, 0.008f, 0.004f),
            .cellSize = 0.0055f,
            .depth = 0.007f,
            .alignment = UiTextAlignment::Left
        };

        const int activePlayerIndex =
            std::clamp(model.activePlayerIndex, 0, 1);

        const std::string disciplineLine =
            model.disciplineName;
        const std::string playerLine =
            "PLAYER " + std::to_string(activePlayerIndex + 1);
        const std::string phaseLine =
            std::string("PHASE ") + shotPhaseLabel(model.shotPhase);
        const std::string flowLine =
            std::string("TABLE ") + matchFlowPhaseLabel(model.flowPhase);
        const std::string groupLine =
            std::string("GROUP ") +
            playerTargetGroupLabel(model.activePlayerGroup);
        const std::string qualityLine =
            std::string("QUALITY ") + renderQualityLabel(model.quality);
        const std::string cameraLine =
            std::string("CAMERA ") + cameraViewModeLabel(model.cameraMode);

        std::string statusLine = "STATUS READY";
        if (model.foulCommitted)
        {
            statusLine = "FOUL RECORDED";
        }

        if (model.ballInHand)
        {
            statusLine = "BALL IN HAND";
        }

        if (model.winnerPlayerIndex >= 0)
        {
            statusLine =
                "WINNER PLAYER " +
                std::to_string(model.winnerPlayerIndex + 1);
        }

        const float panelWidth = 0.96f;
        const float chargeBarWidth = 0.60f;
        const float clampedCharge = std::clamp(model.charge01, 0.0f, 1.0f);
        const float chargeFillWidth = chargeBarWidth * clampedCharge;
        const glm::vec3 panelCenter =
            hudLeft +
            view.right * 0.44f -
            view.up * 0.12f +
            view.forward * 0.10f;

        glDisable(GL_DEPTH_TEST);

        renderUiOverlayBox(
            shader,
            cube,
            view.overlayFrame,
            panelCenter,
            glm::vec3(panelWidth, 0.40f, 0.020f),
            panelMaterial,
            glm::vec3(0.010f, 0.008f, 0.008f)
        );

        renderUiOverlayText(
            shader,
            cube,
            view.overlayFrame,
            disciplineLine,
            hudLeft + view.up * 0.02f + view.forward * 0.020f,
            titleStyle
        );

        renderUiOverlayText(
            shader,
            cube,
            view.overlayFrame,
            playerLine,
            hudLeft - view.up * 0.05f + view.forward * 0.020f,
            accentStyle
        );

        renderUiOverlayText(
            shader,
            cube,
            view.overlayFrame,
            phaseLine,
            hudLeft - view.up * 0.12f + view.forward * 0.020f,
            bodyStyle
        );

        renderUiOverlayText(
            shader,
            cube,
            view.overlayFrame,
            flowLine,
            hudLeft - view.up * 0.18f + view.forward * 0.020f,
            bodyStyle
        );

        renderUiOverlayText(
            shader,
            cube,
            view.overlayFrame,
            groupLine,
            hudLeft - view.up * 0.24f + view.forward * 0.020f,
            bodyStyle
        );

        renderUiOverlayText(
            shader,
            cube,
            view.overlayFrame,
            statusLine,
            hudLeft - view.up * 0.30f + view.forward * 0.020f,
            bodyStyle
        );

        renderUiOverlayText(
            shader,
            cube,
            view.overlayFrame,
            qualityLine,
            hudLeft + view.right * 0.52f - view.up * 0.05f + view.forward * 0.020f,
            hintStyle
        );

        renderUiOverlayText(
            shader,
            cube,
            view.overlayFrame,
            cameraLine,
            hudLeft + view.right * 0.52f - view.up * 0.11f + view.forward * 0.020f,
            hintStyle
        );

        const glm::vec3 chargeTrackCenter =
            hudLeft + view.right * 0.35f - view.up * 0.37f + view.forward * 0.020f;

        renderUiOverlayText(
            shader,
            cube,
            view.overlayFrame,
            "CHARGE",
            hudLeft - view.up * 0.37f + view.forward * 0.020f,
            hintStyle
        );

        renderUiOverlayBox(
            shader,
            cube,
            view.overlayFrame,
            chargeTrackCenter,
            glm::vec3(chargeBarWidth, 0.026f, 0.012f),
            chargeTrackMaterial,
            glm::vec3(0.0f)
        );

        if (chargeFillWidth > 0.0f)
        {
            renderUiOverlayBox(
                shader,
                cube,
                view.overlayFrame,
                chargeTrackCenter - view.right * (0.5f * (chargeBarWidth - chargeFillWidth)),
                glm::vec3(chargeFillWidth, 0.018f, 0.010f),
                chargeFillMaterial,
                glm::vec3(0.040f, 0.020f, 0.006f) * clampedCharge
            );
        }

        // One hint per line: two columns overlapped at this text size.
        const char* const freeLookHints[] = {
            "RMB DRAG ORBIT - WHEEL ZOOM",
            "JL ORBIT - IK TILT - UO ZOOM",
            "TAB OR 1-4 CAMERA",
            "ESC PAUSE - F11 FULLSCREEN"
        };
        const char* const aimingHints[] = {
            "MOUSE OR A/D AIM - SHIFT FINE",
            "LMB DRAG BACK SHOOT - SPACE",
            "RMB DRAG OR ARROWS SPIN",
            "ESC PAUSE - F11 FULLSCREEN"
        };
        const bool freeLook = (model.cameraMode == CameraViewMode::FreeLook);

        for (int line = 0; line < 4; ++line)
        {
            renderUiOverlayText(
                shader,
                cube,
                view.overlayFrame,
                freeLook ? freeLookHints[line] : aimingHints[line],
                hudLeft - view.up * (0.46f + 0.07f * static_cast<float>(line)) + view.forward * 0.020f,
                hintStyle
            );
        }

        glEnable(GL_DEPTH_TEST);
    }
}
