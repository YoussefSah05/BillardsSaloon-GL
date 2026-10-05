#include "core/launch_options.h"

#include <charconv>
#include <stdexcept>

namespace BilliardsSaloon
{
    namespace
    {
        StartScreen parseStartScreen(std::string_view value)
        {
            if (value == "title") return StartScreen::Title;
            if (value == "main") return StartScreen::MainMenu;
            if (value == "game") return StartScreen::Gameplay;
            if (value == "pause") return StartScreen::Pause;
            if (value == "settings") return StartScreen::Settings;
            if (value == "setup") return StartScreen::MatchSetup;
            if (value == "locker") return StartScreen::Locker;
            throw std::invalid_argument("Unknown screen '" + std::string(value) + "' (use title, main, setup, locker, game, pause or settings).");
        }

        int parsePositiveInt(std::string_view option, std::string_view value)
        {
            int result = 0;
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
            if ((error != std::errc{}) || (end != value.data() + value.size()) || (result <= 0))
            {
                throw std::invalid_argument(std::string(option) + " needs a positive whole number.");
            }
            return result;
        }
    }

    LaunchOptions parseLaunchOptions(std::span<const std::string_view> arguments)
    {
        LaunchOptions options;

        for (std::size_t i = 0; i < arguments.size(); ++i)
        {
            const std::string_view argument = arguments[i];

            auto nextValue = [&]() -> std::string_view
            {
                if (i + 1 >= arguments.size())
                {
                    throw std::invalid_argument(std::string(argument) + " needs a value.");
                }
                return arguments[++i];
            };

            if (argument == "--fullscreen")
            {
                options.fullscreen = true;
            }
            else if (argument == "--screen")
            {
                options.startScreen = parseStartScreen(nextValue());
            }
            else if (argument == "--capture")
            {
                options.capturePath = std::filesystem::path(std::string(nextValue()));
            }
            else if (argument == "--scenario")
            {
                const std::string_view value = nextValue();
                if (value == "foul") options.scenario = DevScenario::Foul;
                else if (value == "choice") options.scenario = DevScenario::Choice;
                else if (value == "call") options.scenario = DevScenario::Call;
                else if (value == "replay") options.scenario = DevScenario::Replay;
                else throw std::invalid_argument("--scenario needs 'foul', 'choice', 'call' or 'replay'.");
                options.startScreen = StartScreen::Gameplay;
            }
            else if (argument == "--camera")
            {
                const std::string_view value = nextValue();
                if (value == "aim") options.startCamera = 0;
                else if (value == "overview") options.startCamera = 1;
                else if (value == "follow") options.startCamera = 2;
                else if (value == "free") options.startCamera = 3;
                else if (value == "broadcast") options.startCamera = 4;
                else throw std::invalid_argument("--camera needs broadcast, aim, overview, follow or free.");
            }
            else if (argument == "--capture-frames")
            {
                options.captureAfterFrames = parsePositiveInt(argument, nextValue());
            }
            else
            {
                throw std::invalid_argument("Unknown option '" + std::string(argument) + "'.\n" + launchOptionsUsage());
            }
        }

        return options;
    }

    std::string launchOptionsUsage()
    {
        return
            "Usage: BilliardsSaloon [options]\n"
            "  --fullscreen            start in fullscreen\n"
            "  --screen title|main|setup|locker|game|pause|settings  start on this screen\n"
            "  --scenario foul|choice|call|replay  (development) script a state: ball in hand, a referee choice, a called shot, a winning replay\n"
            "  --camera broadcast|aim|overview|follow|free  start with this camera view\n"
            "  --capture FILE.png      save a screenshot after a few frames, then quit\n"
            "  --capture-frames N      frames to render before capturing (default 90)\n";
    }
}
