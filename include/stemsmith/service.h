/**
 * @file service.h
 * @brief High-level submission API for Stemsmith job processing.
 *
 * This header exposes ::stemsmith::service for managing workers,
 * ::stemsmith::job_request/job_handle for per-job control, and callback types.
 */
#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "stemsmith/job_config.h"
#include "stemsmith/job_result.h"
#include "stemsmith/weight_fetcher.h"

namespace stemsmith
{

class model_cache;
class job_runner;

struct job_event
{
    std::size_t id{};
    job_status status{job_status::queued};
    float progress{-1.0f};
    std::string message{};
    std::optional<std::string> error{};
};

struct job_descriptor
{
    std::filesystem::path input_path;
    job_template config;
    std::filesystem::path output_dir;
};

struct job_request
{
    std::filesystem::path input_path;
    std::optional<model_profile_id> profile{};
    std::optional<std::vector<std::string>> stems{};
    std::optional<std::filesystem::path> output_subdir{};
    job_observer observer{};
};

struct model_handle
{
    model_profile_id profile;
    std::filesystem::path weights_path;
    std::string sha256;
    std::uint64_t size_bytes{};
    bool was_cached{false};
};

using weight_progress_callback =
    std::function<void(model_profile_id profile, std::size_t bytes_downloaded, std::size_t total_bytes)>;

struct runtime_config
{
    struct cache_config
    {
        std::filesystem::path root;
        std::shared_ptr<weight_fetcher> fetcher{};
        weight_progress_callback on_progress{};
    };

    cache_config cache{};
    std::filesystem::path output_root;
    std::size_t worker_count{std::thread::hardware_concurrency()};
    std::function<void(const job_descriptor&, const job_event&)> on_job_event{};
};

/**
 * @brief High-level service for submitting and managing separation jobs.
 *
 * The ::stemsmith::service class provides a convenient interface for
 * submitting audio separation jobs, managing model weights, and receiving
 * job events via callbacks.
 */
class service
{
public:
    using event_callback = std::function<void(const job_descriptor&, const job_event&)>;

    static std::expected<std::unique_ptr<service>, std::string> create(runtime_config runtime,
                                                                       job_template defaults = {});

    [[nodiscard]] std::expected<job_handle, std::string> submit(job_request request) const;
    [[nodiscard]] std::expected<model_handle, std::string> ensure_model_ready(model_profile_id profile) const;
    [[nodiscard]] std::expected<void, std::string> purge_models(
        std::optional<model_profile_id> profile = std::nullopt) const;

    service(const service&) = delete;
    service& operator=(const service&) = delete;
    service(service&&) = delete;
    service& operator=(service&&) = delete;
    ~service();

private:
    service(std::shared_ptr<model_cache> cache, std::unique_ptr<job_runner> runner);

    std::shared_ptr<model_cache> cache_;
    std::unique_ptr<job_runner> runner_;
};

} // namespace stemsmith
