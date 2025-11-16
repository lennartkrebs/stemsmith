#pragma once

#include "stemsmith/job_catalog.h"
#include "stemsmith/separation_engine.h"
#include "stemsmith/worker_pool.h"

#include <expected>
#include <future>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>
#include <unordered_map>

namespace stemsmith
{

struct job_result
{
    std::filesystem::path input_path;
    std::filesystem::path output_dir;
    job_status status{job_status::queued};
    std::optional<std::string> error{};
};

class job_runner
{
public:
    job_runner(job_config base_config,
               model_cache& cache,
               std::filesystem::path output_root,
               std::size_t worker_count = std::thread::hardware_concurrency(),
               std::function<void(const job_descriptor&, const job_event&)> event_callback = {});

    job_runner(job_config base_config,
               separation_engine engine,
               std::size_t worker_count = std::thread::hardware_concurrency(),
               std::function<void(const job_descriptor&, const job_event&)> event_callback = {});

    std::expected<std::future<job_result>, std::string> submit(const std::filesystem::path& path, const job_overrides& overrides = {});

private:
    struct job_context
    {
        std::promise<job_result> promise;
        std::optional<std::filesystem::path> output_dir;
        std::optional<std::string> error;
        job_descriptor job;
        std::size_t job_id{static_cast<std::size_t>(-1)};
    };

    void process_job(const job_descriptor& job, const std::atomic_bool& stop_flag);
    void handle_event(const job_event& event);
    std::shared_ptr<job_context> context_for(const std::filesystem::path& path) const;

    job_catalog catalog_;
    separation_engine engine_;
    worker_pool pool_;
    std::function<void(const job_descriptor&, const job_event&)> event_callback_;

    mutable std::mutex mutex_;
    std::unordered_map<std::filesystem::path, std::shared_ptr<job_context>> contexts_;
    std::unordered_map<std::size_t, std::filesystem::path> paths_by_id_;
    std::unordered_map<std::size_t, std::vector<job_event>> pending_events_;
};

} // namespace stemsmith
