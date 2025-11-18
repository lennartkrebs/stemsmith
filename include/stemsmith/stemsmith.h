#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <thread>

#include "stemsmith/job_catalog.h"
#include "stemsmith/job_config.h"
#include "stemsmith/job_runner.h"
#include "stemsmith/model_cache.h"
#include "stemsmith/weight_fetcher.h"

namespace stemsmith
{

class service
{
public:
    using event_callback = std::function<void(const job_descriptor&, const job_event&)>;
    using weight_progress_callback = model_cache::weight_progress_callback;

    struct job_request
    {
        std::filesystem::path input_path;
        job_overrides overrides{};
        job_observer observer{};
    };

    static std::expected<std::unique_ptr<service>, std::string> create(
        job_config config,
        std::filesystem::path cache_root,
        std::filesystem::path output_root,
        std::shared_ptr<weight_fetcher> fetcher = {},
        std::size_t worker_count = std::thread::hardware_concurrency(),
        event_callback callback = {},
        weight_progress_callback weight_callback = {});

    [[nodiscard]] std::expected<job_handle, std::string> submit(job_request request) const;
    [[nodiscard]] std::expected<model_handle, std::string> ensure_model_ready(model_profile_id profile) const;
    [[nodiscard]] std::expected<void, std::string> purge_models(std::optional<model_profile_id> profile = std::nullopt) const;

    service(const service&) = delete;
    service& operator=(const service&) = delete;
    service(service&&) = delete;
    service& operator=(service&&) = delete;

private:
    service(std::shared_ptr<model_cache> cache, std::unique_ptr<job_runner> runner);
    std::shared_ptr<model_cache> cache_;
    std::unique_ptr<job_runner> runner_;
};

} // namespace stemsmith
