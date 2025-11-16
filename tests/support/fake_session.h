#pragma once

#include <Eigen/Dense>
#include <expected>
#include <filesystem>
#include <memory>
#include <stdexcept>

#include "stemsmith/audio_buffer.h"
#include "stemsmith/job_config.h"
#include "stemsmith/model_session.h"

namespace stemsmith::test
{

inline audio_buffer make_buffer(std::size_t frames)
{
    audio_buffer buffer;
    buffer.sample_rate = demucscpp::SUPPORTED_SAMPLE_RATE;
    buffer.channels = 2;
    buffer.samples.resize(frames * buffer.channels);
    return buffer;
}

inline std::unique_ptr<model_session> make_stub_session(model_profile_id profile_id,
                                                        int frame_count = 4,
                                                        float fill_value = 0.0f)
{
    const auto profile_opt = lookup_profile(profile_id);
    if (!profile_opt)
    {
        throw std::runtime_error("Unknown profile id");
    }

    auto resolver = []() -> std::expected<std::filesystem::path, std::string>
    { return std::filesystem::path{"stub-weights.bin"}; };
    auto loader = [](demucscpp::demucs_model&, const std::filesystem::path&)
    { return std::expected<void, std::string>{}; };

    const auto stem_count = static_cast<int>(profile_opt->stem_count);
    auto inference = [stem_count, frame_count, fill_value](const demucscpp::demucs_model&,
                                                           const Eigen::MatrixXf&,
                                                           demucscpp::ProgressCallback cb)
    {
        if (cb)
        {
            cb(0.0f, "stub");
            cb(0.25f, "stub");
            cb(0.5f, "stub");
            cb(1.0f, "stub");
        }
        Eigen::Tensor3dXf tensor(stem_count, 2, frame_count);
        tensor.setConstant(fill_value);
        return tensor;
    };

    return std::make_unique<model_session>(*profile_opt, std::move(resolver), std::move(loader), std::move(inference));
}

} // namespace stemsmith::test
