// Writes the game's recorded sounds, and a full break mixed from the
// simulator's sound cues, to WAV files for listening and sound design:
//   ./build/bs_sound_preview OUTPUT_DIR

#include "audio/shot_sounds.h"
#include "app/audio_engine.h"
#include "audio/recorded_bank.h"
#include "core/asset_paths.h"
#include "gameplay/game_variant.h"
#include "gameplay/sim_bridge.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace BilliardsSaloon;

namespace
{
    // 16-bit PCM WAV, mono or interleaved stereo.
    void writeWav(const std::filesystem::path& file, const std::vector<float>& samples, int channels)
    {
        std::ofstream out(file, std::ios::binary);
        const auto u32 = [&](std::uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
        const auto u16 = [&](std::uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };
        const auto bytes = static_cast<std::uint32_t>(samples.size() * 2);
        out.write("RIFF", 4); u32(36 + bytes); out.write("WAVE", 4);
        out.write("fmt ", 4); u32(16); u16(1); u16(static_cast<std::uint16_t>(channels));
        u32(Audio::SAMPLE_RATE); u32(static_cast<std::uint32_t>(Audio::SAMPLE_RATE * channels * 2));
        u16(static_cast<std::uint16_t>(channels * 2)); u16(16);
        out.write("data", 4); u32(bytes);
        for (const float s : samples)
        {
            const auto v = static_cast<std::int16_t>(std::lround(std::clamp(s, -1.0f, 1.0f) * 32767.0f));
            out.write(reinterpret_cast<const char*>(&v), 2);
        }
        std::cout << "Wrote " << file.string() << '\n';
    }
}

int main(int argc, char** argv)
{
    const std::filesystem::path dir = (argc > 1) ? argv[1] : "sound_preview";
    std::filesystem::create_directories(dir);

    for (const auto& sample : Audio::loadRecordedBank(resolveAssetPath("audio/bank.json")))
    {
        writeWav(dir / sample.file, sample.pcm.samples, 1);
    }

    // A break from the head spot, mixed as the game would: each cue's sound at
    // its time, its loudness, panned by where on the table it happened.
    const GameVariantDefinition& variant = eightBallVariant();
    const Sim::Table table = Sim::buildPocketTable(variant.table.pocketGeometry);
    std::vector<Sim::BallState> balls(1);
    balls[0].r = glm::dvec3(0.5 * variant.table.clothDepth, 0.25 * variant.table.clothWidth, variant.table.simBall.R);
    for (const glm::vec3& p : buildRackPositions(variant))
    {
        Sim::BallState ball;
        ball.r = SimBridge::toSimPosition(p, variant.table.clothWidth, variant.table.clothDepth);
        ball.r.z = variant.table.simBall.R;
        balls.push_back(ball);
    }
    Sim::CueStrike strike;
    strike.speed = 7.0;
    strike.phiDegrees = 90.0;
    const Sim::ShotTrajectory shot = Sim::simulateShot(table, balls, 0, strike, variant.table.simBall);
    const std::vector<Audio::SoundCue> cues = Audio::planShotSounds(shot, table);

    std::vector<float> mix(static_cast<std::size_t>((shot.duration() + 1.0) * Audio::SAMPLE_RATE) * 2, 0.0f);
    AudioEngine engine(true, true);
    engine.setListener({-1.8f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
    if (!engine.available())
    {
        std::cerr << "Could not initialize offline audio mixer.\n";
        return 1;
    }
    std::size_t next = 0;
    for (std::size_t frame = 0; frame < mix.size() / 2; frame += 256)
    {
        const double now = static_cast<double>(frame) / Audio::SAMPLE_RATE;
        while (next < cues.size() && cues[next].time <= now + 0.05)
        {
            engine.playShotCue(cues[next], cues[next].time - now);
            ++next;
        }
        const auto states = shot.stateAt(now);
        float energy = 0.0f;
        glm::vec3 position(0.0f);
        for (const auto& ball : states)
        {
            const float speedSquared = static_cast<float>(glm::dot(ball.v, ball.v));
            energy += speedSquared;
            position += speedSquared * SimBridge::toGamePosition(ball.r, table.length, table.width);
        }
        engine.setRolling(std::sqrt(energy), energy > 0.0f ? position / energy : glm::vec3(0.0f));
        const std::size_t count = std::min<std::size_t>(512, mix.size() - 2 * frame);
        if (!engine.renderOffline(std::span<float>(mix).subspan(2 * frame, count))) return 1;
        engine.update();
    }
    writeWav(dir / "break_mix.wav", mix, 2);
    std::cout << cues.size() << " sound cues over " << shot.duration() << " s\n";
    return 0;
}
