#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "stemsmith/job_config.h"

namespace
{
std::filesystem::path fixture_path(const std::string& relative)
{
    return std::filesystem::path{STEMSMITH_TEST_DATA_DIR} / relative;
}
} // namespace

namespace stemsmith
{
TEST(JobConfigTest, DefaultsAreApplied)
{
    const job_config config;

    EXPECT_EQ(config.profile, model_profile_id::balanced_six_stem);
    EXPECT_TRUE(config.stems_filter.empty());

    const auto& profile = lookup_profile(config.profile);
    EXPECT_EQ(profile.key, "balanced-six-stem");

    const std::vector<std::string> expected{"vocals", "drums", "bass", "piano", "guitar", "other"};
    EXPECT_EQ(config.resolved_stems(), expected);
    EXPECT_EQ(config.cache_root, std::filesystem::path{"build/cache"});
}

TEST(JobConfigTest, LoadsOverridesFromJson)
{
    setenv("STEMSMITH_CACHE", "/tmp/stemsmith", 1);

    const auto result =
        job_config::from_file(fixture_path("job_config/basic.json"));
    ASSERT_TRUE(result.has_value());

    const auto& config = result.value();
    const std::vector<std::string> expected{"vocals", "guitar"};
    EXPECT_EQ(config.profile, model_profile_id::balanced_six_stem);
    EXPECT_EQ(config.stems_filter, expected);
    EXPECT_EQ(config.resolved_stems(), expected);
    EXPECT_EQ(config.cache_root, std::filesystem::path{"/tmp/stemsmith/models"});
}

TEST(JobConfigTest, ResolvesProfileStemsWhenFilterEmpty)
{
    const auto result =
        job_config::from_file(fixture_path("job_config/four_stem.json"));
    ASSERT_TRUE(result.has_value());

    const auto& config = result.value();
    EXPECT_EQ(config.profile, model_profile_id::balanced_four_stem);
    EXPECT_TRUE(config.stems_filter.empty());

    const std::vector<std::string> expected{"vocals", "drums", "bass", "other"};
    EXPECT_EQ(config.resolved_stems(), expected);
}

TEST(JobConfigTest, RejectsInvalidConfig)
{
    const auto result =
        job_config::from_file(fixture_path("job_config/invalid.json"));
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("stems entries must be strings"), std::string::npos);
}

TEST(JobConfigTest, RejectsUnsupportedStemNames)
{
    const auto result =
        job_config::from_file(fixture_path("job_config/unsupported.json"));
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("Unsupported stem"), std::string::npos);
}

TEST(JobConfigTest, IgnoresUnknownKeys)
{
    const auto result =
        job_config::from_file(fixture_path("job_config/unknown_key.json"));
    ASSERT_TRUE(result.has_value());

    const auto& [profile, stems_filter, cache_root] = result.value();
    EXPECT_EQ(profile, model_profile_id::balanced_six_stem);
    EXPECT_TRUE(stems_filter.empty());
    EXPECT_EQ(cache_root, std::filesystem::path{"build/cache"});
}

TEST(JobConfigTest, RejectsUnknownModel)
{
    const auto result = job_config::from_file(fixture_path("job_config/unknown_model.json"));
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("Unknown model profile"), std::string::npos);
}
} // namespace stemsmith
