#include "stemsmith/stemsmith.h"

#include <memory>
#include <system_error>

#include "http_weight_fetcher.h"
#include "job_runner.h"
#include "model_cache.h"

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

    if (request.input_path.empty())
    {
        return std::unexpected("Input path must not be empty");
    }

    return runner_->submit(request.input_path, request.overrides, std::move(request.observer));
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

std::expected<std::unique_ptr<service>, std::string> service::create(std::filesystem::path cache_root,
                                                                     std::filesystem::path output_root,
                                                                     std::shared_ptr<weight_fetcher> fetcher,
                                                                     std::size_t worker_count,
                                                                     event_callback callback,
                                                                     weight_progress_callback weight_callback)
{
    if (!fetcher)
    {
        // Default to HTTP fetcher
        fetcher = std::make_shared<http_weight_fetcher>();
    }

    std::error_code ec;
    if (!cache_root.empty())
    {
        std::filesystem::create_directories(cache_root, ec);
        if (ec)
        {
            return std::unexpected("Failed to create cache root: " + ec.message());
        }
    }

    if (!output_root.empty())
    {
        std::filesystem::create_directories(output_root, ec);
        if (ec)
        {
            return std::unexpected("Failed to create output root: " + ec.message());
        }
    }

    auto cache_result = model_cache::create(std::move(cache_root), std::move(fetcher), std::move(weight_callback));
    if (!cache_result)
    {
        return std::unexpected(cache_result.error());
    }
    auto cache_ptr = std::make_shared<model_cache>(std::move(cache_result.value()));

    auto runner = std::make_unique<job_runner>(*cache_ptr,
                                               std::move(output_root),
                                               worker_count,
                                               std::move(callback));

    return std::unique_ptr<service>(new service(std::move(cache_ptr), std::move(runner)));
}

} // namespace stemsmith
