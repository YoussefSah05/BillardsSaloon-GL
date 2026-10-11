#include "app/audio_engine.h"
#include "core/asset_paths.h"
#include <miniaudio.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>

namespace Septhuit
{
    namespace
    {
        enum Bank
        {
            BANK_BALL, BANK_CUSHION, BANK_POCKET, BANK_CUE, BANK_JAW,
            BANK_PLACE, BANK_RACK, BANK_SLIDE, BANK_CLOCK, BANK_FOUL,
            BANK_UI_MOVE, BANK_UI_CONFIRM, BANK_APPLAUSE_SHORT,
            BANK_APPLAUSE_LONG, BANK_MURMUR, BANK_ROLL, BANK_COUNT
        };
        enum Bus { BUS_EFFECTS, BUS_CROWD, BUS_INTERFACE, BUS_COUNT };
        constexpr std::array<const char*, BANK_COUNT> BANK_NAMES {
            "ball", "cushion", "pocket", "cue", "jaw", "place", "rack", "slide", "clock", "foul",
            "ui_move", "ui_confirm", "applause_short", "applause_long", "murmur", "roll"
        };
    }

    struct AudioEngine::Impl
    {
        struct Voice
        {
            ma_audio_buffer buffer {};
            ma_sound sound {};
            Bank bank {BANK_BALL};
            int strength {0};
            bool initialized {false};
            bool active {false};
            ma_uint64 started {0};
            ma_uint64 ends {0};
            std::size_t frames {0};
            float volume {0.0f};
        };
        ma_engine engine {};
        bool ready {false};
        bool offline {false};
        int initializedBuses {0};
        std::array<ma_sound_group, BUS_COUNT> buses {};
        std::vector<Audio::RecordedSample> samples;
        std::vector<std::unique_ptr<Voice>> voices;
        std::unique_ptr<Voice> murmur;
        std::unique_ptr<Voice> rolling;
        std::mt19937 random {1234};
        glm::vec3 listener {0.0f};
        glm::vec3 right {1.0f, 0.0f, 0.0f};
        float limiterGain {1.0f}; // Only the audio callback / offline reader touches this.

        ~Impl()
        {
            if (!ready) return;
            for (const auto& voice : voices) release(*voice);
            if (murmur) release(*murmur);
            if (rolling) release(*rolling);
            for (int i = 0; i < initializedBuses; ++i)
            {
                ma_sound_group_uninit(&buses[static_cast<std::size_t>(i)]);
            }
            ma_engine_uninit(&engine);
        }

        static void release(Voice& voice)
        {
            if (voice.initialized)
            {
                ma_sound_uninit(&voice.sound);
                ma_audio_buffer_uninit(&voice.buffer);
                voice.initialized = false;
            }
        }

        // Linked stereo limiting: instant attack, 80 ms release. No locks
        // or allocations in this real-time callback.
        static void limit(void* user, float* output, ma_uint64 frameCount)
        {
            auto& impl = *static_cast<Impl*>(user);
            constexpr float RELEASE = 1.0f / (0.08f * Audio::SAMPLE_RATE);
            for (ma_uint64 frame = 0; frame < frameCount; ++frame)
            {
                const std::size_t i = static_cast<std::size_t>(frame) * 2;
                const float peak = std::max(std::abs(output[i]), std::abs(output[i + 1]));
                const float target = peak > 0.92f ? 0.92f / peak : 1.0f;
                impl.limiterGain = std::min(target, impl.limiterGain + RELEASE * (1.0f - impl.limiterGain));
                output[i] *= impl.limiterGain;
                output[i + 1] *= impl.limiterGain;
            }
        }

        std::unique_ptr<Voice> createVoice(Bank bank, Bus bus, const Audio::SoundBuffer& data, int strength = 0)
        {
            auto voice = std::make_unique<Voice>();
            ma_audio_buffer_config config = ma_audio_buffer_config_init(ma_format_f32, 1,
                data.samples.size(), data.samples.data(), nullptr);
            config.sampleRate = Audio::SAMPLE_RATE;
            if (ma_audio_buffer_init(&config, &voice->buffer) != MA_SUCCESS) return nullptr;
            if (ma_sound_init_from_data_source(&engine, &voice->buffer, MA_SOUND_FLAG_NO_SPATIALIZATION,
                &buses[static_cast<std::size_t>(bus)], &voice->sound) != MA_SUCCESS)
            {
                ma_audio_buffer_uninit(&voice->buffer);
                return nullptr;
            }
            voice->bank = bank;
            voice->strength = strength;
            voice->initialized = true;
            voice->frames = data.samples.size();
            return voice;
        }

