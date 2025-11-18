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

#include "stemsmith/job_catalog.h"
#include "stemsmith/separation_engine.h"
#include "stemsmith/worker_pool.h"

namespace stemsmith
{

struct job_result
{
    std::filesystem::path input_path;
    std::filesystem::path output_dir;
    job_status status{job_status::queued};
    std::optional<std::string> error{};
};

struct job_observer
{
    std::function<void(const job_descriptor&, const job_event&)> callback{};
};

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

class job_handle
{
public:
    job_handle() = default;

    [[nodiscard]] std::size_t id() const noexcept;
    [[nodiscard]] const job_descriptor& descriptor() const;
    [[nodiscard]] std::shared_future<job_result> result() const;
    [[nodiscard]] std::expected<void, std::string> cancel(std::string reason = {}) const;
    void set_observer(job_observer observer) const;
    explicit operator bool() const noexcept
    {
        return static_cast<bool>(state_);
    }

private:
    explicit job_handle(std::shared_ptr<job_handle_state> state);
    std::shared_ptr<job_handle_state> state_;

    friend class job_runner;
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

    std::expected<job_handle, std::string> submit(const std::filesystem::path& path,
                                                  const job_overrides& overrides = {},
                                                  job_observer observer = {});

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

inline job_handle::job_handle(std::shared_ptr<job_handle_state> state) : state_(std::move(state)) {}

inline std::size_t job_handle::id() const noexcept
{
    if (!state_)
    {
        return static_cast<std::size_t>(-1);
    }
    return state_->job_id;
}

inline const job_descriptor& job_handle::descriptor() const
{
    if (!state_)
    {
        throw std::runtime_error("Job handle is empty");
    }
    return state_->job;
}

inline std::shared_future<job_result> job_handle::result() const
{
    if (!state_)
    {
        return {};
    }
    return state_->future;
}

inline std::expected<void, std::string> job_handle::cancel(std::string reason) const
{
    if (!state_)
    {
        return std::unexpected("Job handle is empty");
    }

    if (!state_->pool)
    {
        return std::unexpected("Worker pool unavailable");
    }

    if (bool expected = false; !state_->cancel_requested.compare_exchange_strong(expected, true))
    {
        return std::unexpected("Cancellation already requested");
    }

    if (!state_->pool->cancel(state_->job_id, std::move(reason)))
    {
        return std::unexpected("Job is no longer cancellable");
    }

    return {};
}

inline void job_handle::set_observer(job_observer observer) const
{
    if (!state_)
    {
        return;
    }
    std::lock_guard lock(state_->observer_mutex);
    state_->observer = std::move(observer);
}

} // namespace stemsmith
