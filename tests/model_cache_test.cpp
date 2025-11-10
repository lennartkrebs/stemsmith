#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>

#include "stemsmith/model_cache.h"
#include "stemsmith/model_manifest.h"
#include "stemsmith/weight_fetcher.h"

#include "fake_fetcher.h"

namespace
{
struct temp_dir
{
    temp_dir()
    {
        const auto base = std::filesystem::temp_directory_path();
        path = base / std::filesystem::path("stemsmith-cache-test-" + std::to_string(std::rand()));
        std::filesystem::create_directories(path);
    }

    ~temp_dir()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    std::filesystem::path path;
};
} // namespace

namespace stemsmith
{
TEST(ModelManifestTest, LoadsDefaultManifest)
{
    const auto manifest = model_manifest::load_default();
    ASSERT_TRUE(manifest.has_value());
    EXPECT_NE(nullptr, manifest->find(model_profile_id::balanced_four_stem));
    EXPECT_NE(nullptr, manifest->find(model_profile_id::balanced_six_stem));
}

TEST(ModelCacheTest, DownloadsAndCachesWeights)
{
    const auto& profile = lookup_profile(model_profile_id::balanced_four_stem);
    const std::string payload = "fake-weights";
    const std::string expected_sha = "bf6875a563be64dafa0c8e16f4b6093f55e15ba38f5c7a8844eaa61141dc805e";
    model_manifest manifest({model_manifest_entry{
        model_profile_id::balanced_four_stem,
        std::string{profile.key},
        "ggml-model-test.bin",
        "http://example.invalid/ggml-model-test.bin",
        payload.size(),
        expected_sha}});

    auto fetcher = std::make_shared<test::fake_fetcher>(payload);
    temp_dir dir;
    model_cache cache(dir.path, fetcher, std::move(manifest));

    const auto first = cache.ensure_ready(model_profile_id::balanced_four_stem);
    ASSERT_TRUE(first.has_value());
    EXPECT_FALSE(first->was_cached);
    EXPECT_EQ(fetcher->call_count, 1U);
    EXPECT_TRUE(std::filesystem::exists(first->weights_path));

    const auto second = cache.ensure_ready(model_profile_id::balanced_four_stem);
    ASSERT_TRUE(second.has_value());
    EXPECT_TRUE(second->was_cached);
    EXPECT_EQ(fetcher->call_count, 1U);
    EXPECT_EQ(second->weights_path, first->weights_path);
}
} // namespace stemsmith
