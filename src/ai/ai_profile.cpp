#include "ai/ai_profile.h"

#include "core/asset_paths.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

namespace BilliardsSaloon::Ai
{
    std::vector<AiProfile> loadAiProfiles(const std::filesystem::path& file)
    {
        std::ifstream stream(file);
        if (!stream)
        {
            throw std::runtime_error("Could not open AI players " + file.string());
        }
        try
        {
            const nlohmann::json json = nlohmann::json::parse(stream);
            std::vector<AiProfile> profiles;
            for (const nlohmann::json& item : json.at("players"))
            {
                AiProfile p;
                p.id = item.at("id").get<std::string>();
                p.name = item.at("name").get<std::string>();
                p.tier = item.value("tier", p.tier);
                p.style = item.value("style", p.style);
                p.aimNoiseDegrees = item.value("aimNoiseDegrees", p.aimNoiseDegrees);
                p.powerNoise = item.value("powerNoise", p.powerNoise);
                p.spinNoise = item.value("spinNoise", p.spinNoise);
                p.safetyBias = item.value("safetyBias", p.safetyBias);
                p.powerStyle = item.value("powerStyle", p.powerStyle);
                p.breakPower = item.value("breakPower", p.breakPower);
                p.thinkSeconds = item.value("thinkSeconds", p.thinkSeconds);
                p.samples = item.value("samples", p.samples);
                profiles.push_back(p);
            }
            if (profiles.empty())
            {
                throw std::invalid_argument("no players");
            }
            return profiles;
        }
        catch (const std::exception& error)
        {
            throw std::runtime_error("Invalid AI players " + file.string() + ": " + error.what());
        }
    }

    const std::vector<AiProfile>& aiProfiles()
    {
        static const std::vector<AiProfile> profiles = loadAiProfiles(resolveAssetPath("data/ai/players.json"));
        return profiles;
    }

    const AiProfile& findAiProfile(const std::string& id)
    {
        for (const AiProfile& profile : aiProfiles())
        {
            if (profile.id == id)
            {
                return profile;
            }
        }
        return aiProfiles().front();
    }
}
