// Writes the game's synthesised sounds, and a full break mixed from the
// simulator's sound cues, to WAV files for listening and sound design:
//   ./build/bs_sound_preview OUTPUT_DIR

#include "audio/shot_sounds.h"
#include "audio/sound_synth.h"
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

    writeWav(dir / "ball_click.wav", Audio::ballClick(1).samples, 1);
    writeWav(dir / "cushion_thud.wav", Audio::cushionThud(101).samples, 1);
    writeWav(dir / "pocket_drop.wav", Audio::pocketDrop(201).samples, 1);
    writeWav(dir / "cue_strike.wav", Audio::cueStrike(301).samples, 1);
    writeWav(dir / "ui_tick.wav", Audio::uiTick().samples, 1);
    writeWav(dir / "ui_confirm.wav", Audio::uiConfirm().samples, 1);
    writeWav(dir / "crowd_murmur.wav", Audio::crowdMurmur(8.0, 43).samples, 1);
    writeWav(dir / "applause.wav", Audio::applause(5.0, 41).samples, 1);

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
    const std::vector<Audio::SoundCue> cues = Audio::planShotSounds(shot, variant.table.clothWidth, variant.table.clothDepth);

    std::vector<float> mix(static_cast<std::size_t>((shot.duration() + 1.0) * Audio::SAMPLE_RATE) * 2, 0.0f);
    std::uint32_t seed = 1;
    for (const Audio::SoundCue& cue : cues)
    {
        Audio::SoundBuffer sound;
        switch (cue.kind)
        {
            case Audio::SoundKind::BallBall: sound = Audio::ballClick(seed++); break;
            case Audio::SoundKind::Cushion: sound = Audio::cushionThud(seed++); break;
            case Audio::SoundKind::Pocket: sound = Audio::pocketDrop(seed++); break;
            case Audio::SoundKind::CueStrike: sound = Audio::cueStrike(seed++); break;
        }
        // Listener at the head end, looking down the table: +z is to the right.
        const float pan = std::clamp(cue.position.z / 0.8f, -1.0f, 1.0f);
        const float left = cue.intensity * std::sqrt(0.5f * (1.0f - pan));
        const float right = cue.intensity * std::sqrt(0.5f * (1.0f + pan));
        const auto start = static_cast<std::size_t>(cue.time * Audio::SAMPLE_RATE);
        for (std::size_t i = 0; i < sound.samples.size() && 2 * (start + i) + 1 < mix.size(); ++i)
        {
            mix[2 * (start + i)] += 0.5f * left * sound.samples[i];
            mix[2 * (start + i) + 1] += 0.5f * right * sound.samples[i];
        }
    }
    writeWav(dir / "break_mix.wav", mix, 2);
    std::cout << cues.size() << " sound cues over " << shot.duration() << " s\n";
    return 0;
}
