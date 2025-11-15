#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <future>
#include <optional>
#include <string>
#include <vector>

#include <Eigen/Dense>

#include "stemsmith/job_runner.h"

namespace
{
using stemsmith::audio_buffer;
using stemsmith::job_runner;
using stemsmith::job_result;
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

std::unique_ptr<model_session> make_stub_session(model_profile_id id)
{
    const auto profile = stemsmith::lookup_profile(id);
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
    const auto stem_count = static_cast<int>(profile->stem_count);
    auto inference = [stem_count](const demucscpp::demucs_model&,
                                  const Eigen::MatrixXf&,
                                  demucscpp::ProgressCallback) {
        Eigen::Tensor3dXf tensor(stem_count, 2, 4);
        tensor.setConstant(0.25f);
        return tensor;
    };

    return std::make_unique<model_session>(*profile, std::move(resolver), std::move(loader), std::move(inference));
}

std::filesystem::path write_temp_wav()
{
    const auto temp = std::filesystem::temp_directory_path() / "stemsmith-job.wav";
    std::ofstream out(temp);
    out << "data";
    out.close();
    return temp;
}
} // namespace

namespace stemsmith
{
TEST(job_runner_test, resolves_future_on_completion)
{
    std::vector<std::filesystem::path> writes;
    auto writer = [&](const std::filesystem::path& path, const audio_buffer&) -> std::expected<void, std::string> {
        writes.push_back(path);
        return {};
    };
    auto loader = [](const std::filesystem::path&) -> std::expected<audio_buffer, std::string> {
        return make_buffer(4);
    };

    model_session_pool pool(
        [](model_profile_id id) -> std::expected<std::unique_ptr<model_session>, std::string> {
            return make_stub_session(id);
        });

    const auto output_root = std::filesystem::temp_directory_path() / "stemsmith-job-output";
    std::filesystem::remove_all(output_root);

    separation_engine engine(std::move(pool), output_root, loader, writer);
    job_runner runner({}, std::move(engine), 1);

    const auto input_path = write_temp_wav();
    auto submit_result = runner.submit(input_path);
    ASSERT_TRUE(submit_result.has_value());

    auto future = std::move(submit_result.value());
    auto result = future.get();

    EXPECT_EQ(result.status, job_status::completed);
    EXPECT_EQ(result.input_path, input_path.lexically_normal());
    EXPECT_EQ(result.output_dir, output_root / input_path.stem());
    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(writes.size(), 6U);
}

TEST(job_runner_test, propagates_engine_errors_to_future)
{
auto loader = [](const std::filesystem::path&) -> std::expected<audio_buffer, std::string> {
        return make_buffer(4);
    };

    auto writer = [](const std::filesystem::path&, const audio_buffer&) -> std::expected<void, std::string> {
        return std::unexpected("writer failed");
    };

    model_session_pool pool(
        [](model_profile_id id) -> std::expected<std::unique_ptr<model_session>, std::string> {
            return make_stub_session(id);
        });

    const auto output_root = std::filesystem::temp_directory_path() / "stemsmith-job-output";
    std::filesystem::remove_all(output_root);

    separation_engine engine(std::move(pool), output_root, loader, writer);
    job_runner runner({}, std::move(engine), 1);

    const auto input_path = write_temp_wav();
    auto submit_result = runner.submit(input_path);
    ASSERT_TRUE(submit_result.has_value());

    auto future = std::move(submit_result.value());
    auto result = future.get();
    EXPECT_EQ(result.status, job_status::failed);
    ASSERT_TRUE(result.error.has_value());
    const auto error = result.error.value();
    EXPECT_NE(error.find("writer failed"), std::string::npos);
}
} // namespace stemsmith
