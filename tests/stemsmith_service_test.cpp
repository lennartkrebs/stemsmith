#include <filesystem>
#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "stemsmith/stemsmith.h"
#include "support/fake_fetcher.h"

namespace stemsmith
{
TEST(stemsmith_service_test, creates_runner_with_cache)
{
    const auto cache_root = std::filesystem::temp_directory_path() / "stemsmith-service-cache";
    const auto output_root = std::filesystem::temp_directory_path() / "stemsmith-service-output";
    std::filesystem::remove_all(cache_root);
    std::filesystem::remove_all(output_root);

    auto fetcher = std::make_shared<test::fake_fetcher>("payload");
    bool weight_progress_called = false;
    weight_progress_callback weight_callback = [&](model_profile_id, std::size_t downloaded, std::size_t total)
    {
        weight_progress_called = true;
        EXPECT_GE(total, downloaded);
    };

    runtime_config runtime;
    runtime.cache.root = cache_root;
    runtime.cache.fetcher = fetcher;
    runtime.cache.on_progress = weight_callback;
    runtime.output_root = output_root;
    runtime.worker_count = 1;

    auto service_result = service::create(std::move(runtime));
    ASSERT_TRUE(service_result.has_value());
    auto svc = std::move(service_result.value());
    ASSERT_NE(svc, nullptr);

    EXPECT_TRUE(std::filesystem::exists(cache_root));
    EXPECT_TRUE(std::filesystem::exists(output_root) || std::filesystem::create_directories(output_root));

    const auto submit = svc->submit({});
    EXPECT_FALSE(submit.has_value());

    job_request request;
    request.input_path = output_root / "missing.wav";
    const auto service_submit = svc->submit(request);
    EXPECT_FALSE(service_submit.has_value());

    const auto ensure = svc->ensure_model_ready(model_profile_id::balanced_four_stem);
    EXPECT_FALSE(ensure.has_value());
    EXPECT_TRUE(weight_progress_called);
    EXPECT_TRUE(svc->purge_models().has_value());
}
} // namespace stemsmith
