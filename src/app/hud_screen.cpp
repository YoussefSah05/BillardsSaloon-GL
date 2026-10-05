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

        const char* groupLabel(Rules::Group group)
        {
            switch (group)
            {
                case Rules::Group::Solids:
                    return "SOLIDS";
                case Rules::Group::Stripes:
                    return "STRIPES";
                case Rules::Group::None:
                    return "OPEN TABLE";
            }
            return "";
        }

        const char* foulText(Rules::Foul foul)
        {
            switch (foul)
            {
                case Rules::Foul::CueBallPocketed:
                    return "CUE BALL SCRATCHED";
                case Rules::Foul::NoBallHit:
                    return "NO BALL HIT";
                case Rules::Foul::WrongBallFirst:
                    return "WRONG BALL HIT FIRST";
                case Rules::Foul::NoRail:
                    return "NO CUSHION AFTER CONTACT";
                case Rules::Foul::IllegalBreak:
                    return "ILLEGAL BREAK";
                case Rules::Foul::TimeOut:
                    return "SHOT CLOCK EXPIRED";
                case Rules::Foul::None:
                    return "";
            }
            return "";
        }

        std::string foulsLabel(int fouls)
        {
            return (fouls <= 0) ? std::string() : (fouls == 1) ? std::string("1 FOUL") : std::to_string(fouls) + " FOULS";
        }

        std::string ballList(const std::vector<int>& numbers)
        {
            std::string text;
            for (const int number : numbers)
            {
                text += (text.empty() ? "" : ", ") + std::to_string(number);
            }
            return text;
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

    std::string pocketName(const glm::vec3& pocketPosition)
    {
        // Seen from the head end, looking down the table (+x) towards the rack.
        const char* end = (pocketPosition.x > 0.3f) ? "FOOT" : (pocketPosition.x < -0.3f) ? "HEAD" : "SIDE";
        const char* side = (pocketPosition.z < 0.0f) ? "RIGHT" : "LEFT";
        return std::string(end) + " " + side;
    }

    HudScreen::HudScreen(UiSystem& ui, const GameVariantDefinition& variant)
    {
        setVariant(variant);

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
            ball.RegisterMember("on", &HudBall::on);
        }
        model.RegisterArray<std::vector<HudBall>>();

        model.Bind("p1_name", &m_names[0]);
        model.Bind("p2_name", &m_names[1]);
        model.Bind("p1_group", &m_groupLabels[0]);
        model.Bind("p2_group", &m_groupLabels[1]);
        model.Bind("tray1", &m_trays[0]);
        model.Bind("tray2", &m_trays[1]);
        model.Bind("tray_table", &m_trays[2]);
        model.Bind("groups_assigned", &m_groupsAssigned);
        model.Bind("rotation", &m_rotation);
        model.Bind("show_frames", &m_showFrames);
        model.Bind("p1_frames", &m_frames[0]);
        model.Bind("p2_frames", &m_frames[1]);
        model.Bind("race", &m_race);
        model.Bind("placing", &m_placing);
        model.Bind("placement_valid", &m_placementValid);
        model.Bind("can_place", &m_canPlace);
        model.Bind("place_text", &m_placeText);
        model.Bind("calling", &m_calling);
        model.Bind("call_text", &m_callText);
        model.Bind("push_available", &m_pushAvailable);
        model.Bind("push_declared", &m_pushDeclared);
        model.Bind("replaying", &m_replaying);
        model.Bind("clock_visible", &m_clockVisible);
        model.Bind("clock_low", &m_clockLow);
        model.Bind("clock_extend", &m_clockExtend);
        model.Bind("clock_text", &m_clockText);
        model.Bind("ext1", &m_extension[0]);
        model.Bind("ext2", &m_extension[1]);
        model.Bind("replay_slow", &m_replaySlow);
        model.Bind("can_replay", &m_canReplay);
        model.Bind("active", &m_active);
        model.Bind("discipline", &m_discipline);
        model.Bind("camera_label", &m_cameraLabel);
        model.Bind("aiming", &m_aiming);
        model.Bind("gamepad", &m_gamepad);
        model.Bind("show_prompts", &m_showPrompts);

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

        model.Bind("elevation", &m_elevation);
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

    void HudScreen::setVariant(const GameVariantDefinition& variant)
    {
        m_ballStyles = {};
        for (const BallSpawnDefinition& definition : variant.objectBalls)
        {
            if ((definition.number >= 0) && (definition.number < static_cast<int>(m_ballStyles.size())))
            {
                HudBall ball;
                ball.color = hexColor(definition.albedo);
                ball.stripe = definition.ruleTag == BallRuleTag::Stripe;
                ball.fill = ball.stripe ? "#F7F4EC" : ball.color;
                m_ballStyles[static_cast<std::size_t>(definition.number)] = ball;
            }
        }
        for (std::vector<int>& numbers : m_trayNumbers)
        {
            numbers.clear();   // rebuild the trays with the new colours
        }
        for (int& on : m_trayOn)
        {
            on = -2;
        }
    }

    void HudScreen::setTray(int tray, const std::vector<int>& numbers, int onBall)
    {
        const std::size_t index = static_cast<std::size_t>(tray);
        if ((m_trayNumbers[index] == numbers) && (m_trayOn[index] == onBall))
        {
            return;
        }

        m_trayNumbers[index] = numbers;
        m_trayOn[index] = onBall;
        m_trays[index].clear();
        for (const int number : numbers)
        {
            if ((number >= 0) && (number < static_cast<int>(m_ballStyles.size())))
            {
                HudBall ball = m_ballStyles[static_cast<std::size_t>(number)];
                ball.on = number == onBall;
                m_trays[index].push_back(ball);
            }
        }
        static const char* const NAMES[3] = {"tray1", "tray2", "tray_table"};
        m_model.DirtyVariable(NAMES[index]);
    }

    void HudScreen::update(const HudSnapshot& snapshot, float deltaTimeSeconds)
    {
        assign(m_model, m_names[0], snapshot.playerNames[0], "p1_name");
        assign(m_model, m_names[1], snapshot.playerNames[1], "p2_name");
        const bool rotation = snapshot.game != GameDiscipline::EightBall;
        for (int player = 0; player < 2; ++player)
        {
            const std::size_t i = static_cast<std::size_t>(player);
            const std::string label = rotation ? foulsLabel(snapshot.fouls[i]) : std::string(groupLabel(snapshot.groups[i]));
            assign(m_model, m_groupLabels[i], label, player == 0 ? "p1_group" : "p2_group");
            assign(m_model, m_frames[i], std::to_string(snapshot.frames[i]), player == 0 ? "p1_frames" : "p2_frames");
        }
        assign(m_model, m_rotation, rotation, "rotation");
        assign(m_model, m_groupsAssigned, !rotation && (snapshot.groups[0] != Rules::Group::None), "groups_assigned");
        assign(m_model, m_showFrames, snapshot.raceTo > 1, "show_frames");
        assign(m_model, m_race, "RACE TO " + std::to_string(snapshot.raceTo), "race");
        assign(m_model, m_placing, snapshot.placing, "placing");
        assign(m_model, m_placementValid, snapshot.placementValid, "placement_valid");
        assign(m_model, m_canPlace, snapshot.canPlace && !snapshot.placing, "can_place");
        assign(m_model, m_placeText,
            std::string(snapshot.behindHeadString ? "BEHIND THE HEAD STRING" : "ANYWHERE ON THE TABLE"), "place_text");
        assign(m_model, m_calling, snapshot.calling, "calling");
        assign(m_model, m_callText, snapshot.callText, "call_text");
        assign(m_model, m_pushAvailable, snapshot.pushOutAvailable, "push_available");
        assign(m_model, m_pushDeclared, snapshot.pushOutDeclared, "push_declared");
        assign(m_model, m_replaying, snapshot.replaying, "replaying");
        {
            const int seconds = static_cast<int>(std::ceil(std::max(snapshot.clockSeconds, 0.0f)));
            assign(m_model, m_clockVisible, snapshot.clockEnabled, "clock_visible");
            assign(m_model, m_clockText, std::to_string(seconds), "clock_text");
            assign(m_model, m_clockLow, snapshot.clockRunning && (seconds <= 10), "clock_low");
            const bool extend = snapshot.clockRunning && snapshot.extensions[static_cast<std::size_t>(snapshot.activePlayer)];
            assign(m_model, m_clockExtend, extend, "clock_extend");
            assign(m_model, m_extension[0], snapshot.extensions[0], "ext1");
            assign(m_model, m_extension[1], snapshot.extensions[1], "ext2");
        }
        assign(m_model, m_replaySlow, snapshot.replaySlow, "replay_slow");
        assign(m_model, m_canReplay, snapshot.canReplay && snapshot.aiming, "can_replay");
        assign(m_model, m_active, snapshot.activePlayer, "active");
        assign(m_model, m_discipline, snapshot.discipline, "discipline");
        assign(m_model, m_cameraLabel, snapshot.cameraLabel, "camera_label");
        assign(m_model, m_aiming, snapshot.aiming, "aiming");
        assign(m_model, m_gamepad, snapshot.gamepadPrompts, "gamepad");
        assign(m_model, m_showPrompts, snapshot.showPrompts, "show_prompts");
        setTray(0, rotation ? std::vector<int>{} : snapshot.remainingBalls[0], -1);
        setTray(1, rotation ? std::vector<int>{} : snapshot.remainingBalls[1], -1);
        setTray(2, rotation ? snapshot.tableBalls : std::vector<int>{},
                (rotation && !snapshot.tableBalls.empty()) ? snapshot.tableBalls.front() : -1);

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

        {
            const int degrees = static_cast<int>(std::lround(snapshot.elevationDegrees));
            const std::string elevation = (degrees <= 0) ? std::string("CUE LEVEL")
                : "CUE UP " + std::to_string(degrees) + "°" + ((degrees >= 25) ? " · MASSÉ" : (degrees >= 8) ? " · SWERVE" : "");
            assign(m_model, m_elevation, elevation, "elevation");
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

    void HudScreen::clearAnnouncements()
    {
        assign(m_model, m_bannerVisible, false, "banner_visible");
        assign(m_model, m_lowerVisible, false, "lower_visible");
        m_bannerTimer = 0.0f;
        m_lowerTimer = 0.0f;
    }

    void HudScreen::announce(const ShotOutcome& outcome, const HudSnapshot& snapshot)
    {
        const Rules::Verdict& verdict = outcome.verdict;
        const auto name = [&](int player) { return snapshot.playerNames[static_cast<std::size_t>(player)]; };
        const std::string game = std::to_string(Rules::gameBall(snapshot.game));

        if (verdict.frameOver)
        {
            const bool won = verdict.end == Rules::FrameEnd::GameBallPotted;
            std::string how;
            switch (verdict.end)
            {
                case Rules::FrameEnd::GameBallPotted:
                    how = name(verdict.shooter) + " POTS THE " + game;
                    break;
                case Rules::FrameEnd::EightBallEarly:
                    how = "8-BALL POTTED EARLY";
                    break;
                case Rules::FrameEnd::EightBallOnFoul:
                    how = "8-BALL POTTED ON A FOUL";
                    break;
                case Rules::FrameEnd::EightBallWrongPocket:
                    how = "8-BALL IN AN UNCALLED POCKET";
                    break;
                case Rules::FrameEnd::ThreeFouls:
                    how = "THIRD FOUL IN A ROW";
                    break;
                case Rules::FrameEnd::None:
                    break;
            }
            showBanner(outcome.matchOver ? "MATCH" : "FRAME", how, !won, won, BANNER_SECONDS);
            return;
        }

        if (verdict.pushOut && (verdict.foul == Rules::Foul::None))
        {
            showBanner("PUSH OUT", name(verdict.shooter) + " PUSHES OUT", false, false, BANNER_SECONDS);
            return;
        }

        if (verdict.illegalBreak)
        {
            showBanner("BREAK", verdict.foul == Rules::Foul::None ? "ILLEGAL BREAK" : "ILLEGAL BREAK · SCRATCH",
                       true, false, BANNER_SECONDS);
            return;
        }

        const std::string spotted = verdict.spot.empty()
            ? std::string()
            : "The " + ballList(verdict.spot) + " goes back on the foot spot";

        if (verdict.foul != Rules::Foul::None)
        {
            showBanner("FOUL", foulText(verdict.foul), true, false, BANNER_SECONDS);

            const bool rotation = snapshot.game != GameDiscipline::EightBall;
            if (rotation && (verdict.foulsInRow == 2))
            {
                showLowerThird("ON TWO FOULS", name(verdict.shooter), "One more foul in a row loses the frame",
                               LOWER_THIRD_SECONDS + 0.6f);
                return;
            }

            const bool kitchen = snapshot.behindHeadString;
            showLowerThird(
                "BALL IN HAND",
                name(outcome.nextPlayer),
                !spotted.empty() ? spotted : kitchen ? "Behind the head string" : "Anywhere on the table",
                LOWER_THIRD_SECONDS
            );
            return;
        }

        if (verdict.choice == Rules::Choice::EightOnBreak)
        {
            showBanner("BREAK", "8-BALL ON THE BREAK", false, true, BANNER_SECONDS);
            return;
        }

        if (verdict.groupsAssigned)
        {
            const std::size_t shooter = static_cast<std::size_t>(verdict.shooter);
            showBanner("GROUPS", name(verdict.shooter) + " · " + groupLabel(snapshot.groups[shooter]), false, true, BANNER_SECONDS);
            return;
        }

        if (!spotted.empty())
        {
            showLowerThird("SPOTTED", "THE " + ballList(verdict.spot), spotted, LOWER_THIRD_SECONDS);
            return;
        }

        if (verdict.turnPassed)
        {
            const std::size_t next = static_cast<std::size_t>(outcome.nextPlayer);
            std::string sub;
            if (snapshot.game != GameDiscipline::EightBall)
            {
                sub = snapshot.tableBalls.empty() ? std::string() : "On the " + std::to_string(snapshot.tableBalls.front());
            }
            else if (snapshot.groups[next] == Rules::Group::None)
            {
                sub = "Open table";
            }
            else
            {
                const std::size_t left = snapshot.remainingBalls[next].size();
                sub = (left == 0) ? std::string("On the 8")
                    : std::to_string(left) + (left == 1 ? " ball left" : " balls left") + " before the 8";
            }
            showLowerThird("AT THE TABLE", name(outcome.nextPlayer), sub, LOWER_THIRD_SECONDS);
        }
    }
}
