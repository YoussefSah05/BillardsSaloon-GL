#include "app/audio_engine.h"

#include <miniaudio.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>

namespace BilliardsSaloon
{
    namespace
    {
        constexpr std::size_t VOICES = 48;

        enum Bank
        {
            BANK_BALL = 0,
            BANK_CUSHION,
            BANK_POCKET,
            BANK_CUE,
            BANK_UI_MOVE,
            BANK_UI_CONFIRM,
            BANK_APPLAUSE_SHORT,
            BANK_APPLAUSE_LONG,
            BANK_MURMUR,
            BANK_COUNT
        };

        enum Bus
        {
            BUS_EFFECTS = 0,
            BUS_CROWD,
            BUS_INTERFACE,
            BUS_COUNT
        };
    }

    struct AudioEngine::Impl
    {
        // A playing sound: its own cursor over shared sample data.
        struct Voice
        {
            ma_audio_buffer buffer {};
            ma_sound sound {};
            bool active {false};
        };

        ma_engine engine {};
        bool ready {false};
        std::array<ma_sound_group, BUS_COUNT> buses {};
        std::array<std::vector<Audio::SoundBuffer>, BANK_COUNT> banks;
        std::array<std::unique_ptr<Voice>, VOICES> voices;
        std::unique_ptr<Voice> murmur;
        std::mt19937 random {1234};
        glm::vec3 listener {0.0f};
        glm::vec3 right {1.0f, 0.0f, 0.0f};
        float master {0.8f};
        float effects {1.0f};
        float crowd {0.7f};
        float crowdLevel {0.0f};

        void release(Voice& voice)
        {
            if (voice.active)
            {
                ma_sound_uninit(&voice.sound);
                ma_audio_buffer_uninit(&voice.buffer);
                voice.active = false;
            }
        }

        bool start(Voice& voice, const Audio::SoundBuffer& data, Bus bus, float volume, float pitch, float pan, bool loop)
        {
            const ma_audio_buffer_config config = ma_audio_buffer_config_init(
                ma_format_f32, 1, data.samples.size(), data.samples.data(), nullptr);
            if (ma_audio_buffer_init(&config, &voice.buffer) != MA_SUCCESS)
            {
                return false;
            }
            if (ma_sound_init_from_data_source(&engine, &voice.buffer, MA_SOUND_FLAG_NO_SPATIALIZATION,
                                               &buses[static_cast<std::size_t>(bus)], &voice.sound) != MA_SUCCESS)
            {
                ma_audio_buffer_uninit(&voice.buffer);
                return false;
            }
            voice.active = true;
            ma_sound_set_volume(&voice.sound, volume);
            ma_sound_set_pitch(&voice.sound, pitch);
            ma_sound_set_pan(&voice.sound, pan);
            ma_sound_set_looping(&voice.sound, loop ? MA_TRUE : MA_FALSE);
            ma_sound_start(&voice.sound);
            return true;
        }

        void play(Bank bank, Bus bus, float volume, float pitch, float pan)
        {
            if (!ready || banks[bank].empty() || (volume <= 0.001f))
            {
                return;
            }
            // A free voice, else the oldest-started one is reused.
            Voice* chosen = nullptr;
            for (std::unique_ptr<Voice>& voice : voices)
            {
                if (!voice->active)
                {
                    chosen = voice.get();
                    break;
                }
            }
            if (chosen == nullptr)
            {
                chosen = voices[std::uniform_int_distribution<std::size_t>(0, VOICES - 1)(random)].get();
                release(*chosen);
            }
            const std::vector<Audio::SoundBuffer>& variants = banks[bank];
            const auto& data = variants[std::uniform_int_distribution<std::size_t>(0, variants.size() - 1)(random)];
            (void)start(*chosen, data, bus, volume, pitch, std::clamp(pan, -1.0f, 1.0f), false);
        }

        void applyVolumes()
        {
            if (!ready)
            {
                return;
            }
            ma_engine_set_volume(&engine, master);
            ma_sound_group_set_volume(&buses[BUS_EFFECTS], effects);
            ma_sound_group_set_volume(&buses[BUS_CROWD], crowd);
            ma_sound_group_set_volume(&buses[BUS_INTERFACE], 0.6f);
            if (murmur && murmur->active)
            {
                ma_sound_set_volume(&murmur->sound, 0.35f * crowdLevel);
            }
        }
    };