        float panAt(const glm::vec3& position) const
        {
            const glm::vec3 offset = position - listener;
            const float distance = glm::length(offset);
            return distance > 0.001f ? 0.8f * glm::dot(offset / distance, right) : 0.0f;
        }
        float distanceGain(const glm::vec3& position) const
        {
            return 1.0f / (1.0f + 0.18f * glm::length(position - listener));
        }

        void play(Bank bank, float volume, float pitch, float pan, int strength = 0, double delay = 0.0)
        {
            if (!ready || voices.empty() || volume <= 0.001f) return;
            Voice* chosen = nullptr;
            const ma_uint64 now = ma_engine_get_time_in_pcm_frames(&engine);
            const std::size_t offset = std::uniform_int_distribution<std::size_t>(0, voices.size() - 1)(random);
            for (std::size_t n = 0; n < voices.size(); ++n)
            {
                Voice& voice = *voices[(n + offset) % voices.size()];
                if (voice.bank != bank || voice.strength != strength) continue;
                if (!voice.active || (now >= voice.ends && ma_sound_at_end(&voice.sound)))
                {
                    chosen = &voice;
                    break;
                }
                if (chosen == nullptr || voice.volume < chosen->volume ||
                    (voice.volume == chosen->volume && voice.started < chosen->started))
                {
                    chosen = &voice;
                }
            }
            if (chosen == nullptr) return;
            // Only steal within this bank: UI cannot interrupt pockets or applause.
            ma_sound_stop(&chosen->sound);
            ma_sound_seek_to_pcm_frame(&chosen->sound, 0);
            ma_sound_set_volume(&chosen->sound, volume);
            ma_sound_set_pitch(&chosen->sound, std::clamp(pitch, 0.25f, 2.0f));
            ma_sound_set_pan(&chosen->sound, std::clamp(pan, -1.0f, 1.0f));
            const auto wait = static_cast<ma_uint64>(std::max(delay, 0.0) * Audio::SAMPLE_RATE);
            ma_sound_set_start_time_in_pcm_frames(&chosen->sound, now + wait);
            chosen->started = now + wait;
            chosen->ends = chosen->started + static_cast<ma_uint64>(static_cast<double>(chosen->frames) /
                std::clamp(static_cast<double>(pitch), 0.25, 2.0)) + 256;
            chosen->volume = volume;
            chosen->active = ma_sound_start(&chosen->sound) == MA_SUCCESS;
        }
    };

