#pragma once

#include <cstdint>
#include <vector>

namespace BilliardsSaloon::Audio
{
    inline constexpr int SAMPLE_RATE = 48000;

    // Mono PCM, -1..1.
    struct SoundBuffer
    {
        std::vector<float> samples;

        [[nodiscard]] double seconds() const { return static_cast<double>(samples.size()) / SAMPLE_RATE; }
    };

    // Sounds synthesised from simple physical models, so the game ships no
    // recorded audio. Each takes a seed for small natural variations.

    // Two phenolic balls meeting: a bright, very short click (a few kHz
    // resonances, ~25 ms).
    [[nodiscard]] SoundBuffer ballClick(std::uint32_t seed);

    // A ball into a cushion: a dull rubber thump with a wooden knock.
    [[nodiscard]] SoundBuffer cushionThud(std::uint32_t seed);

    // A ball dropping into a leather pocket: a soft knock, a short rattle
    // and the roll down the drop.
    [[nodiscard]] SoundBuffer pocketDrop(std::uint32_t seed);

    // Leather tip on the cue ball: a short woody tick.
    [[nodiscard]] SoundBuffer cueStrike(std::uint32_t seed);

    // Interface: a soft tick (move) and a fuller confirm.
    [[nodiscard]] SoundBuffer uiTick();
    [[nodiscard]] SoundBuffer uiConfirm();

    // A hall full of people talking quietly, seamless when looped.
    [[nodiscard]] SoundBuffer crowdMurmur(double seconds, std::uint32_t seed);

    // Applause: many hands clapping, swelling and fading.
    [[nodiscard]] SoundBuffer applause(double seconds, std::uint32_t seed);

    // Largest absolute sample value.
    [[nodiscard]] float peak(const SoundBuffer& sound);
}
