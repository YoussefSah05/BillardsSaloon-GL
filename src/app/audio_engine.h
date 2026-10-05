#pragma once

#include "audio/shot_sounds.h"
#include "audio/sound_synth.h"

#include <glm/glm.hpp>

#include <array>
#include <memory>
#include <vector>

namespace BilliardsSaloon
{
    enum class UiSound
    {
        Move,
        Confirm
    };

    // Plays the game's synthesised sounds through miniaudio: shot sounds placed
    // in stereo from the camera, a crowd bed, applause and interface ticks.
    // Without an audio device (or when muted) every call does nothing.
    class AudioEngine
    {
    public:
        explicit AudioEngine(bool enabled);
        ~AudioEngine();
        AudioEngine(const AudioEngine&) = delete;
        AudioEngine& operator=(const AudioEngine&) = delete;

        [[nodiscard]] bool available() const;

        // 0..1 each; effects and crowd are scaled by master.
        void setVolumes(float master, float effects, float crowd);

        // The listener: where the camera is and which way is right.
        void setListener(const glm::vec3& position, const glm::vec3& right);

        void playShotCue(const Audio::SoundCue& cue);
        void playUi(UiSound sound);
        void playApplause(bool long_);

        // The crowd's murmur; level 0 silences it.
        void setCrowdLevel(float level);

        // Frees voices that finished playing; call once per frame.
        void update();

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
