#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>

#include "fake_fetcher.h"
#include "stemsmith/model_cache.h"
#include "stemsmith/model_manifest.h"
#include "stemsmith/weight_fetcher.h"

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
TEST(model_cache_test, load_default_manifest)
{
    const auto manifest = model_manifest::load_default();
    ASSERT_TRUE(manifest.has_value());
    EXPECT_NE(nullptr, manifest->find(model_profile_id::balanced_four_stem));
    EXPECT_NE(nullptr, manifest->find(model_profile_id::balanced_six_stem));
}

TEST(model_cache_test, download_and_cache_weights)
{
    const auto profile = lookup_profile(model_profile_id::balanced_four_stem);
    ASSERT_TRUE(profile.has_value());
    const std::string payload = "fake-weights";
    const std::string expected_sha = "bf6875a563be64dafa0c8e16f4b6093f55e15ba38f5c7a8844eaa61141dc805e";
    model_manifest manifest({model_manifest_entry{model_profile_id::balanced_four_stem,
                                                  std::string{profile->key},
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

TEST(model_cache_test, verify_checksum_reports_matches_and_mismatches)
{
    temp_dir dir;
    const auto file = dir.path / "weights.bin";
    const std::string payload = "checksum-data";
    {
        std::ofstream out(file, std::ios::binary);
        out << payload;
    }

    model_manifest_entry entry{model_profile_id::balanced_four_stem,
                               "balanced-four-stem",
                               "weights.bin",
                               "unused",
                               payload.size(),
                               "40514c38a5c61b38be42cb94586683adef6de01e3c1dcfe11d317583affb8d87"};

    const auto ok = model_cache::verify_checksum(file, entry);
    ASSERT_TRUE(ok.has_value());
    EXPECT_TRUE(ok.value());

    auto mismatched = entry;
    mismatched.sha256 = "84b597a6069a65b44616fb6b335a17088a647fdfb5ff3c662838e6c80c88ab0d";
    const auto mismatch = model_cache::verify_checksum(file, mismatched);
    ASSERT_TRUE(mismatch.has_value());
    EXPECT_FALSE(mismatch.value());
}

TEST(model_cache_test, ensure_ready_replaces_corrupted_cache_entries)
{
    const auto profile = lookup_profile(model_profile_id::balanced_four_stem);
    ASSERT_TRUE(profile.has_value());
    const std::string expected_payload = "fresh-weights";
    model_manifest manifest({model_manifest_entry{model_profile_id::balanced_four_stem,
                                                  std::string{profile->key},
                                                  "ggml-model-test.bin",
                                                  "http://example.invalid/ggml-model-test.bin",
                                                  expected_payload.size(),
                                                  "7087b24a19bdc59f848a3c51304d4f52e6c7d53e7ae952a00c9f2486de786176"}});

    auto fetcher = std::make_shared<test::fake_fetcher>(expected_payload);
    temp_dir dir;

    const auto target_dir = dir.path / profile->key;
    std::filesystem::create_directories(target_dir);
    const auto target_path = target_dir / "ggml-model-test.bin";
    {
        std::ofstream out(target_path, std::ios::binary);
        out << "bad-weights";
    }

    model_cache cache(dir.path, fetcher, std::move(manifest));
    const auto result = cache.ensure_ready(model_profile_id::balanced_four_stem);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->was_cached);
    EXPECT_EQ(fetcher->call_count, 1U);
    EXPECT_EQ(result->weights_path, target_path);

    std::ifstream in(result->weights_path, std::ios::binary);
    const std::string stored{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    EXPECT_EQ(stored, expected_payload);
}
} // namespace stemsmith
