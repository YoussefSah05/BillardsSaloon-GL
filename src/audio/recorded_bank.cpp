#include "audio/recorded_bank.h"

#include <miniaudio.h>
#include <nlohmann/json.hpp>

#include <cmath>
#include <fstream>
#include <stdexcept>

namespace BilliardsSaloon::Audio
{
    std::vector<RecordedSample> loadRecordedBank(const std::filesystem::path& manifest)
    {
        std::ifstream input(manifest);
        if (!input)
        {
            throw std::runtime_error("Cannot open sound bank: " + manifest.string());
        }
        const auto document = nlohmann::json::parse(input);
        std::vector<RecordedSample> samples;
        samples.reserve(document.at("samples").size());
        for (const auto& entry : document.at("samples"))
        {
            RecordedSample sample;
            sample.bank = entry.at("bank").get<std::string>();
            sample.strength = entry.value("strength", 0);
            sample.file = entry.at("file").get<std::string>();
            const auto path = manifest.parent_path() / sample.file;
            ma_decoder decoder {};
            const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 1, SAMPLE_RATE);
            if (ma_decoder_init_file(path.string().c_str(), &config, &decoder) != MA_SUCCESS)
            {
                throw std::runtime_error("Cannot decode sound: " + path.string());
            }
            ma_uint64 count = 0;
            const ma_result lengthResult = ma_decoder_get_length_in_pcm_frames(&decoder, &count);
            if (lengthResult != MA_SUCCESS || count == 0 || count > 60 * SAMPLE_RATE)
            {
                ma_decoder_uninit(&decoder);
                throw std::runtime_error("Invalid sound duration: " + path.string());
            }
            sample.pcm.samples.resize(static_cast<std::size_t>(count));
            ma_uint64 read = 0;
            const ma_result result = ma_decoder_read_pcm_frames(&decoder, sample.pcm.samples.data(), count, &read);
            ma_decoder_uninit(&decoder);
            if ((result != MA_SUCCESS && result != MA_AT_END) || read != count)
            {
                throw std::runtime_error("Incomplete sound: " + path.string());
            }
            for (const float value : sample.pcm.samples)
            {
                if (!std::isfinite(value) || std::abs(value) > 1.0f)
                {
                    throw std::runtime_error("Invalid sound samples: " + path.string());
                }
            }
            samples.push_back(std::move(sample));
        }
        if (samples.empty())
        {
            throw std::runtime_error("Empty sound bank: " + manifest.string());
        }
        return samples;
    }
}
