#include "ai/value_model.h"

#include "ai/features.h"

#include <onnxruntime_cxx_api.h>

#include <array>
#include <iostream>

namespace BilliardsSaloon::Ai
{
    struct ValueModel::Impl
    {
        Ort::Env env {ORT_LOGGING_LEVEL_WARNING, "billiards"};
        std::unique_ptr<Ort::Session> session;
        Ort::MemoryInfo memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    };

    ValueModel::ValueModel()
        : m_impl(std::make_unique<Impl>())
    {
    }

    ValueModel::~ValueModel() = default;

    std::unique_ptr<ValueModel> ValueModel::load(const std::filesystem::path& file)
    {
        if (!std::filesystem::exists(file))
        {
            std::cerr << "No AI value model at " << file.string() << "; using the classical evaluation.\n";
            return nullptr;
        }
        try
        {
            std::unique_ptr<ValueModel> model(new ValueModel());
            Ort::SessionOptions options;
            options.SetIntraOpNumThreads(1);   // the planner calls from one worker thread at a time
            options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
            model->m_impl->session = std::make_unique<Ort::Session>(model->m_impl->env, file.c_str(), options);
            return model;
        }
        catch (const Ort::Exception& error)
        {
            std::cerr << "Could not load the AI value model " << file.string() << ": " << error.what() << '\n';
            return nullptr;
        }
    }

    double ValueModel::winProbability(const AiTable& table, int player) const
    {
        std::vector<float> features = encodeTable(table, player);
        const std::array<std::int64_t, 2> shape {1, static_cast<std::int64_t>(features.size())};
        Ort::Value input = Ort::Value::CreateTensor<float>(m_impl->memory, features.data(), features.size(), shape.data(), shape.size());
        const char* inputs[] = {"features"};
        const char* outputs[] = {"win_probability"};
        std::vector<Ort::Value> result = m_impl->session->Run(Ort::RunOptions{nullptr}, inputs, &input, 1, outputs, 1);
        return static_cast<double>(result.front().GetTensorData<float>()[0]);
    }

    ValueFunction ValueModel::function() const
    {
        return [this](const AiTable& table, int player) { return winProbability(table, player); };
    }
}
