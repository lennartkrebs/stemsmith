#include <gtest/gtest.h>

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include "stemsmith/job_catalog.h"
#include "stemsmith/model_session_pool.h"
#include "stemsmith/separation_engine.h"
#include "support/fake_session.h"

namespace stemsmith
{
TEST(separation_engine_test, processes_job_and_writes_stems)
{
    std::vector<std::pair<std::filesystem::path, audio_buffer>> writes;
    auto writer = [&](const std::filesystem::path& path,
                      const audio_buffer& buffer) -> std::expected<void, std::string>
    {
        writes.emplace_back(path, buffer);
        return {};
    };

    auto loader = [](const std::filesystem::path&) -> std::expected<audio_buffer, std::string>
    { return test::make_buffer(4); };

    model_session_pool pool([](model_profile_id profile_id)
                                -> std::expected<std::unique_ptr<model_session>, std::string>
                            { return test::make_stub_session(profile_id); });

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
        [](model_profile_id) -> std::expected<std::unique_ptr<model_session>, std::string>
        { return test::make_stub_session(model_profile_id::balanced_four_stem); });

    auto loader = [](const std::filesystem::path&) -> std::expected<audio_buffer, std::string>
    { return std::unexpected("fail"); };

    auto writer = [](const std::filesystem::path&,
                     const audio_buffer&) -> std::expected<void, std::string> { return {}; };

    separation_engine engine(std::move(pool), std::filesystem::path{"out"}, loader, writer);

    job_descriptor job;
    job.input_path = std::filesystem::path{"/music/song.wav"};

    const auto result = engine.process(job);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("fail"), std::string::npos);
}
} // namespace stemsmith
