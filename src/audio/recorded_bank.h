#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace BilliardsSaloon::Audio
{
    inline constexpr int SAMPLE_RATE = 48000;

    struct SoundBuffer
    {
        std::vector<float> samples;
        [[nodiscard]] double seconds() const { return static_cast<double>(samples.size()) / SAMPLE_RATE; }
    };

    struct RecordedSample
    {
        std::string bank;
        int strength {0};
        std::string file;
        SoundBuffer pcm;
    };

    // Decode bundled recordings once at startup. Throws an actionable error
    // for a missing/corrupt sample; there is no generated-sound fallback.
    [[nodiscard]] std::vector<RecordedSample> loadRecordedBank(const std::filesystem::path& manifest);
}
