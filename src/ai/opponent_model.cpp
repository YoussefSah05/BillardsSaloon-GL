#include "ai/opponent_model.h"
#include "core/asset_paths.h"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>

namespace Septhuit::Ai
{
    namespace
    {
        constexpr std::array<float, 5> PRIOR {0.1f, 0.2f, 0.4f, 0.2f, 0.1f};
        struct Likelihood
        {
            std::array<float, 3> pot {1.2f, 1.2f, 1.2f};
            std::array<float, 3> foul {-2.0f, -2.0f, -2.0f};
            float potSkill {2.0f};
            float potEase {0.8f};
            float foulSkill {-1.0f};
            float foulEase {-0.35f};
        };

        const Likelihood& likelihood()
        {
            static const Likelihood calibrated = []()
            {
                Likelihood result;
                std::ifstream stream(resolveAssetPathOrEmpty("data/ai/skill_calibration.json"));
                if (!stream) return result;
                try
                {
                    const auto data = nlohmann::json::parse(stream);
                    if (data.at("version").get<int>() != 1) return result;
                    Likelihood loaded;
                    loaded.pot = data.at("pot").at("intercepts").get<std::array<float, 3>>();
                    loaded.foul = data.at("foul").at("intercepts").get<std::array<float, 3>>();
                    loaded.potSkill = data.at("pot").at("skill").get<float>();
                    loaded.potEase = data.at("pot").at("log_ease").get<float>();
                    loaded.foulSkill = data.at("foul").at("skill").get<float>();
                    loaded.foulEase = data.at("foul").at("log_ease").get<float>();
                    for (const float value : loaded.pot) if (!std::isfinite(value)) return result;
                    for (const float value : loaded.foul) if (!std::isfinite(value)) return result;
                    if (!std::isfinite(loaded.potSkill) || !std::isfinite(loaded.potEase) ||
                        !std::isfinite(loaded.foulSkill) || !std::isfinite(loaded.foulEase)) return result;
                    if (loaded.potSkill < 0.0f || loaded.potEase < 0.0f || loaded.foulSkill > 0.0f || loaded.foulEase > 0.0f) return result;
                    return loaded;
                }
                catch (const std::exception&) { return result; }
            }();
            return calibrated;
        }

        float sigmoid(float value) { return 1.0f / (1.0f + std::exp(-std::clamp(value, -20.0f, 20.0f))); }
    }
    AiProfile profileAtSkill(float skill)
    {
        const std::array<const char*, 5> ladder {"sam_whitlock", "dani_reyes", "iris_lindqvist", "marta_okafor", "viktor_hale"};
        const float index = std::clamp(skill, 0.0f, 1.0f) * 4.0f;
        const auto lower = static_cast<std::size_t>(index);
        const AiProfile& a = findAiProfile(ladder[lower]);
        const AiProfile& b = findAiProfile(ladder[std::min(lower + 1, ladder.size() - 1)]);
        const float t = index - static_cast<float>(lower);
        AiProfile result = a;
        result.aimNoiseDegrees = a.aimNoiseDegrees + t * (b.aimNoiseDegrees - a.aimNoiseDegrees);
        result.powerNoise = a.powerNoise + t * (b.powerNoise - a.powerNoise);
        result.spinNoise = a.spinNoise + t * (b.spinNoise - a.spinNoise);
        result.samples = static_cast<int>(std::round(static_cast<float>(a.samples) + t * static_cast<float>(b.samples - a.samples)));
        result.safetyBias = a.safetyBias + t * (b.safetyBias - a.safetyBias);
        result.powerStyle = a.powerStyle + t * (b.powerStyle - a.powerStyle);
        result.breakPower = a.breakPower + t * (b.breakPower - a.breakPower);
        return result;
    }

    void OpponentModel::reset()
    {
        m_skill = 0.5f;
        m_observations = 0;
        m_belief = PRIOR;
    }

    void OpponentModel::observe(float potEase, bool potted, bool foul, bool breakShot, GameDiscipline discipline)
    {
        if (!std::isfinite(potEase) || breakShot || (!foul && potEase < 0.02f)) return;
        ++m_observations;
        const Likelihood& fit = likelihood();
        const auto game = static_cast<std::size_t>(discipline);
        const float logEase = std::log(std::clamp(potEase, 0.02f, 1.0f));
        float sum = 0.0f;
        for (std::size_t level = 0; level < m_belief.size(); ++level)
        {
            const float skill = static_cast<float>(level) / 4.0f;
            const float chancePot = sigmoid(fit.pot[game] + fit.potSkill*skill + fit.potEase*logEase);
            const float chanceFoul = sigmoid(fit.foul[game] + fit.foulSkill*skill + fit.foulEase*logEase);
            const float observation = foul ? chanceFoul : (1.0f-chanceFoul) * (potted ? chancePot : 1.0f-chancePot);
            // Gentle forgetting allows a player's level to change later in a
            // match; posterior confidence never becomes irreversible.
            m_belief[level] = (0.98f*m_belief[level] + 0.02f*PRIOR[level]) * std::max(observation, 1e-5f);
            sum += m_belief[level];
        }
        float estimate = 0.0f;
        for (std::size_t level = 0; level < m_belief.size(); ++level)
        {
            m_belief[level] /= sum;
            estimate += m_belief[level] * static_cast<float>(level) / 4.0f;
        }
        m_skill = std::clamp(m_skill + std::clamp(0.4f*(estimate-m_skill), -0.03f, 0.03f), 0.0f, 1.0f);
    }

    AiProfile OpponentModel::estimatedProfile() const { return profileAtSkill(m_skill); }

    AiProfile OpponentModel::adapt(const AiProfile& personality) const
    {
        if (!personality.adaptive) return personality;
        const AiProfile skillProfile = profileAtSkill(std::min(1.0f, m_skill + 0.08f));
        AiProfile result = personality;
        result.aimNoiseDegrees = skillProfile.aimNoiseDegrees;
        result.powerNoise = skillProfile.powerNoise;
        result.spinNoise = skillProfile.spinNoise;
        result.samples = skillProfile.samples;
        return result;
    }
}
