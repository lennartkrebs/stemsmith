#pragma once

#include "stemsmith/job_config.h"
#include "stemsmith/job_runner.h"
#include "stemsmith/model_cache.h"
#include "stemsmith/weight_fetcher.h"

#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <thread>

namespace stemsmith
{

class service
{
public:
    using event_callback = std::function<void(const job_descriptor&, const job_event&)>;

    static std::expected<std::unique_ptr<service>, std::string>
    create(job_config config,
           std::filesystem::path cache_root,
           std::filesystem::path output_root,
           std::shared_ptr<weight_fetcher> fetcher,
           std::size_t worker_count = std::thread::hardware_concurrency(),
           service::event_callback callback = {});

    service(const service&) = delete;
    service& operator=(const service&) = delete;
    service(service&&) = delete;
    service& operator=(service&&) = delete;

    job_runner& runner() noexcept { return *runner_; }
    model_cache& cache() noexcept { return *cache_; }

private:
    service(std::shared_ptr<model_cache> cache, std::unique_ptr<job_runner> runner);

    std::shared_ptr<model_cache> cache_;
    std::unique_ptr<job_runner> runner_;
};

} // namespace stemsmith