    AudioEngine::AudioEngine(bool enabled, bool offline)
        : m_impl(std::make_unique<Impl>())
    {
        if (!enabled) return;
        Impl& impl = *m_impl;
        ma_engine_config config = ma_engine_config_init();
        config.sampleRate = Audio::SAMPLE_RATE;
        config.channels = 2;
        config.noDevice = offline ? MA_TRUE : MA_FALSE;
        config.onProcess = &Impl::limit;
        config.pProcessUserData = &impl;
        if (ma_engine_init(&config, &impl.engine) != MA_SUCCESS)
        {
            std::cerr << "No audio device; the game will be silent.\n";
            return;
        }
        impl.ready = true;
        impl.offline = offline;
        for (ma_sound_group& bus : impl.buses)
        {
            if (ma_sound_group_init(&impl.engine, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &bus) != MA_SUCCESS)
            {
                std::cerr << "Could not initialize audio mixer.\n";
                m_impl = std::make_unique<Impl>();
                return;
            }
            ++impl.initializedBuses;
        }
        try
        {
            impl.samples = Audio::loadRecordedBank(resolveAssetPath("audio/bank.json"));
            for (int b = 0; b < BANK_COUNT; ++b)
            {
                const auto bank = static_cast<Bank>(b);
                const Bus bus = b >= BANK_APPLAUSE_SHORT && bank != BANK_ROLL ? BUS_CROWD :
                    (b >= BANK_UI_MOVE && b < BANK_APPLAUSE_SHORT ? BUS_INTERFACE : BUS_EFFECTS);
                bool found = false;
                std::array<bool, 3> strengths {};
                for (const auto& sample : impl.samples)
                {
                    if (sample.bank != BANK_NAMES[static_cast<std::size_t>(b)]) continue;
                    if (sample.strength < 0 || sample.strength > 2)
                    {
                        throw std::runtime_error("Invalid sound strength: " + sample.file);
                    }
                    found = true;
                    strengths[static_cast<std::size_t>(sample.strength)] = true;
                    // Enough copies for simultaneous rack contacts and overlapping rails.
                    const int copies = bank == BANK_BALL ? 4 : (bank == BANK_CUSHION || bank == BANK_JAW ? 3 : 2);
                    for (int copy = 0; copy < copies; ++copy)
                    {
                        auto voice = impl.createVoice(bank, bus, sample.pcm, sample.strength);
                        if (!voice) throw std::runtime_error("Cannot initialize sound voice: " + sample.file);
                        if (bank == BANK_MURMUR) impl.murmur = std::move(voice);
                        else if (bank == BANK_ROLL) impl.rolling = std::move(voice);
                        else impl.voices.push_back(std::move(voice));
                        if (bank == BANK_MURMUR || bank == BANK_ROLL) break;
                    }
                }
                if (!found || ((bank == BANK_BALL || bank == BANK_CUSHION || bank == BANK_CUE) &&
                    (!strengths[0] || !strengths[1] || !strengths[2])))
                {
                    throw std::runtime_error("Incomplete sound bank: " + std::string(BANK_NAMES[static_cast<std::size_t>(b)]));
                }
            }
        }
        catch (const std::exception& error)
        {
            std::cerr << "Audio disabled: " << error.what() << '\n';
            m_impl = std::make_unique<Impl>();
            return;
        }
        setVolumes(0.8f, 1.0f, 0.7f);
    }

    AudioEngine::~AudioEngine() = default;
    bool AudioEngine::available() const { return m_impl->ready; }

    void AudioEngine::setVolumes(float master, float effects, float crowd)
    {
        if (!m_impl->ready) return;
        ma_engine_set_volume(&m_impl->engine, std::clamp(master, 0.0f, 1.0f));
        ma_sound_group_set_volume(&m_impl->buses[BUS_EFFECTS], std::clamp(effects, 0.0f, 1.0f));
        ma_sound_group_set_volume(&m_impl->buses[BUS_CROWD], std::clamp(crowd, 0.0f, 1.0f));
        ma_sound_group_set_volume(&m_impl->buses[BUS_INTERFACE], 0.45f);
    }

    void AudioEngine::setListener(const glm::vec3& position, const glm::vec3& right)
    {
        m_impl->listener = position;
        m_impl->right = glm::length(right) > 0.0001f ? glm::normalize(right) : glm::vec3(1.0f, 0.0f, 0.0f);
    }

    void AudioEngine::playShotCue(const Audio::SoundCue& cue, double delaySeconds, float playbackRate)
    {
        if (!m_impl->ready) return;
        const float intensity = std::clamp(cue.intensity, 0.0f, 1.0f);
        const float gain = 0.65f * intensity * m_impl->distanceGain(cue.position);
        const float pitch = std::uniform_real_distribution<float>(0.985f, 1.015f)(m_impl->random) * playbackRate;
        const int strength = intensity < 0.38f ? 0 : (intensity < 0.72f ? 1 : 2);
        const float pan = m_impl->panAt(cue.position);
        switch (cue.kind)
        {
            case Audio::SoundKind::BallBall: m_impl->play(BANK_BALL, gain, pitch, pan, strength, delaySeconds); break;
            case Audio::SoundKind::Cushion: m_impl->play(BANK_CUSHION, 0.85f * gain, pitch, pan, strength, delaySeconds); break;
            case Audio::SoundKind::Pocket: m_impl->play(BANK_POCKET, gain, pitch, pan, 0, delaySeconds); break;
            case Audio::SoundKind::CueStrike: m_impl->play(BANK_CUE, gain, pitch, pan, strength, delaySeconds); break;
            case Audio::SoundKind::PocketJaw: m_impl->play(BANK_JAW, gain, pitch, pan, 0, delaySeconds); break;
        }
    }

