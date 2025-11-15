#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <future>
#include <optional>
#include <string>
#include <vector>

#include "stemsmith/job_runner.h"
#include "support/fake_session.h"

namespace
{
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
        return test::make_buffer(4);
    };

    model_session_pool pool(
        [](model_profile_id id) -> std::expected<std::unique_ptr<model_session>, std::string> {
            return test::make_stub_session(id);
        });

    const auto output_root = std::filesystem::temp_directory_path() / "stemsmith-job-output";
    std::filesystem::remove_all(output_root);

    separation_engine engine(std::move(pool), output_root, loader, writer);
    job_runner runner({}, std::move(engine), 1);

    const auto input_path_wav = write_temp_wav();
    auto submit_result = runner.submit(input_path_wav);
    ASSERT_TRUE(submit_result.has_value());

    auto future = std::move(submit_result.value());
    auto [input_path, output_dir, status, error] = future.get();

    EXPECT_EQ(status, job_status::completed);
    EXPECT_EQ(input_path, input_path.lexically_normal());
    EXPECT_EQ(output_dir, output_root / input_path.stem());
    EXPECT_FALSE(error.has_value());
    EXPECT_EQ(writes.size(), 6U);
}

TEST(job_runner_test, propagates_engine_errors_to_future)
{
auto loader = [](const std::filesystem::path&) -> std::expected<audio_buffer, std::string> {
        return test::make_buffer(4);
    };

    auto writer = [](const std::filesystem::path&, const audio_buffer&) -> std::expected<void, std::string> {
        return std::unexpected("writer failed");
    };

    model_session_pool pool(
        [](model_profile_id id) -> std::expected<std::unique_ptr<model_session>, std::string> {
            return test::make_stub_session(id);
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
