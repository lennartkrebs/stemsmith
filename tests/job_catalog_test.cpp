#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <unordered_set>
#include <vector>

#include "job_catalog.h"

namespace
{
class fake_filesystem
{
public:
    fake_filesystem() = default;

    fake_filesystem(const std::initializer_list<std::filesystem::path> entries)
    {
        for (const auto& path : entries)
        {
            files_.insert(path.lexically_normal());
        }
    }

    [[nodiscard]] bool exists(const std::filesystem::path& path) const
    {
        return files_.contains(path.lexically_normal());
    }

private:
    std::unordered_set<std::filesystem::path> files_;
};
} // namespace

namespace stemsmith
{
TEST(job_catalog_test, enqueues_files_with_base_config)
{
    const fake_filesystem fs{"/music/a.wav", "/music/b.wav"};
    job_catalog builder({}, [&fs](const std::filesystem::path& path) { return fs.exists(path); });

    ASSERT_TRUE(builder.add_file("/music/a.wav", {}, "/output/a").has_value());
    ASSERT_TRUE(builder.add_file("/music/b.wav", {}, "/output/b").has_value());

    ASSERT_EQ(builder.size(), 2U);
    const auto& jobs = builder.jobs();
    EXPECT_EQ(jobs[0].input_path, std::filesystem::path{"/music/a.wav"});
    EXPECT_EQ(jobs[1].input_path, std::filesystem::path{"/music/b.wav"});
    EXPECT_EQ(jobs[0].config.profile, model_profile_id::balanced_six_stem);
    EXPECT_TRUE(jobs[0].config.stems_filter.empty());
    EXPECT_EQ(jobs[0].output_dir, std::filesystem::path{"/output/a"});
}

TEST(job_catalog_test, reject_duplicate_files)
{
    const fake_filesystem fs{"/music/a.wav"};
    job_catalog builder({}, [&fs](const std::filesystem::path& path) { return fs.exists(path); });

    ASSERT_TRUE(builder.add_file("/music/a.wav", {}, "/output/a").has_value());
    const auto dup = builder.add_file("/music/a.wav", {}, "/output/a");
    ASSERT_FALSE(dup.has_value());
    EXPECT_NE(dup.error().find("already enqueued"), std::string::npos);
}

TEST(job_catalog_test, applies_overrides)
{
    const fake_filesystem fs{"/music/a.wav"};
    job_catalog builder({}, [&fs](const std::filesystem::path& path) { return fs.exists(path); });

    job_overrides overrides;
    overrides.profile = model_profile_id::balanced_four_stem;
    overrides.stems_filter = std::vector<std::string>{"vocals", "drums"};

    ASSERT_TRUE(builder.add_file("/music/a.wav", overrides, "/output/a").has_value());
    const auto& [input_path, config, output_dir] = builder.jobs().front();
    EXPECT_EQ(config.profile, model_profile_id::balanced_four_stem);
    EXPECT_EQ(config.stems_filter, overrides.stems_filter);
    EXPECT_EQ(output_dir, std::filesystem::path{"/output/a"});
}

TEST(job_catalog_test, reject_unsupported_stem_in_overrides)
{
    const fake_filesystem fs{"/music/a.wav"};
    job_catalog builder({}, [&fs](const std::filesystem::path& path) { return fs.exists(path); });

    job_overrides overrides;
    overrides.stems_filter = std::vector<std::string>{"vocals", "synths"}; // synths not in preset

    const auto result = builder.add_file("/music/a.wav", overrides, "/output/a");
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("Unsupported stem"), std::string::npos);
}
} // namespace stemsmith
