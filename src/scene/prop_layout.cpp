#include "scene/prop_layout.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

namespace BilliardsSaloon
{
    std::vector<PropPlacement> loadPropLayout(const std::filesystem::path& file, const std::string& layout)
    {
        std::ifstream stream(file);
        if (!stream)
        {
            throw std::runtime_error("Could not open " + file.string());
        }
        try
        {
            const nlohmann::json json = nlohmann::json::parse(stream);
            std::vector<PropPlacement> placements;
            const nlohmann::json& halls = json.at("halls");
            if (!halls.contains(layout))
            {
                return placements;
            }
            for (const nlohmann::json& item : halls.at(layout))
            {
                PropPlacement placement;
                placement.model = item.at("model").get<std::string>();
                const std::vector<float> p = item.at("position").get<std::vector<float>>();
                if (p.size() != 3)
                {
                    throw std::runtime_error("position needs three numbers");
                }
                placement.position = glm::vec3(p[0], p[1], p[2]);
                placement.yawDegrees = item.value("yaw", 0.0f);
                placement.scale = item.value("scale", 1.0f);
                placements.push_back(placement);
            }
            return placements;
        }
        catch (const nlohmann::json::exception& error)
        {
            throw std::runtime_error(file.string() + ": " + error.what());
        }
    }
}
