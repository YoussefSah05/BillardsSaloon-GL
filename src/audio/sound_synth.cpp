#include "audio/sound_synth.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace BilliardsSaloon::Audio
{
    namespace
    {
        constexpr double PI = 3.14159265358979323846;

        std::size_t frames(double seconds)
        {
            return static_cast<std::size_t>(seconds * SAMPLE_RATE);
        }

        // Adds a decaying sine (one mode of a struck object).
        void mode(std::vector<float>& out, double start, double frequency, double amplitude, double decaySeconds)
        {
            const std::size_t first = frames(start);
            for (std::size_t i = first; i < out.size(); ++i)
            {
                const double t = static_cast<double>(i - first) / SAMPLE_RATE;
                const double envelope = std::exp(-t / decaySeconds);
                if (envelope < 1.0e-4)
                {
                    break;
                }
                out[i] += static_cast<float>(amplitude * envelope * std::sin(2.0 * PI * frequency * t));
            }
        }

        // Adds a burst of filtered noise with an exponential decay. `brightness`
        // in 0..1 sets a one-pole low-pass (0 = dark, 1 = untouched).
        void noiseBurst(std::vector<float>& out, std::mt19937& random, double start, double amplitude,
                        double attackSeconds, double decaySeconds, double brightness)
        {
            std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
            const std::size_t first = frames(start);
            float low = 0.0f;
            const float k = static_cast<float>(std::clamp(brightness, 0.001, 1.0));
            for (std::size_t i = first; i < out.size(); ++i)
            {
                const double t = static_cast<double>(i - first) / SAMPLE_RATE;
                const double envelope = std::min(1.0, t / std::max(attackSeconds, 1.0e-5)) * std::exp(-t / decaySeconds);
                if ((t > attackSeconds) && (envelope < 1.0e-4))
                {
                    break;
                }
                low += k * (noise(random) - low);
                out[i] += static_cast<float>(amplitude * envelope) * low;
            }
        }

        void normalise(SoundBuffer& sound, float target)
        {
            const float p = peak(sound);
            if (p > 0.0f)
            {
                for (float& s : sound.samples)
                {
                    s *= target / p;
                }
            }
        }

        double jitter(std::mt19937& random, double spread)
        {
            return std::uniform_real_distribution<double>(1.0 - spread, 1.0 + spread)(random);
        }
    }

    float peak(const SoundBuffer& sound)
    {
        float p = 0.0f;
        for (const float s : sound.samples)
        {
            p = std::max(p, std::abs(s));
        }
        return p;
    }

    SoundBuffer ballClick(std::uint32_t seed)
    {
        std::mt19937 random(seed);
        SoundBuffer sound;
        sound.samples.assign(frames(0.06), 0.0f);
        // Hard resin rings in a few high, fast-dying modes, on top of a sharp transient.
        mode(sound.samples, 0.0, 3150.0 * jitter(random, 0.05), 0.9, 0.0045);
        mode(sound.samples, 0.0, 4870.0 * jitter(random, 0.05), 0.6, 0.0032);
        mode(sound.samples, 0.0, 7420.0 * jitter(random, 0.05), 0.35, 0.0020);
        mode(sound.samples, 0.0, 1650.0 * jitter(random, 0.05), 0.25, 0.0060);
        noiseBurst(sound.samples, random, 0.0, 0.5, 0.0002, 0.0012, 0.9);
        normalise(sound, 0.9f);
        return sound;
    }

    SoundBuffer cushionThud(std::uint32_t seed)
    {
        std::mt19937 random(seed);
        SoundBuffer sound;
        sound.samples.assign(frames(0.16), 0.0f);
        mode(sound.samples, 0.0, 145.0 * jitter(random, 0.08), 0.9, 0.030);
        mode(sound.samples, 0.0, 310.0 * jitter(random, 0.08), 0.45, 0.018);
        mode(sound.samples, 0.0, 820.0 * jitter(random, 0.10), 0.18, 0.008);   // the rail's wood
        noiseBurst(sound.samples, random, 0.0, 0.35, 0.0008, 0.010, 0.12);
        normalise(sound, 0.85f);
        return sound;
    }

    SoundBuffer pocketDrop(std::uint32_t seed)
    {
        std::mt19937 random(seed);
        SoundBuffer sound;
        sound.samples.assign(frames(0.75), 0.0f);
        // A soft knock on the leather...
        mode(sound.samples, 0.0, 190.0 * jitter(random, 0.08), 0.8, 0.040);
        noiseBurst(sound.samples, random, 0.0, 0.3, 0.001, 0.02, 0.08);
        // ...a short rattle as it settles...
        double t = 0.05;
        for (int i = 0; i < 5; ++i)
        {
            t += 0.03 + 0.03 * jitter(random, 0.5);
            mode(sound.samples, t, 260.0 * jitter(random, 0.15), 0.35 * std::exp(-0.45 * i), 0.025);
            noiseBurst(sound.samples, random, t, 0.12 * std::exp(-0.45 * i), 0.0008, 0.012, 0.1);
        }
        // ...and the roll down the drop.
        noiseBurst(sound.samples, random, 0.18, 0.10, 0.08, 0.20, 0.03);
        normalise(sound, 0.8f);
        return sound;
    }

    SoundBuffer cueStrike(std::uint32_t seed)
    {
        std::mt19937 random(seed);
        SoundBuffer sound;
        sound.samples.assign(frames(0.08), 0.0f);
        mode(sound.samples, 0.0, 1250.0 * jitter(random, 0.06), 0.7, 0.006);
        mode(sound.samples, 0.0, 2480.0 * jitter(random, 0.06), 0.45, 0.004);
        mode(sound.samples, 0.0, 520.0 * jitter(random, 0.06), 0.35, 0.012);   // the shaft
        noiseBurst(sound.samples, random, 0.0, 0.45, 0.0003, 0.0025, 0.5);
        normalise(sound, 0.85f);
        return sound;
    }

    SoundBuffer uiTick()
    {
        std::mt19937 random(7);
        SoundBuffer sound;
        sound.samples.assign(frames(0.05), 0.0f);
        mode(sound.samples, 0.0, 2200.0, 0.6, 0.006);
        noiseBurst(sound.samples, random, 0.0, 0.15, 0.0005, 0.002, 0.4);
        normalise(sound, 0.5f);
        return sound;
    }

    SoundBuffer uiConfirm()
    {
        SoundBuffer sound;
        sound.samples.assign(frames(0.22), 0.0f);
        mode(sound.samples, 0.0, 660.0, 0.6, 0.06);
        mode(sound.samples, 0.045, 990.0, 0.5, 0.08);
        normalise(sound, 0.55f);
        return sound;
    }

    SoundBuffer crowdMurmur(double seconds, std::uint32_t seed)
    {
        std::mt19937 random(seed);
        SoundBuffer sound;
        sound.samples.assign(frames(seconds), 0.0f);

        // Many voices: band-limited noise shaped by slow, independent
        // syllable-rate envelopes, mixed down.
        std::uniform_real_distribution<double> unit(0.0, 1.0);
        constexpr int VOICES = 24;
        for (int v = 0; v < VOICES; ++v)
        {
            std::vector<float> voice(sound.samples.size(), 0.0f);
            noiseBurst(voice, random, 0.0, 1.0, 0.001, 1.0e9, 0.05 + 0.04 * unit(random));
            const double rate = 2.0 + 3.0 * unit(random);       // syllables per second
            const double phase = unit(random) * 2.0 * PI;
            const double formant = 0.5 + 0.5 * unit(random);
            for (std::size_t i = 0; i < voice.size(); ++i)
            {
                const double t = static_cast<double>(i) / SAMPLE_RATE;
                // Envelopes repeat a whole number of times over the loop, so it is seamless.
                const double cycles = std::round(rate * seconds) / seconds;
                const double syllable = std::pow(0.5 + 0.5 * std::sin(2.0 * PI * cycles * t + phase), 2.0);
                sound.samples[i] += static_cast<float>(syllable * formant) * voice[i];
            }
        }
        // Voices live below ~2 kHz: two more low-pass poles (~1.1 kHz) take
        // out the hiss, and a gentle high-pass the rumble.
        for (int pass = 0; pass < 2; ++pass)
        {
            float low = 0.0f;
            for (float& sample : sound.samples)
            {
                low += 0.15f * (sample - low);
                sample = low;
            }
        }
        {
            float slow = 0.0f;
            for (float& sample : sound.samples)
            {
                slow += 0.016f * (sample - slow);
                sample -= slow;
            }
        }

        // Remove the loop's start/end mismatch with a short crossfade.
        const std::size_t fade = frames(0.25);
        for (std::size_t i = 0; i < fade && i < sound.samples.size() / 2; ++i)
        {
            const float w = static_cast<float>(i) / static_cast<float>(fade);
            const std::size_t j = sound.samples.size() - fade + i;
            const float blended = sound.samples[i] * w + sound.samples[j] * (1.0f - w);
            sound.samples[i] = blended;
        }
        sound.samples.resize(sound.samples.size() - fade);
        normalise(sound, 0.6f);
        return sound;
    }

    SoundBuffer applause(double seconds, std::uint32_t seed)
    {
        std::mt19937 random(seed);
        SoundBuffer sound;
        sound.samples.assign(frames(seconds), 0.0f);
        std::uniform_real_distribution<double> unit(0.0, 1.0);

        // Each pair of hands claps at its own rate; the crowd swells in and fades out.
        constexpr int PEOPLE = 60;
        for (int person = 0; person < PEOPLE; ++person)
        {
            const double rate = 3.5 + 2.5 * unit(random);
            double t = 0.15 * unit(random);
            const double stop = seconds * (0.55 + 0.4 * unit(random));
            const double brightness = 0.25 + 0.5 * unit(random);
            while (t < stop)
            {
                const double swell = std::min(1.0, t / 0.4) * std::clamp((stop - t) / 0.8, 0.0, 1.0);
                noiseBurst(sound.samples, random, t, 0.3 * swell * (0.6 + 0.4 * unit(random)), 0.0005, 0.009, brightness);
                t += (1.0 / rate) * (0.85 + 0.3 * unit(random));
            }
        }
        normalise(sound, 0.7f);
        return sound;
    }
}
