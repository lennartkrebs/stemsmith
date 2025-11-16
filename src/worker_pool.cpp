#include "stemsmith/worker_pool.h"

#include "stemsmith/job_catalog.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace stemsmith
{
struct worker_pool::impl
{
    struct queued_job
    {
        std::size_t id{};
        job_descriptor job;
    };

    job_processor processor;
    job_callback callback;

    std::mutex mutex;
    std::condition_variable cv;
    std::deque<queued_job> queue;
    std::vector<std::thread> workers;
    std::atomic<std::size_t> next_id{0};
    bool shutting_down{false};
    std::atomic_bool stop_requested{false};

    explicit impl(std::size_t thread_count, job_processor processor_fn, job_callback callback_fn)
        : processor(std::move(processor_fn)), callback(std::move(callback_fn))
    {
        if (!processor)
        {
            throw std::invalid_argument("job_processor must not be empty");
        }

        if (thread_count == 0)
        {
            throw std::invalid_argument("thread_count must be at least 1");
        }

        workers.reserve(thread_count);
        for (std::size_t i = 0; i < thread_count; ++i)
        {
            workers.emplace_back([this]() { worker_loop(); });
        }
    }

    ~impl() { shutdown(); }

    [[nodiscard]] bool is_shutdown() const noexcept { return shutting_down; }

    std::size_t enqueue(job_descriptor job)
    {
        std::size_t id;
        {
            std::lock_guard lock(mutex);
            if (shutting_down)
            {
                return -1;
            }
            id = next_id++;
            queue.push_back(queued_job{id, std::move(job)});
        }
        emit_event(id, job_status::queued);
        cv.notify_one();
        return id;
    }

    void shutdown()
    {
        // Cancel running jobs
        if (stop_requested.exchange(true))
            return;

        std::vector<queued_job> cancelled;
        {
            std::lock_guard lock(mutex);
            if (shutting_down)
            {
                return;
            }
            shutting_down = true;
            cancelled.reserve(queue.size());
            while (!queue.empty())
            {
                cancelled.push_back(std::move(queue.front()));
                queue.pop_front();
            }
        }

        cv.notify_all();

        for (auto& worker : workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }

        workers.clear();

        for (auto& [id, job] : cancelled)
        {
            emit_event(id, job_status::cancelled, -1.0f, "Worker pool shutting down");
        }
    }

    void worker_loop()
    {
        while (true)
        {
            queued_job next;
            {
                std::unique_lock lock(mutex);
                cv.wait(lock, [&] { return shutting_down || !queue.empty(); });

                if (queue.empty())
                {
                    if (shutting_down)
                    {
                        break;
                    }
                    continue;
                }

                next = std::move(queue.front());
                queue.pop_front();
            }

            emit_event(next.id, job_status::running);

            std::optional<std::string> error;
            try
            {
                processor(next.job, stop_requested);
            }
            catch (const std::exception& ex)
            {
                error = ex.what();
            }
            catch (...)
            {
                error = "Unknown job failure";
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

    void emit_event(std::size_t id,
                    job_status status,
                    float progress = -1.0f,
                    std::string message = {},
                    std::optional<std::string> error = {}) const
    {
        if (callback)
        {
            job_event event;
            event.id = id;
            event.status = status;
            event.progress = progress;
            event.message = std::move(message);
            event.error = std::move(error);
            callback(event);
        }
    }
};

worker_pool::worker_pool(std::size_t thread_count, job_processor processor, job_callback callback)
    : impl_(std::make_unique<impl>(thread_count, std::move(processor), std::move(callback)))
{
}

worker_pool::~worker_pool() = default;

std::size_t worker_pool::enqueue(job_descriptor job) const
{
    return impl_->enqueue(std::move(job));
}

void worker_pool::shutdown() const { impl_->shutdown(); }

bool worker_pool::is_shutdown() const noexcept { return impl_->is_shutdown(); }
} // namespace stemsmith
