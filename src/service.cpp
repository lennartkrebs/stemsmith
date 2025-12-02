#include <memory>
#include <system_error>

#include "http_weight_fetcher.h"
#include "job_runner.h"
#include "model_cache.h"
#include "stemsmith/stemsmith.h"

namespace stemsmith
{

service::service(std::shared_ptr<model_cache> cache, std::unique_ptr<job_runner> runner)
    : cache_(std::move(cache))
    , runner_(std::move(runner))
{
}

service::~service() = default;

std::expected<job_handle, std::string> service::submit(job_request request) const
{
    if (!runner_)
    {
        return std::unexpected("Service is not initialized");
    }

    return runner_->submit(std::move(request));
}

std::expected<void, std::string> service::purge_models(std::optional<model_profile_id> profile) const
{
    if (!cache_)
    {
        return std::unexpected("Model cache is not available");
    }

    if (profile)
    {
        return cache_->purge(*profile);
    }

    return cache_->purge_all();
}

std::expected<model_handle, std::string> service::ensure_model_ready(model_profile_id profile) const
{
    if (!cache_)
    {
        return std::unexpected("Model cache is not available");
    }
    return cache_->ensure_ready(profile);
}

std::expected<std::unique_ptr<service>, std::string> service::create(runtime_config runtime, job_template defaults)
{
    if (runtime.cache.root.empty())
    {
        return std::unexpected("cache_root is required");
    }
    if (runtime.output_root.empty())
    {
        return std::unexpected("output_root is required");
    }

    if (!runtime.cache.fetcher)
    {
        runtime.cache.fetcher = std::make_shared<http_weight_fetcher>();
    }

    std::error_code ec;
    std::filesystem::create_directories(runtime.cache.root, ec);
    if (ec)
    {
        return std::unexpected("Failed to create cache root: " + ec.message());
    }

    std::filesystem::create_directories(runtime.output_root, ec);
    if (ec)
    {
        return std::unexpected("Failed to create output root: " + ec.message());
    }

    auto cache_result =
        model_cache::create(runtime.cache.root, runtime.cache.fetcher, std::move(runtime.cache.on_progress));
    if (!cache_result)
    {
        return std::unexpected(cache_result.error());
    }
    auto cache_ptr = std::make_shared<model_cache>(std::move(cache_result.value()));

    auto runner = std::make_unique<job_runner>(*cache_ptr,
                                               runtime.output_root,
                                               defaults,
                                               runtime.worker_count,
                                               std::move(runtime.on_job_event));

    return std::unique_ptr<service>(new service(std::move(cache_ptr), std::move(runner)));
}

} // namespace stemsmith
