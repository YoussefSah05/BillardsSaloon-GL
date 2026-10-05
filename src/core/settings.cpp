#include "core/settings.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <system_error>

namespace BilliardsSaloon
{
    namespace
    {
        using Json = nlohmann::json;

        std::filesystem::path environmentPath(const char* name)
        {
            const char* value = std::getenv(name);
            return (value != nullptr && *value != '\0') ? std::filesystem::path(value) : std::filesystem::path{};
        }

        QualityLevel qualityFromName(const std::string& name)
        {
            if (name == "low") return QualityLevel::Low;
            if (name == "high") return QualityLevel::High;
            return QualityLevel::Balanced;
        }
    }

    const char* qualityName(QualityLevel quality)
    {
        switch (quality)
        {
            case QualityLevel::Low:
                return "low";
            case QualityLevel::Balanced:
                return "balanced";
            case QualityLevel::High:
                return "high";
        }
        return "balanced";
    }

    GameSettings sanitized(GameSettings settings)
    {
        settings.mouseSensitivity = std::clamp(settings.mouseSensitivity, MIN_MOUSE_SENSITIVITY, MAX_MOUSE_SENSITIVITY);
        settings.uiScale = std::clamp(settings.uiScale, MIN_UI_SCALE, MAX_UI_SCALE);
        settings.matchGame = std::clamp(settings.matchGame, 0, MATCH_GAME_COUNT - 1);
        settings.raceTo = std::clamp(settings.raceTo, 1, MAX_RACE_TO);
        return settings;
    }

    std::filesystem::path userDataDirectory()
    {
        std::filesystem::path directory;

#if defined(_WIN32)
        const std::filesystem::path appData = environmentPath("APPDATA");
        if (!appData.empty())
        {
            directory = appData / "Billiards Saloon";
        }
#elif defined(__APPLE__)
        const std::filesystem::path home = environmentPath("HOME");
        if (!home.empty())
        {
            directory = home / "Library" / "Application Support" / "Billiards Saloon";
        }
#else
        const std::filesystem::path xdg = environmentPath("XDG_CONFIG_HOME");
        const std::filesystem::path home = environmentPath("HOME");
        if (!xdg.empty())
        {
            directory = xdg / "billiards-saloon";
        }
        else if (!home.empty())
        {
            directory = home / ".config" / "billiards-saloon";
        }
#endif

        if (!directory.empty())
        {
            std::error_code error;
            std::filesystem::create_directories(directory, error);
        }
        return directory;
    }

    GameSettings loadSettings(const std::filesystem::path& file, std::string* outWarning)
    {
        GameSettings settings;

        std::error_code error;
        if (!std::filesystem::exists(file, error))
        {
            return settings;
        }

        try
        {
            std::ifstream stream(file);
            const Json json = Json::parse(stream);

            // Unknown keys are ignored and missing keys keep their defaults,
            // so older and newer files both load.
            settings.fullscreen = json.value("fullscreen", settings.fullscreen);
            settings.vsync = json.value("vsync", settings.vsync);
            settings.quality = qualityFromName(json.value("quality", std::string(qualityName(settings.quality))));
            settings.mouseSensitivity = json.value("mouseSensitivity", settings.mouseSensitivity);
            settings.uiScale = json.value("uiScale", settings.uiScale);
            settings.reducedMotion = json.value("reducedMotion", settings.reducedMotion);
            if (json.contains("match"))
            {
                const Json& match = json.at("match");
                settings.matchGame = match.value("game", settings.matchGame);
                settings.raceTo = match.value("raceTo", settings.raceTo);
                settings.winnerBreaks = match.value("winnerBreaks", settings.winnerBreaks);
            }
        }
        catch (const std::exception& exception)
        {
            if (outWarning != nullptr)
            {
                *outWarning = "Ignoring unreadable settings file " + file.string() + ": " + exception.what();
            }
            return GameSettings{};
        }

        return sanitized(settings);
    }

    bool saveSettings(const std::filesystem::path& file, const GameSettings& settings)
    {
        const Json json = {
            {"version", GameSettings::CURRENT_VERSION},
            {"fullscreen", settings.fullscreen},
            {"vsync", settings.vsync},
            {"quality", qualityName(settings.quality)},
            {"mouseSensitivity", settings.mouseSensitivity},
            {"uiScale", settings.uiScale},
            {"reducedMotion", settings.reducedMotion},
            {"match", {
                {"game", settings.matchGame},
                {"raceTo", settings.raceTo},
                {"winnerBreaks", settings.winnerBreaks}
            }}
        };

        std::filesystem::path temporary = file;
        temporary += ".tmp";

        {
            std::ofstream stream(temporary, std::ios::trunc);
            if (!stream)
            {
                return false;
            }
            stream << json.dump(2) << '\n';
            if (!stream)
            {
                return false;
            }
        }

        std::error_code error;
        std::filesystem::rename(temporary, file, error);
        if (error)
        {
            std::filesystem::remove(temporary, error);
            return false;
        }
        return true;
    }
}
