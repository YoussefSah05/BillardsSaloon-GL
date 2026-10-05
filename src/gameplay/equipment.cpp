#include "gameplay/equipment.h"

#include "core/asset_paths.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

namespace BilliardsSaloon
{
    namespace
    {
        using Json = nlohmann::json;

        glm::vec3 readColor(const Json& value)
        {
            if (!value.is_array() || (value.size() != 3))
            {
                throw std::invalid_argument("a colour must be [r, g, b]");
            }
            return glm::vec3(value[0].get<float>(), value[1].get<float>(), value[2].get<float>());
        }

        std::vector<FinishOption> readFinishes(const Json& json, const char* category)
        {
            std::vector<FinishOption> options;
            for (const Json& item : json.at(category))
            {
                FinishOption option;
                option.id = item.at("id").get<std::string>();
                option.name = item.at("name").get<std::string>();
                option.color = readColor(item.at("color"));
                option.roughness = item.value("roughness", option.roughness);
                option.clearcoat = item.value("clearcoat", option.clearcoat);
                option.metal = item.value("metal", option.metal);
                options.push_back(option);
            }
            if (options.empty())
            {
                throw std::invalid_argument(std::string("\"") + category + "\" has no options");
            }
            return options;
        }
    }

    EquipmentCatalog loadEquipmentCatalog(const std::filesystem::path& file)
    {
        std::ifstream stream(file);
        if (!stream)
        {
            throw std::runtime_error("Could not open equipment catalogue " + file.string());
        }

        try
        {
            const Json json = Json::parse(stream);
            EquipmentCatalog catalog;
            catalog.cloth = readFinishes(json, "cloth");
            catalog.rails = readFinishes(json, "rails");
            catalog.trim = readFinishes(json, "trim");
            catalog.pockets = readFinishes(json, "pockets");

            for (const Json& item : json.at("balls"))
            {
                BallSetOption option;
                option.id = item.at("id").get<std::string>();
                option.name = item.at("name").get<std::string>();
                option.measleCueBall = item.value("cueBall", std::string("plain")) == "measle";
                // Keep the colours object alive for the loop (items() only refers to it).
                const Json colors = item.value("colors", Json::object());
                for (const auto& [number, color] : colors.items())
                {
                    option.colors[std::stoi(number)] = readColor(color);
                }
                catalog.balls.push_back(option);
            }

            for (const Json& item : json.at("cues"))
            {
                CueOption option;
                option.id = item.at("id").get<std::string>();
                option.name = item.at("name").get<std::string>();
                option.shaft = readColor(item.at("shaft"));
                option.forearm = readColor(item.at("forearm"));
                option.wrap = readColor(item.at("wrap"));
                option.joint = readColor(item.at("joint"));
                catalog.cues.push_back(option);
            }

            for (const Json& item : json.at("halls"))
            {
                HallOption option;
                option.id = item.at("id").get<std::string>();
                option.name = item.at("name").get<std::string>();
                option.lamp = readColor(item.at("lamp"));
                option.intensity = item.value("intensity", option.intensity);
                option.exposure = item.value("exposure", option.exposure);
                option.clear = readColor(item.at("clear"));
                option.layout = item.value("layout", option.layout);
                option.ambient = item.value("ambient", option.ambient);
                catalog.halls.push_back(option);
            }

            if (catalog.balls.empty() || catalog.cues.empty() || catalog.halls.empty())
            {
                throw std::invalid_argument("balls, cues and halls need at least one option each");
            }
            return catalog;
        }
        catch (const std::exception& error)
        {
            throw std::runtime_error("Invalid equipment catalogue " + file.string() + ": " + error.what());
        }
    }

    const EquipmentCatalog& equipmentCatalog()
    {
        static const EquipmentCatalog catalog = loadEquipmentCatalog(resolveAssetPath("data/equipment/catalog.json"));
        return catalog;
    }
}
