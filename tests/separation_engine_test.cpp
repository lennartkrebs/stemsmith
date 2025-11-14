#include <gtest/gtest.h>

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include <Eigen/Dense>

#include "stemsmith/job_catalog.h"
#include "stemsmith/model_session_pool.h"
#include "stemsmith/separation_engine.h"

namespace
{
using stemsmith::audio_buffer;
using stemsmith::job_config;
using stemsmith::job_descriptor;
using stemsmith::model_profile_id;
using stemsmith::model_session;
using stemsmith::model_session_pool;
using stemsmith::separation_engine;

audio_buffer make_buffer(std::size_t frames)
{
    audio_buffer buffer;
    buffer.sample_rate = demucscpp::SUPPORTED_SAMPLE_RATE;
    buffer.channels = 2;
    buffer.samples.resize(frames * buffer.channels);
    return buffer;
}

std::unique_ptr<model_session> make_stub_session(model_profile_id profile_id)
{
    const auto profile = stemsmith::lookup_profile(profile_id);
    if (!profile)
    {
        throw std::runtime_error("unknown profile");
    }

    auto resolver = []() -> std::expected<std::filesystem::path, std::string> {
        return std::filesystem::path{"weights.bin"};
    };
    auto loader = [](demucscpp::demucs_model&, const std::filesystem::path&) {
        return std::expected<void, std::string>{};
    };
    auto inference = [](const demucscpp::demucs_model&,
                        const Eigen::MatrixXf&,
                        demucscpp::ProgressCallback) {
        Eigen::Tensor3dXf tensor(4, 2, 4);
        tensor.setConstant(0.5f);
        return tensor;
    };

    return std::make_unique<model_session>(*profile, std::move(resolver), std::move(loader), std::move(inference));
}
} // namespace

namespace stemsmith
{
TEST(separation_engine_test, processes_job_and_writes_stems)
{
    std::vector<std::pair<std::filesystem::path, audio_buffer>> writes;
    auto writer = [&](const std::filesystem::path& path, const audio_buffer& buffer) -> std::expected<void, std::string> {
        writes.emplace_back(path, buffer);
        return {};
    };
    auto loader = [](const std::filesystem::path&) -> std::expected<audio_buffer, std::string> {
        return make_buffer(4);
    };

    model_session_pool pool(
        [](model_profile_id profile_id) -> std::expected<std::unique_ptr<model_session>, std::string> {
            return make_stub_session(profile_id);
        });

    const auto output_root = std::filesystem::temp_directory_path() / "stemsmith-sep-test";
    std::filesystem::remove_all(output_root);
    separation_engine engine(std::move(pool), output_root, loader, writer);

    job_descriptor job;
    job.input_path = std::filesystem::path{"/music/song.wav"};
    job.config.profile = model_profile_id::balanced_four_stem;
    job.config.stems_filter = {"vocals", "drums"};

    const auto result = engine.process(job);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(writes.size(), 2U);
    EXPECT_EQ(writes[0].first.filename(), std::filesystem::path{"vocals.wav"});
    EXPECT_EQ(writes[1].first.filename(), std::filesystem::path{"drums.wav"});
}

TEST(separation_engine_test, propagates_loader_errors)
{
    model_session_pool pool(
        [](model_profile_id) -> std::expected<std::unique_ptr<model_session>, std::string> {
            return make_stub_session(model_profile_id::balanced_four_stem);
        });

    auto loader = [](const std::filesystem::path&) -> std::expected<audio_buffer, std::string> {
        return std::unexpected("fail");
    };
    auto writer = [](const std::filesystem::path&, const audio_buffer&) -> std::expected<void, std::string> {
        return {};
    };

    separation_engine engine(std::move(pool), std::filesystem::path{"out"}, loader, writer);

    job_descriptor job;
    job.input_path = std::filesystem::path{"/music/song.wav"};

    const auto result = engine.process(job);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("fail"), std::string::npos);
}
} // namespace stemsmith
