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
    job_config config;
    const auto cache_root = std::filesystem::temp_directory_path() / "stemsmith-service-cache";
    const auto output_root = std::filesystem::temp_directory_path() / "stemsmith-service-output";
    std::filesystem::remove_all(cache_root);
    std::filesystem::remove_all(output_root);

    auto fetcher = std::make_shared<test::fake_fetcher>("payload");
    auto service_result = service::create(config, cache_root, output_root, fetcher, 1);
    ASSERT_TRUE(service_result.has_value());
    auto svc = std::move(service_result.value());
    ASSERT_NE(svc, nullptr);

    EXPECT_TRUE(std::filesystem::exists(cache_root));
    EXPECT_TRUE(std::filesystem::exists(output_root) || std::filesystem::create_directories(output_root));

    const auto submit = svc->submit({});
    EXPECT_FALSE(submit.has_value());

    service::job_request request;
    request.input_path = output_root / "missing.wav";
    const auto service_submit = svc->submit(std::move(request));
    EXPECT_FALSE(service_submit.has_value());
}
} // namespace stemsmith
