#pragma once

#include <glm/glm.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace BilliardsSaloon
{
    // One choice in a finish category (cloth, rail wood, trim, pocket leather).
    struct FinishOption
    {
        std::string id;
        std::string name;
        glm::vec3 color {1.0f};
        float roughness {0.4f};
        float clearcoat {0.0f};
        float metal {0.0f};      // reflectance at normal incidence for metals
    };

    struct BallSetOption
    {
        std::string id;
        std::string name;
        bool measleCueBall {false};          // red spots, as on broadcast cue balls
        std::map<int, glm::vec3> colors;     // by number; missing numbers keep the variant's colour
    };

    struct CueOption
    {
        std::string id;
        std::string name;
        glm::vec3 shaft {0.86f, 0.74f, 0.55f};
        glm::vec3 forearm {0.24f, 0.10f, 0.05f};
        glm::vec3 wrap {0.05f};
        glm::vec3 joint {0.78f};
    };

    // A lighting mood for the hall: lamp colour and strength, exposure,
    // and the colour of the dark around the table.
    struct HallOption
    {
        std::string id;
        std::string name;
        glm::vec3 lamp {1.0f};
        float intensity {4.4f};
        float exposure {1.0f};
        glm::vec3 clear {0.03f};
        std::string layout {"arena"};   // "arena" or "saloon" (scene/hall_geometry)
        float ambient {1.0f};           // scale on the room's fill light
    };

    struct EquipmentCatalog
    {
        std::vector<FinishOption> cloth;
        std::vector<FinishOption> rails;
        std::vector<FinishOption> trim;
        std::vector<FinishOption> pockets;
        std::vector<BallSetOption> balls;
        std::vector<CueOption> cues;
        std::vector<HallOption> halls;
    };

    // The option with this id, or the first one if there is none (so old or
    // hand-edited settings never leave the table without a finish).
    template <typename Option>
    [[nodiscard]] const Option& findOption(const std::vector<Option>& options, const std::string& id)
    {
        for (const Option& option : options)
        {
            if (option.id == id)
            {
                return option;
            }
        }
        return options.front();
    }

    // Loads a catalogue; throws std::runtime_error naming the file and problem.
    // Every category must have at least one option.
    [[nodiscard]] EquipmentCatalog loadEquipmentCatalog(const std::filesystem::path& file);

    // assets/data/equipment/catalog.json, loaded once.
    const EquipmentCatalog& equipmentCatalog();
}
