#include "ai/value_model.h"

#include "ai/features.h"

#include <onnxruntime_cxx_api.h>

#include <array>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace BilliardsSaloon::Ai
{
    struct ValueModel::Impl
    {
        Ort::Env env {ORT_LOGGING_LEVEL_WARNING, "billiards"};
        std::unique_ptr<Ort::Session> session;
        Ort::MemoryInfo memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        bool adaptive {false};
        bool ensemble {false};
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
            model->m_impl->env.DisableTelemetryEvents();
            Ort::SessionOptions options;
            options.SetIntraOpNumThreads(1);   // the planner calls from one worker thread at a time
            options.SetInterOpNumThreads(1);
            options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
            model->m_impl->session = std::make_unique<Ort::Session>(model->m_impl->env, file.c_str(), options);
            const std::size_t outputCount = model->m_impl->session->GetOutputCount();
            if (model->m_impl->session->GetInputCount() != 1 || (outputCount != 1 && outputCount != 2))
                throw std::runtime_error("AI model needs one feature input and a probability output");
            Ort::AllocatorWithDefaultOptions allocator;
            if (std::string(model->m_impl->session->GetInputNameAllocated(0, allocator).get()) != "features")
                throw std::runtime_error("AI input must be named features");
            const auto inputType = model->m_impl->session->GetInputTypeInfo(0);
            const auto input = inputType.GetTensorTypeAndShapeInfo();
            const auto outputType = model->m_impl->session->GetOutputTypeInfo(0);
            const auto output = outputType.GetTensorTypeAndShapeInfo();
            if (input.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
                output.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
                throw std::runtime_error("AI model input/output must be float32 tensors");
            const auto shape = input.GetShape();
            if (shape.size() != 2 || (shape[0] != -1 && shape[0] != 1) || (shape[1] != static_cast<std::int64_t>(FEATURE_COUNT) &&
                                      shape[1] != static_cast<std::int64_t>(ADAPTIVE_FEATURE_COUNT)))
            {
                std::cerr << "Unsupported AI feature schema in " << file.string() << '\n';
                return nullptr;
            }
            model->m_impl->adaptive = shape[1] == static_cast<std::int64_t>(ADAPTIVE_FEATURE_COUNT);
            for (std::size_t i = 0; i < outputCount; ++i)
            {
                const auto type = model->m_impl->session->GetOutputTypeInfo(i);
                const auto tensor = type.GetTensorTypeAndShapeInfo();
                const auto dimensions = tensor.GetShape();
                const std::string name = model->m_impl->session->GetOutputNameAllocated(i, allocator).get();
                const std::string expected = i == 0 ? "win_probability" : "model_disagreement";
                if (name != expected || tensor.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
                    dimensions.size() != 1 || (dimensions[0] != -1 && dimensions[0] != 1))
                    throw std::runtime_error("AI output must be a float32 batch vector named " + expected);
            }
            model->m_impl->ensemble = outputCount == 2;
            // Exercise the session now, so corrupt outputs fail during loading
            // instead of throwing from an asynchronous planner during a match.
            AiTable probe;
            probe.numbers = {0}; probe.positions = {glm::vec2(0.0f)}; probe.pocketed = {false};
            (void)model->estimate(probe, 0, findAiProfile("iris_lindqvist"), findAiProfile("iris_lindqvist"));
            std::cout << "Loaded " << (model->m_impl->adaptive ? "opponent-conditioned" : "legacy")
                      << (model->m_impl->ensemble ? " calibrated ensemble" : "") << " AI value model: " << file.string() << '\n';
            return model;
        }
        catch (const Ort::Exception& error)
        {
            std::cerr << "Could not load the AI value model " << file.string() << ": " << error.what() << '\n';
            return nullptr;
        }
        catch (const std::exception& error)
        {
            std::cerr << "Invalid AI model: " << error.what() << '\n';
            return nullptr;
        }
    }

    double ValueModel::winProbability(const AiTable& table, int player) const
    {
        return winProbability(table, player, findAiProfile("iris_lindqvist"), findAiProfile("iris_lindqvist"));
    }

    double ValueModel::winProbability(const AiTable& table, int player, const AiProfile& own, const AiProfile& opponent) const
    {
        return estimate(table, player, own, opponent).probability;
    }

    ValueEstimate ValueModel::estimate(const AiTable& table, int player, const AiProfile& own, const AiProfile& opponent) const
    {
        std::vector<float> features = m_impl->adaptive ? encodeAdaptiveTable(table, player, own, opponent) : encodeTable(table, player);
        const std::array<std::int64_t, 2> shape {1, static_cast<std::int64_t>(features.size())};
        Ort::Value input = Ort::Value::CreateTensor<float>(m_impl->memory, features.data(), features.size(), shape.data(), shape.size());
        const char* inputs[] = {"features"};
        const char* outputs[] = {"win_probability", "model_disagreement"};
        std::vector<Ort::Value> result = m_impl->session->Run(Ort::RunOptions{nullptr}, inputs, &input, 1, outputs, m_impl->ensemble ? 2 : 1);
        for (const auto& tensor : result)
            if (tensor.GetTensorTypeAndShapeInfo().GetElementCount() != 1)
                throw std::runtime_error("AI model returned an invalid output size");
        const double probability = static_cast<double>(result.front().GetTensorData<float>()[0]);
        if (!std::isfinite(probability) || probability < 0.0 || probability > 1.0)
            throw std::runtime_error("AI model returned an invalid probability");
        const double disagreement = m_impl->ensemble ? static_cast<double>(result[1].GetTensorData<float>()[0]) : 0.0;
        if (!std::isfinite(disagreement) || disagreement < 0.0 || disagreement > 0.5)
            throw std::runtime_error("AI model returned invalid ensemble disagreement");
        return {probability, disagreement, m_impl->ensemble};
    }

    ValueFunction ValueModel::function() const
    {
        return [this](const AiTable& table, int player) { return winProbability(table, player); };
    }

    ValueFunction ValueModel::function(const AiProfile& own, const AiProfile& opponent, int ownSeat) const
    {
        return [this, own, opponent, ownSeat](const AiTable& table, int player)
        {
            return winProbability(table, player, player == ownSeat ? own : opponent, player == ownSeat ? opponent : own);
        };
    }
}
