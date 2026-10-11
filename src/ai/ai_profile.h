#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace BilliardsSaloon::Ai
{
    // A computer opponent: how accurately they play and how they like to play.
    // Fictional people; any resemblance to real players is not intended.
    struct AiProfile
    {
        std::string id;
        std::string name;              // as shown in the scorebug, e.g. "VIKTOR HALE"
        std::string tier;              // "Club", "Regional", "Pro", "Champion"
        std::string style;             // one line for the setup screen

        // Execution error, one standard deviation.
        float aimNoiseDegrees {0.5f};
        float powerNoise {0.08f};      // relative
        float spinNoise {0.10f};       // in tip-offset units

        float safetyBias {0.0f};       // added to safeties' scores: positive plays safe more
        float powerStyle {0.0f};       // -1 soft and precise .. +1 firm
        float breakPower {0.9f};
        float thinkSeconds {1.2f};
        int samples {8};               // noisy simulations per finalist (more = steadier choices)
        bool learnedValue {false};     // judge positions with the learned value network when it is available
        bool adaptive {false};         // match execution skill to the observed opponent
        float valueWeight {1.0f};       // blend learned and classical scores; 1 preserves legacy behaviour
    };

    [[nodiscard]] std::vector<AiProfile> loadAiProfiles(const std::filesystem::path& file);

    // assets/data/ai/players.json, loaded once.
    const std::vector<AiProfile>& aiProfiles();
    [[nodiscard]] const AiProfile& findAiProfile(const std::string& id);
}