    AudioEngine::AudioEngine(bool enabled)
        : m_impl(std::make_unique<Impl>())
    {
        if (!enabled)
        {
            return;
        }

        ma_engine_config config = ma_engine_config_init();
        config.sampleRate = Audio::SAMPLE_RATE;
        if (ma_engine_init(&config, &m_impl->engine) != MA_SUCCESS)
        {
            std::cerr << "No audio device; the game will be silent.\n";
            return;
        }
        for (ma_sound_group& bus : m_impl->buses)
        {
            ma_sound_group_init(&m_impl->engine, 0, nullptr, &bus);
        }
        m_impl->ready = true;

        // Several takes of each impact, so repeated hits do not sound identical.
        for (std::uint32_t seed = 1; seed <= 6; ++seed)
        {
            m_impl->banks[BANK_BALL].push_back(Audio::ballClick(seed));
            m_impl->banks[BANK_CUSHION].push_back(Audio::cushionThud(seed + 100));
            m_impl->banks[BANK_POCKET].push_back(Audio::pocketDrop(seed + 200));
            m_impl->banks[BANK_CUE].push_back(Audio::cueStrike(seed + 300));
        }
        m_impl->banks[BANK_UI_MOVE].push_back(Audio::uiTick());
        m_impl->banks[BANK_UI_CONFIRM].push_back(Audio::uiConfirm());
        m_impl->banks[BANK_APPLAUSE_SHORT].push_back(Audio::applause(3.5, 41));
        m_impl->banks[BANK_APPLAUSE_LONG].push_back(Audio::applause(7.0, 42));
        m_impl->banks[BANK_MURMUR].push_back(Audio::crowdMurmur(12.0, 43));

        for (std::unique_ptr<Impl::Voice>& voice : m_impl->voices)
        {
            voice = std::make_unique<Impl::Voice>();
        }
        m_impl->murmur = std::make_unique<Impl::Voice>();
        m_impl->applyVolumes();
    }

    AudioEngine::~AudioEngine()
    {
        if (!m_impl->ready)
        {
            return;
        }
        for (std::unique_ptr<Impl::Voice>& voice : m_impl->voices)
        {
            m_impl->release(*voice);
        }
        m_impl->release(*m_impl->murmur);
        for (ma_sound_group& bus : m_impl->buses)
        {
            ma_sound_group_uninit(&bus);
        }
        ma_engine_uninit(&m_impl->engine);
    }

    bool AudioEngine::available() const
    {
        return m_impl->ready;
    }

    void AudioEngine::setVolumes(float master, float effects, float crowd)
    {
        m_impl->master = std::clamp(master, 0.0f, 1.0f);
        m_impl->effects = std::clamp(effects, 0.0f, 1.0f);
        m_impl->crowd = std::clamp(crowd, 0.0f, 1.0f);
        m_impl->applyVolumes();
    }

    void AudioEngine::setListener(const glm::vec3& position, const glm::vec3& right)
    {
        m_impl->listener = position;
        m_impl->right = (glm::length(right) > 1.0e-4f) ? glm::normalize(right) : glm::vec3(1.0f, 0.0f, 0.0f);
    }

    void AudioEngine::playShotCue(const Audio::SoundCue& cue)
    {
        const glm::vec3 offset = cue.position - m_impl->listener;
        const float distance = glm::length(offset);
        const float pan = (distance > 1.0e-3f) ? 0.8f * glm::dot(offset / distance, m_impl->right) : 0.0f;
        const float gain = cue.intensity / (1.0f + 0.35f * distance);
        // Harder hits ring a touch higher; each hit varies a little.
        const float pitch = (0.95f + 0.1f * cue.intensity) * std::uniform_real_distribution<float>(0.97f, 1.03f)(m_impl->random);

        switch (cue.kind)
        {
            case Audio::SoundKind::BallBall:
                m_impl->play(BANK_BALL, BUS_EFFECTS, gain, pitch, pan);
                break;
            case Audio::SoundKind::Cushion:
                m_impl->play(BANK_CUSHION, BUS_EFFECTS, gain * 0.9f, pitch, pan);
                break;
            case Audio::SoundKind::Pocket:
                m_impl->play(BANK_POCKET, BUS_EFFECTS, gain, pitch, pan);
                break;
            case Audio::SoundKind::CueStrike:
                m_impl->play(BANK_CUE, BUS_EFFECTS, 0.4f + 0.6f * cue.intensity, pitch, pan);
                break;
        }
    }

    void AudioEngine::playUi(UiSound sound)
    {
        m_impl->play((sound == UiSound::Move) ? BANK_UI_MOVE : BANK_UI_CONFIRM, BUS_INTERFACE, 1.0f, 1.0f, 0.0f);
    }

    void AudioEngine::playApplause(bool long_)
    {
        m_impl->play(long_ ? BANK_APPLAUSE_LONG : BANK_APPLAUSE_SHORT, BUS_CROWD, 0.9f, 1.0f, 0.0f);
    }

    void AudioEngine::setCrowdLevel(float level)
    {
        Impl& impl = *m_impl;
        impl.crowdLevel = std::clamp(level, 0.0f, 1.0f);
        if (!impl.ready)
        {
            return;
        }
        if ((impl.crowdLevel > 0.0f) && !impl.murmur->active)
        {
            (void)impl.start(*impl.murmur, impl.banks[BANK_MURMUR].front(), BUS_CROWD, 0.0f, 1.0f, 0.0f, true);
        }
        impl.applyVolumes();
    }

    void AudioEngine::update()
    {
        if (!m_impl->ready)
        {
            return;
        }
        for (std::unique_ptr<Impl::Voice>& voice : m_impl->voices)
        {
            if (voice->active && ma_sound_at_end(&voice->sound))
            {
                m_impl->release(*voice);
            }
        }
    }
}
