#include <gtest/gtest.h>

#include <Eigen/Dense>

#include <array>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include "stemsmith/audio_buffer.h"
#include "stemsmith/job_config.h"
#include "stemsmith/model_session.h"

namespace
{
using stemsmith::audio_buffer;
using stemsmith::model_profile_id;
using stemsmith::model_session;
using stemsmith::separation_result;

audio_buffer make_audio_buffer(std::size_t frames)
{
    audio_buffer buffer;
    buffer.sample_rate = demucscpp::SUPPORTED_SAMPLE_RATE;
    buffer.channels = 2;
    buffer.samples.resize(frames * buffer.channels);
    for (std::size_t i = 0; i < frames; ++i)
    {
        buffer.samples[2 * i] = static_cast<float>(i);
        buffer.samples[2 * i + 1] = static_cast<float>(i + 1);
    }
    return buffer;
}

Eigen::Tensor3dXf make_tensor(std::size_t targets, std::size_t frames)
{
    Eigen::Tensor3dXf tensor(static_cast<int>(targets), 2, static_cast<int>(frames));
    for (std::size_t t = 0; t < targets; ++t)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            for (std::size_t f = 0; f < frames; ++f)
            {
                tensor(static_cast<int>(t), ch, static_cast<int>(f)) =
                    static_cast<float>(t + ch + f);
            }
        }
    }
    return tensor;
}

model_session make_session(const stemsmith::model_profile& profile,
                           Eigen::Tensor3dXf outputs)
{
    auto resolver = []() -> std::expected<std::filesystem::path, std::string> {
        return std::filesystem::path{"unused.bin"};
    };
    auto loader = [](demucscpp::demucs_model&, const std::filesystem::path&) {
        return std::expected<void, std::string>{};
    };
    auto inference =
        [tensor = std::move(outputs)](const demucscpp::demucs_model&,
                                      const Eigen::MatrixXf&,
                                      demucscpp::ProgressCallback) mutable {
            return tensor;
        };

    return model_session(profile, std::move(resolver), std::move(loader), std::move(inference));
}
} // namespace

namespace stemsmith
{
TEST(model_session_test, separates_requested_stems)
{
    const auto profile_opt = lookup_profile(model_profile_id::balanced_four_stem);
    ASSERT_TRUE(profile_opt.has_value());
    const auto profile = profile_opt.value();

    auto session = make_session(profile, make_tensor(4, 3));
    const auto input = make_audio_buffer(3);
    const std::array<std::string_view, 2> filter{"vocals", "bass"};
    const auto result = session.separate(input, filter);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->stems.size(), filter.size());
    EXPECT_EQ(result->stems[0].first, "vocals");
    EXPECT_EQ(result->stems[1].first, "bass");
    for (const auto& [name, buffer] : result->stems)
    {
        EXPECT_EQ(buffer.sample_rate, demucscpp::SUPPORTED_SAMPLE_RATE);
        EXPECT_EQ(buffer.channels, 2U);
        EXPECT_EQ(buffer.samples.size(), input.samples.size());
        (void)name;
    }
}

TEST(model_session_test, rejects_unknown_stem_request)
{
    const auto profile_opt = lookup_profile(model_profile_id::balanced_four_stem);
    ASSERT_TRUE(profile_opt.has_value());
    auto session = make_session(profile_opt.value(), make_tensor(4, 2));
    const auto input = make_audio_buffer(2);
    const std::array<std::string_view, 1> filter{"flute"};
    const auto result = session.separate(input, filter);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("Unknown stem"), std::string::npos);
}

TEST(model_session_test, rejects_invalid_channels)
{
    const auto profile_opt = lookup_profile(model_profile_id::balanced_four_stem);
    ASSERT_TRUE(profile_opt.has_value());
    auto session = make_session(profile_opt.value(), make_tensor(4, 2));

    audio_buffer buffer;
    buffer.sample_rate = demucscpp::SUPPORTED_SAMPLE_RATE;
    buffer.channels = 1;
    buffer.samples = {0.0f, 1.0f};

    const auto result = session.separate(buffer);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("stereo"), std::string::npos);
}
} // namespace stemsmith
