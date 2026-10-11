#pragma once

#include "audio/shot_sounds.h"
#include "audio/recorded_bank.h"

#include <glm/glm.hpp>

#include <array>
#include <memory>
#include <span>
#include <vector>

namespace BilliardsSaloon
{
    enum class UiSound
    {
        Move,
        Confirm
    };

    enum class GameplaySound
    {
        BallPlace,
        Rack,
        CueSlide,
        ClockTick,
        Foul
    };

    // Plays the game's bundled recordings through miniaudio: shot sounds placed
    // in stereo from the camera, a crowd bed, applause and interface ticks.
    // Without an audio device (or when muted) every call does nothing.
    class AudioEngine
    {
    public:
        explicit AudioEngine(bool enabled, bool offline = false);
        ~AudioEngine();
        AudioEngine(const AudioEngine&) = delete;
        AudioEngine& operator=(const AudioEngine&) = delete;

        [[nodiscard]] bool available() const;

        // 0..1 each; effects and crowd are scaled by master.
        void setVolumes(float master, float effects, float crowd);

        // The listener: where the camera is and which way is right.
        void setListener(const glm::vec3& position, const glm::vec3& right);

        void playShotCue(const Audio::SoundCue& cue, double delaySeconds = 0.0, float playbackRate = 1.0f);
        void playGameplay(GameplaySound sound, const glm::vec3& position = glm::vec3(0.0f));
        void setRolling(float speed, const glm::vec3& position);
        void stopEffects();
        void playUi(UiSound sound);
        void playApplause(bool long_);

        // The crowd's murmur; level 0 silences it.
        void setCrowdLevel(float level);

        // Marks finished voices reusable; call once per frame.
        void update();
        // Offline tools/tests use the exact game mixer without an audio device.
        bool renderOffline(std::span<float> interleavedStereo);

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