    void AudioEngine::playGameplay(GameplaySound sound, const glm::vec3& position)
    {
        switch (sound)
        {
            case GameplaySound::BallPlace: m_impl->play(BANK_PLACE, 0.4f * m_impl->distanceGain(position), 1.0f, m_impl->panAt(position)); break;
            case GameplaySound::Rack: m_impl->play(BANK_RACK, 0.55f, 1.0f, 0.0f); break;
            case GameplaySound::CueSlide: m_impl->play(BANK_SLIDE, 0.16f, 1.0f, m_impl->panAt(position)); break;
            case GameplaySound::ClockTick: m_impl->play(BANK_CLOCK, 0.35f, 1.0f, 0.0f); break;
            case GameplaySound::Foul: m_impl->play(BANK_FOUL, 0.55f, 1.0f, 0.0f); break;
        }
    }

    void AudioEngine::setRolling(float speed, const glm::vec3& position)
    {
        Impl& impl = *m_impl;
        if (!impl.ready || !impl.rolling) return;
        Impl::Voice& voice = *impl.rolling;
        const float gain = 0.18f * std::sqrt(std::clamp(speed / 3.0f, 0.0f, 1.0f)) * impl.distanceGain(position);
        if (!voice.active && gain > 0.001f)
        {
            ma_sound_seek_to_pcm_frame(&voice.sound, 0);
            ma_sound_set_looping(&voice.sound, MA_TRUE);
            ma_sound_set_volume(&voice.sound, gain);
            ma_sound_set_fade_in_milliseconds(&voice.sound, 0.0f, 1.0f, 30);
            voice.active = ma_sound_start(&voice.sound) == MA_SUCCESS;
        }
        ma_sound_set_volume(&voice.sound, gain);
        ma_sound_set_pan(&voice.sound, impl.panAt(position));
        ma_sound_set_pitch(&voice.sound, 0.65f + 0.5f * std::clamp(speed / 3.0f, 0.0f, 1.0f));
    }

    void AudioEngine::stopEffects()
    {
        if (!m_impl->ready) return;
        for (const auto& voice : m_impl->voices)
        {
            if (voice->bank < BANK_UI_MOVE)
            {
                ma_sound_stop(&voice->sound);
                voice->active = false;
            }
        }
        if (m_impl->rolling)
        {
            ma_sound_stop(&m_impl->rolling->sound);
            m_impl->rolling->active = false;
        }
    }

    void AudioEngine::playUi(UiSound sound)
    {
        m_impl->play(sound == UiSound::Move ? BANK_UI_MOVE : BANK_UI_CONFIRM, 0.7f, 1.0f, 0.0f);
    }
    void AudioEngine::playApplause(bool long_)
    {
        m_impl->play(long_ ? BANK_APPLAUSE_LONG : BANK_APPLAUSE_SHORT, 0.7f, 1.0f, 0.0f);
    }
    void AudioEngine::setCrowdLevel(float level)
    {
        Impl& impl = *m_impl;
        if (!impl.ready || !impl.murmur) return;
        const float volume = 0.2f * std::clamp(level, 0.0f, 1.0f);
        if (!impl.murmur->active && volume > 0.0f)
        {
            ma_sound_set_looping(&impl.murmur->sound, MA_TRUE);
            ma_sound_set_volume(&impl.murmur->sound, volume);
            impl.murmur->active = ma_sound_start(&impl.murmur->sound) == MA_SUCCESS;
        }
        ma_sound_set_volume(&impl.murmur->sound, volume);
    }
    void AudioEngine::update()
    {
        if (!m_impl->ready) return;
        const ma_uint64 now = ma_engine_get_time_in_pcm_frames(&m_impl->engine);
        for (const auto& voice : m_impl->voices)
        {
            if (voice->active && now >= voice->ends && ma_sound_at_end(&voice->sound)) voice->active = false;
        }
    }
    bool AudioEngine::renderOffline(std::span<float> stereo)
    {
        if (!m_impl->ready || !m_impl->offline || stereo.size() % 2 != 0) return false;
        return ma_engine_read_pcm_frames(&m_impl->engine, stereo.data(), stereo.size() / 2, nullptr) == MA_SUCCESS;
    }
}
