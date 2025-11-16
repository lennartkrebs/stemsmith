#include <filesystem>
#include <gtest/gtest.h>
#include <string>

#include "stemsmith/model_session_pool.h"
#include "support/fake_session.h"

namespace
{
using stemsmith::audio_buffer;
using stemsmith::model_profile;
using stemsmith::model_profile_id;
using stemsmith::model_session;
using stemsmith::model_session_pool;

} // namespace

namespace stemsmith
{
TEST(model_session_pool_test, creates_sessions_through_factory)
{
    int factory_calls = 0;
    model_session_pool pool(
        [&](model_profile_id id)
        {
            ++factory_calls;
            return test::make_stub_session(id);
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
    model_session_pool pool(
        [&](model_profile_id id)
        {
            ++factory_calls;
            return test::make_stub_session(id);
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
    model_session_pool pool([](model_profile_id) -> std::expected<std::unique_ptr<model_session>, std::string>
                            { return std::unexpected("boom"); });

    const auto handle = pool.acquire(model_profile_id::balanced_four_stem);
    ASSERT_FALSE(handle.has_value());
    EXPECT_NE(handle.error().find("boom"), std::string::npos);
}
} // namespace stemsmith
