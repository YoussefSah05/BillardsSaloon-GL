#include "core/launch_options.h"

#include <charconv>
#include <stdexcept>

namespace BilliardsSaloon
{
    namespace
    {
        StartScreen parseStartScreen(std::string_view value)
        {
            if (value == "main") return StartScreen::MainMenu;
            if (value == "game") return StartScreen::Gameplay;
            if (value == "pause") return StartScreen::Pause;
            throw std::invalid_argument("Unknown screen '" + std::string(value) + "' (use main, game or pause).");
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
            "  --screen main|game|pause  start on this screen\n"
            "  --capture FILE.png      save a screenshot after a few frames, then quit\n"
            "  --capture-frames N      frames to render before capturing (default 90)\n";
    }
}
