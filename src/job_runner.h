#pragma once

#include <atomic>
#include <expected>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "job_catalog.h"
#include "separation_engine.h"
#include "stemsmith/job_result.h"
#include "stemsmith/service.h"
#include "worker_pool.h"

namespace stemsmith
{

struct job_handle_state
{
    job_descriptor job;
    std::size_t job_id{static_cast<std::size_t>(-1)};
    std::shared_future<job_result> future;
    worker_pool* pool{nullptr};
    std::atomic_bool cancel_requested{false};
    mutable std::mutex observer_mutex;
    job_observer observer;

    void notify(const job_descriptor& descriptor, const job_event& event) const
    {
        std::lock_guard lock(observer_mutex);
        if (observer.callback)
        {
            observer.callback(descriptor, event);
        }
    }
};

class job_runner
{
public:
    job_runner(model_cache& cache,
               std::filesystem::path output_root,
               job_template defaults = {},
               std::size_t worker_count = std::thread::hardware_concurrency(),
               std::function<void(const job_descriptor&, const job_event&)> event_callback = {});

    job_runner(separation_engine engine,
               job_template defaults = {},
               std::size_t worker_count = std::thread::hardware_concurrency(),
               std::function<void(const job_descriptor&, const job_event&)> event_callback = {});

    std::expected<job_handle, std::string> submit(job_request request);

private:
    struct job_context
    {
        std::promise<job_result> promise;
        std::optional<std::filesystem::path> output_dir;
        std::optional<std::string> error;
        job_descriptor job;
        std::size_t job_id{static_cast<std::size_t>(-1)};
        job_observer observer;
        std::weak_ptr<job_handle_state> handle_state;
    };

    void process_job(const job_descriptor& job, const std::atomic_bool& stop_flag);
    void handle_event(const job_event& event);
    std::shared_ptr<job_context> context_for(const std::filesystem::path& path) const;
    void notify_observers(const std::shared_ptr<job_context>& context, const job_event& event) const;

    job_catalog catalog_;
    separation_engine engine_;
    std::function<void(const job_descriptor&, const job_event&)> event_callback_;

    mutable std::mutex mutex_;
    std::unordered_map<std::filesystem::path, std::shared_ptr<job_context>> contexts_;
    std::unordered_map<std::size_t, std::filesystem::path> paths_by_id_;
    std::unordered_map<std::size_t, std::vector<job_event>> pending_events_;
    worker_pool pool_;
};

} // namespace stemsmith
