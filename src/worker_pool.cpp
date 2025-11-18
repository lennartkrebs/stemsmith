#include "stemsmith/worker_pool.h"

#include <algorithm>
#include <exception>
#include <ranges>
#include <stdexcept>
#include <utility>

#include "stemsmith/job_catalog.h"

namespace stemsmith
{
namespace
{
constexpr auto kDefaultCancellationReason = "Job cancelled";
constexpr auto kShutdownCancellationReason = "Worker pool shutting down";
} // namespace

worker_pool::worker_pool(std::size_t thread_count, job_processor processor, job_callback callback)
    : processor_(std::move(processor))
    , callback_(std::move(callback))
{
    if (!processor_)
    {
        throw std::invalid_argument("job_processor must not be empty");
    }

    if (thread_count == 0)
    {
        throw std::invalid_argument("thread_count must be at least 1");
    }

    workers_.reserve(thread_count);
    for (std::size_t i = 0; i < thread_count; ++i)
    {
        workers_.emplace_back([this]() { worker_loop(); });
    }
}

worker_pool::~worker_pool()
{
    shutdown();
}

std::size_t worker_pool::enqueue(job_descriptor job)
{
    std::size_t id;
    {
        std::lock_guard lock(mutex_);
        if (shutting_down_)
        {
            return -1;
        }

        id = next_id_++;
        queue_.push_back(queued_job{id, std::move(job), std::make_shared<cancellation_state>()});
    }

    emit_event(id, job_status::queued);
    cv_.notify_one();
    return id;
}

bool worker_pool::cancel(std::size_t job_id, std::string reason)
{
    std::optional<queued_job> queued;
    std::shared_ptr<cancellation_state> running;

    {
        std::lock_guard lock(mutex_);
        const auto queued_it =
            std::ranges::find_if(queue_, [job_id](const queued_job& item) { return item.id == job_id; });
        if (queued_it != queue_.end())
        {
            queued = std::move(*queued_it);
            queue_.erase(queued_it);
        }
        else
        {
            const auto running_it = running_.find(job_id);
            if (running_it == running_.end())
            {
                return false;
            }
            running = running_it->second;
        }
    }

    if (queued)
    {
        if (!request_cancel(queued->cancellation, std::move(reason)))
        {
            return false;
        }
        emit_cancelled(queued->id, queued->cancellation);
        return true;
    }

    return request_cancel(running, std::move(reason));
}

void worker_pool::shutdown()
{
    std::vector<queued_job> cancelled_jobs;
    {
        std::lock_guard lock(mutex_);
        if (shutting_down_)
        {
            return;
        }

        shutting_down_ = true;
        cancelled_jobs.reserve(queue_.size());
        while (!queue_.empty())
        {
            cancelled_jobs.push_back(std::move(queue_.front()));
            queue_.pop_front();
            request_cancel(cancelled_jobs.back().cancellation, kShutdownCancellationReason);
        }

        for (auto& state : running_ | std::views::values)
        {
            request_cancel(state, kShutdownCancellationReason);
        }
    }

    cv_.notify_all();

    for (auto& worker : workers_)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }

    workers_.clear();

    for (auto& job : cancelled_jobs)
    {
        emit_cancelled(job.id, job.cancellation);
    }
}

bool worker_pool::is_shutdown() const noexcept
{
    std::lock_guard lock(mutex_);
    return shutting_down_;
}

bool worker_pool::request_cancel(const std::shared_ptr<cancellation_state>& state, std::string reason)
{
    if (!state)
    {
        return false;
    }

    if (bool expected = false; !state->requested.compare_exchange_strong(expected, true))
    {
        return false;
    }

    if (reason.empty())
    {
        reason = kDefaultCancellationReason;
    }

    {
        std::lock_guard reason_lock(state->reason_mutex);
        state->reason = std::move(reason);
    }

    return true;
}

std::string worker_pool::cancellation_reason(const std::shared_ptr<cancellation_state>& state)
{
    if (!state)
    {
        return kDefaultCancellationReason;
    }

    std::lock_guard reason_lock(state->reason_mutex);
    if (state->reason.empty())
    {
        return kDefaultCancellationReason;
    }
    return state->reason;
}

void worker_pool::emit_cancelled(std::size_t id, const std::shared_ptr<cancellation_state>& state) const
{
    auto reason = cancellation_reason(state);
    emit_event(id, job_status::cancelled, -1.0f, {}, std::move(reason));
}

void worker_pool::worker_loop()
{
    while (true)
    {
        queued_job next;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [&] { return shutting_down_ || !queue_.empty(); });

            if (queue_.empty())
            {
                if (shutting_down_)
                {
                    break;
                }
                continue;
            }

            next = std::move(queue_.front());
            queue_.pop_front();
            running_.emplace(next.id, next.cancellation);
        }

        emit_event(next.id, job_status::running);

        std::optional<std::string> error;
        try
        {
            processor_(next.job, next.cancellation->requested);
        }
        catch (const std::exception& ex)
        {
            error = ex.what();
        }
        catch (...)
        {
            error = "Unknown job failure";
        }

        {
            std::lock_guard lock(mutex_);
            running_.erase(next.id);
        }

        if (next.cancellation->requested.load())
        {
            emit_cancelled(next.id, next.cancellation);
            continue;
        }

        if (error.has_value())
        {
            emit_event(next.id, job_status::failed, -1.0f, {}, std::move(error));
        }
        else
        {
            emit_event(next.id, job_status::completed);
        }
    }
}

void worker_pool::emit_event(std::size_t id,
                             job_status status,
                             float progress,
                             std::string message,
                             std::optional<std::string> error) const
{
    if (!callback_)
    {
        return;
    }

    job_event event;
    event.id = id;
    event.status = status;
    event.progress = progress;
    event.message = std::move(message);
    event.error = std::move(error);
    callback_(event);
}
} // namespace stemsmith
