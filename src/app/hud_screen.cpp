#include "app/hud_screen.h"

#include "ui/ui_system.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace BilliardsSaloon
{
    namespace
    {
        constexpr float BANNER_SECONDS = 2.6f;
        constexpr float LOWER_THIRD_SECONDS = 2.8f;

        // Spin widget geometry, in dp: a 96 dp ball with an 18 dp tip marker.
        constexpr float SPIN_BALL_CENTER = 48.0f;
        constexpr float SPIN_TIP_HALF = 9.0f;
        constexpr float SPIN_TRAVEL = 0.36f * 96.0f;

        std::string hexColor(const glm::vec3& color)
        {
            auto channel = [](float value)
            {
                return static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
            };

            char buffer[8];
            std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", channel(color.r), channel(color.g), channel(color.b));
            return buffer;
        }

        std::string dp(float value)
        {
            char buffer[24];
            std::snprintf(buffer, sizeof(buffer), "%.1fdp", static_cast<double>(value));
            return buffer;
        }

        std::string percent(float value01)
        {
            char buffer[16];
            std::snprintf(buffer, sizeof(buffer), "%.1f%%", static_cast<double>(std::clamp(value01, 0.0f, 1.0f) * 100.0f));
            return buffer;
        }

        const char* groupLabel(PlayerTargetGroup group)
        {
            switch (group)
            {
                case PlayerTargetGroup::Solids:
                    return "SOLIDS";
                case PlayerTargetGroup::Stripes:
                    return "STRIPES";
                case PlayerTargetGroup::None:
                    return "OPEN TABLE";
            }
            return "";
        }

        const char* foulText(FoulReason reason)
        {
            switch (reason)
            {
                case FoulReason::CueBallPocketed:
                    return "CUE BALL SCRATCHED";
                case FoulReason::NoBallHit:
                    return "NO BALL HIT";
                case FoulReason::WrongBallFirst:
                    return "WRONG BALL HIT FIRST";
                case FoulReason::None:
                    return "";
            }
            return "";
        }

        // Updates a bound variable and marks it dirty only when it changes.
        template <typename T>
        void assign(Rml::DataModelHandle& model, T& member, const T& value, const char* name)
        {
            if (member != value)
            {
                member = value;
                model.DirtyVariable(name);
            }
        }
    }

    HudScreen::HudScreen(UiSystem& ui, const GameVariantDefinition& variant)
    {
        auto styleFor = [](const BallSpawnDefinition& definition)
        {
            HudBall ball;
            ball.color = hexColor(definition.albedo);
            ball.stripe = definition.ruleTag == BallRuleTag::Stripe;
            ball.fill = ball.stripe ? "#F7F4EC" : ball.color;
            return ball;
        };
        for (const BallSpawnDefinition& definition : variant.objectBalls)
        {
            if ((definition.number >= 0) && (definition.number < static_cast<int>(m_ballStyles.size())))
            {
                m_ballStyles[static_cast<std::size_t>(definition.number)] = styleFor(definition);
            }
        }

        Rml::DataModelConstructor model = ui.context().CreateDataModel("hud");
        if (!model)
        {
            throw std::runtime_error("Could not create the HUD data model.");
        }

        if (Rml::StructHandle<HudBall> ball = model.RegisterStruct<HudBall>())
        {
            ball.RegisterMember("fill", &HudBall::fill);
            ball.RegisterMember("color", &HudBall::color);
            ball.RegisterMember("stripe", &HudBall::stripe);
        }
        model.RegisterArray<std::vector<HudBall>>();

        model.Bind("p1_name", &m_names[0]);
        model.Bind("p2_name", &m_names[1]);
        model.Bind("p1_group", &m_groupLabels[0]);
        model.Bind("p2_group", &m_groupLabels[1]);
        model.Bind("tray1", &m_trays[0]);
        model.Bind("tray2", &m_trays[1]);
        model.Bind("groups_assigned", &m_groupsAssigned);
        model.Bind("active", &m_active);
        model.Bind("discipline", &m_discipline);
        model.Bind("camera_label", &m_cameraLabel);
        model.Bind("aiming", &m_aiming);
        model.Bind("gamepad", &m_gamepad);

        model.BindFunc("power_height", [this](Rml::Variant& value) { value = percent(m_power); });
        model.BindFunc("power_text", [this](Rml::Variant& value)
        {
            value = std::to_string(static_cast<int>(std::lround(m_power * 100.0f))) + "%";
        });
        model.BindFunc("last_power_bottom", [this](Rml::Variant& value) { value = percent(m_lastPower); });
        model.BindFunc("tip_left", [this](Rml::Variant& value)
        {
            value = dp(SPIN_BALL_CENTER - SPIN_TIP_HALF + m_strikeRight * SPIN_TRAVEL);
        });
        model.BindFunc("tip_top", [this](Rml::Variant& value)
        {
            value = dp(SPIN_BALL_CENTER - SPIN_TIP_HALF - m_strikeForward * SPIN_TRAVEL);
        });
        model.BindFunc("spin_name", [this](Rml::Variant& value)
        {
            const char* vertical = (m_strikeForward > 0.15f) ? "HIGH" : (m_strikeForward < -0.15f) ? "LOW" : "";
            const char* side = (m_strikeRight > 0.15f) ? "RIGHT" : (m_strikeRight < -0.15f) ? "LEFT" : "";
            std::string name = vertical;
            if (*side != '\0')
            {
                name += name.empty() ? side : std::string(" ") + side;
            }
            value = name.empty() ? std::string("CENTRE") : name;
        });
        model.BindFunc("spin_hint", [this](Rml::Variant& value)
        {
            std::string hint = (m_strikeForward > 0.15f) ? "Follow" : (m_strikeForward < -0.15f) ? "Draw" : "Stun";
            if (m_strikeRight > 0.15f)
            {
                hint += " + right english";
            }
            else if (m_strikeRight < -0.15f)
            {
                hint += " + left english";
            }
            value = hint;
        });

        model.Bind("banner_visible", &m_bannerVisible);
        model.Bind("banner_foul", &m_bannerFoul);
        model.Bind("banner_legal", &m_bannerLegal);
        model.Bind("banner_kind", &m_bannerKind);
        model.Bind("banner_text", &m_bannerText);
        model.Bind("lower_visible", &m_lowerVisible);
        model.Bind("lower_tag", &m_lowerTag);
        model.Bind("lower_title", &m_lowerTitle);
        model.Bind("lower_sub", &m_lowerSub);

        m_model = model.GetModelHandle();
        m_document = &ui.loadDocument("ui/hud.rml");
    }

    void HudScreen::setVisible(bool visible)
    {
        if (visible == m_document->IsVisible())
        {
            return;
        }

        if (visible)
        {
            // The HUD never takes keyboard focus away from the game.
            m_document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        }
        else
        {
            m_document->Hide();
        }
    }

    void HudScreen::setTray(int player, const std::vector<int>& numbers)
    {
        const std::size_t index = static_cast<std::size_t>(player);
        if (m_trayNumbers[index] == numbers)
        {
            return;
        }

        m_trayNumbers[index] = numbers;
        m_trays[index].clear();
        for (const int number : numbers)
        {
            if ((number >= 0) && (number < static_cast<int>(m_ballStyles.size())))
            {
                m_trays[index].push_back(m_ballStyles[static_cast<std::size_t>(number)]);
            }
        }
        m_model.DirtyVariable(player == 0 ? "tray1" : "tray2");
    }

    void HudScreen::update(const HudSnapshot& snapshot, float deltaTimeSeconds)
    {
        assign(m_model, m_names[0], snapshot.playerNames[0], "p1_name");
        assign(m_model, m_names[1], snapshot.playerNames[1], "p2_name");
        assign(m_model, m_groupLabels[0], std::string(groupLabel(snapshot.groups[0])), "p1_group");
        assign(m_model, m_groupLabels[1], std::string(groupLabel(snapshot.groups[1])), "p2_group");
        assign(m_model, m_groupsAssigned, snapshot.groups[0] != PlayerTargetGroup::None, "groups_assigned");
        assign(m_model, m_active, snapshot.activePlayer, "active");
        assign(m_model, m_discipline, snapshot.discipline, "discipline");
        assign(m_model, m_cameraLabel, snapshot.cameraLabel, "camera_label");
        assign(m_model, m_aiming, snapshot.aiming, "aiming");
        assign(m_model, m_gamepad, snapshot.gamepadPrompts, "gamepad");
        setTray(0, snapshot.remainingBalls[0]);
        setTray(1, snapshot.remainingBalls[1]);

        if (m_power != snapshot.power01)
        {
            m_power = snapshot.power01;
            m_model.DirtyVariable("power_height");
            m_model.DirtyVariable("power_text");
        }

        if ((m_strikeRight != snapshot.strikeRight01) || (m_strikeForward != snapshot.strikeForward01))
        {
            m_strikeRight = snapshot.strikeRight01;
            m_strikeForward = snapshot.strikeForward01;
            m_model.DirtyVariable("tip_left");
            m_model.DirtyVariable("tip_top");
            m_model.DirtyVariable("spin_name");
            m_model.DirtyVariable("spin_hint");
        }

        if (m_bannerVisible)
        {
            m_bannerTimer -= deltaTimeSeconds;
            if (m_bannerTimer <= 0.0f)
            {
                assign(m_model, m_bannerVisible, false, "banner_visible");
            }
        }

        if (m_lowerVisible)
        {
            m_lowerTimer -= deltaTimeSeconds;
            if (m_lowerTimer <= 0.0f)
            {
                assign(m_model, m_lowerVisible, false, "lower_visible");
            }
        }
    }

    void HudScreen::markShotPower(float power01)
    {
        if (m_lastPower != power01)
        {
            m_lastPower = power01;
            m_model.DirtyVariable("last_power_bottom");
        }
    }

    void HudScreen::showBanner(const char* kind, const std::string& text, bool foul, bool legal, float seconds)
    {
        assign(m_model, m_bannerKind, std::string(kind), "banner_kind");
        assign(m_model, m_bannerText, text, "banner_text");
        assign(m_model, m_bannerFoul, foul, "banner_foul");
        assign(m_model, m_bannerLegal, legal, "banner_legal");
        assign(m_model, m_bannerVisible, true, "banner_visible");
        m_bannerTimer = seconds;
    }

    void HudScreen::showLowerThird(const std::string& tag, const std::string& title, const std::string& sub, float seconds)
    {
        assign(m_model, m_lowerTag, tag, "lower_tag");
        assign(m_model, m_lowerTitle, title, "lower_title");
        assign(m_model, m_lowerSub, sub, "lower_sub");
        assign(m_model, m_lowerVisible, true, "lower_visible");
        m_lowerTimer = seconds;
    }

    void HudScreen::announce(const ShotOutcome& outcome, const HudSnapshot& snapshot)
    {
        const auto name = [&](int player) { return snapshot.playerNames[static_cast<std::size_t>(player)]; };

        if (outcome.frameOver)
        {
            const bool legal = outcome.frameEnd == FrameEndReason::EightBallPotted;
            const std::string how =
                legal ? name(outcome.shooter) + " POTS THE 8"
                : (outcome.frameEnd == FrameEndReason::EightBallPottedOnFoul) ? "8-BALL POTTED ON A FOUL"
                : "8-BALL POTTED EARLY";
            showBanner("FRAME", how, !legal, legal, BANNER_SECONDS);
            return;
        }

        if (outcome.foul != FoulReason::None)
        {
            showBanner("FOUL", foulText(outcome.foul), true, false, BANNER_SECONDS);

            const std::size_t next = static_cast<std::size_t>(outcome.nextPlayer);
            showLowerThird(
                "BALL IN HAND",
                name(outcome.nextPlayer) + " · " + groupLabel(snapshot.groups[next]),
                "The cue ball goes back to the head spot",
                LOWER_THIRD_SECONDS
            );
            return;
        }

        if (outcome.groupsAssigned)
        {
            const std::size_t shooter = static_cast<std::size_t>(outcome.shooter);
            showBanner("GROUPS", name(outcome.shooter) + " · " + groupLabel(snapshot.groups[shooter]), false, true, BANNER_SECONDS);
            return;
        }

        if (outcome.turnPassed)
        {
            const std::size_t next = static_cast<std::size_t>(outcome.nextPlayer);
            const std::size_t left = snapshot.remainingBalls[next].size();
            const std::string sub =
                (snapshot.groups[next] == PlayerTargetGroup::None)
                    ? std::string("Open table")
                    : std::to_string(left) + (left == 1 ? " ball left" : " balls left") + " before the 8";
            showLowerThird("AT THE TABLE", name(outcome.nextPlayer), sub, LOWER_THIRD_SECONDS);
        }
    }
}
