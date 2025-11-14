#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include <Eigen/Dense>

#include "stemsmith/model_session_pool.h"

namespace
{
using stemsmith::audio_buffer;
using stemsmith::model_profile;
using stemsmith::model_profile_id;
using stemsmith::model_session;
using stemsmith::model_session_pool;

std::expected<std::unique_ptr<model_session>, std::string> make_stub_session(model_profile_id profile_id)
{
    const auto profile_opt = stemsmith::lookup_profile(profile_id);
    if (!profile_opt)
    {
        return std::unexpected("Unknown profile id");
    }

    auto resolver = []() -> std::expected<std::filesystem::path, std::string>
    {
        return std::filesystem::path{"weights.stub"};
    };

    auto loader = [](demucscpp::demucs_model&, const std::filesystem::path&)
    {
        return std::expected<void, std::string>{};
    };

    auto inference = [](const demucscpp::demucs_model&, const Eigen::MatrixXf&, demucscpp::ProgressCallback)
    {
        return Eigen::Tensor3dXf(4, 2, 1);
    };

    return std::make_unique<model_session>(profile_opt.value(), std::move(resolver), std::move(loader), std::move(inference));
}
} // namespace

namespace stemsmith
{
TEST(model_session_pool_test, creates_sessions_through_factory)
{
    int factory_calls = 0;
    model_session_pool pool([&](model_profile_id id) {
        ++factory_calls;
        return make_stub_session(id);
    });

    const auto first = pool.acquire(model_profile_id::balanced_four_stem);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(factory_calls, 1);

    const auto second = pool.acquire(model_profile_id::balanced_four_stem);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(factory_calls, 2);
}

TEST(model_session_pool_test, recycles_sessions_when_handles_destroyed)
{
    int factory_calls = 0;
    model_session_pool pool([&](model_profile_id id) {
        ++factory_calls;
        return make_stub_session(id);
    });

    const auto first = pool.acquire(model_profile_id::balanced_four_stem);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(factory_calls, 1);

    {
        const auto second = pool.acquire(model_profile_id::balanced_four_stem);
        ASSERT_TRUE(second.has_value());
        EXPECT_EQ(factory_calls, 2);
    }

    const auto third = pool.acquire(model_profile_id::balanced_four_stem);
    ASSERT_TRUE(third.has_value());
    EXPECT_EQ(factory_calls, 2);
}

TEST(model_session_pool_test, propagates_factory_errors)
{
    model_session_pool pool(
        [](model_profile_id) -> std::expected<std::unique_ptr<model_session>, std::string> {
            return std::unexpected("boom");
        });

    const auto handle = pool.acquire(model_profile_id::balanced_four_stem);
    ASSERT_FALSE(handle.has_value());
    EXPECT_NE(handle.error().find("boom"), std::string::npos);
}
} // namespace stemsmith
