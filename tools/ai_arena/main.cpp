// Plays the classical AI against the same player with the learned value
// network, alternating seats and breaks, and reports the learned version's
// win rate with a 95% interval.
//
//   ./build/bs_ai_arena assets/models/value.onnx [frames] [profile] [discipline]

#include "ai/environment.h"
#include "ai/value_model.h"

#include <atomic>
#include <cmath>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace BilliardsSaloon;

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: bs_ai_arena MODEL.onnx [frames=200] [profile=dani_reyes] [eight|nine|ten=nine]\n";
        return 2;
    }
    const std::unique_ptr<Ai::ValueModel> model = Ai::ValueModel::load(argv[1]);
    if (!model)
    {
        return 1;
    }
    const int frames = (argc > 2) ? std::stoi(argv[2]) : 200;
    const Ai::AiProfile profile = Ai::findAiProfile((argc > 3) ? argv[3] : "dani_reyes");
    const std::string game = (argc > 4) ? argv[4] : "nine";
    const GameDiscipline discipline = (game == "eight") ? GameDiscipline::EightBall
        : (game == "ten") ? GameDiscipline::TenBall : GameDiscipline::NineBall;

    std::atomic<int> next {0};
    std::atomic<int> learnedWins {0};
    std::atomic<int> played {0};
    const unsigned threads = std::max(1U, std::thread::hardware_concurrency() - 1);
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < threads; ++t)
    {
        pool.emplace_back([&]()
        {
            Ai::Environment env(discipline, 1);
            for (int frame = next++; frame < frames; frame = next++)
            {
                // Seats and breaks alternate so neither side has the break more often.
                const int learnedSeat = frame % 2;
                const Ai::ValueFunction learned = model->function(profile, profile, learnedSeat);
                env.reset(static_cast<std::uint32_t>(50000 + frame), (frame / 2) % 2);
                for (int move = 0; (move < 250) && (env.decision() != Ai::Decision::FrameOver); ++move)
                {
                    const bool useLearned = env.toAct() == learnedSeat;
                    (void)env.playClassical(profile, static_cast<std::uint32_t>(frame * 1000 + move), true,
                                            useLearned ? learned : Ai::ValueFunction{});
                }
                if (env.frame().winner >= 0)
                {
                    learnedWins += (env.frame().winner == learnedSeat) ? 1 : 0;
                    ++played;
                }
            }
        });
    }
    for (std::thread& t : pool)
    {
        t.join();
    }

    const double n = played.load();
    const double p = learnedWins.load() / std::max(n, 1.0);
    const double half = 1.96 * std::sqrt(p * (1.0 - p) / std::max(n, 1.0));
    std::cout << profile.name << ", " << game << "-ball, " << played.load() << " frames: learned value wins "
              << learnedWins.load() << " (" << 100.0 * p << "% +/- " << 100.0 * half << "%)\n";
    return 0;
}
