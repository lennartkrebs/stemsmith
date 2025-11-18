#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "stemsmith/job_catalog.h"

namespace stemsmith
{

enum class job_status
{
    queued,
    running,
    completed,
    failed,
    cancelled
};

struct job_event
{
    std::size_t id{};
    job_status status{job_status::queued};
    float progress{-1.0f};
    std::string message{};
    std::optional<std::string> error{};
};

/**
 * @brief A pool of worker threads to process jobs concurrently.
 */
class worker_pool
{
public:
    using job_processor = std::function<void(const job_descriptor&, const std::atomic_bool& stop_flag)>;
    using job_callback = std::function<void(const job_event&)>;

    worker_pool(std::size_t thread_count, job_processor processor, job_callback callback = {});
    ~worker_pool();

    // non-copyable, non-movable
    worker_pool(const worker_pool&) = delete;
    worker_pool& operator=(const worker_pool&) = delete;
    worker_pool(worker_pool&&) = delete;
    worker_pool& operator=(worker_pool&&) = delete;

    [[nodiscard]] std::size_t enqueue(job_descriptor job);
    [[nodiscard]] bool cancel(std::size_t job_id, std::string reason = {});
    void shutdown();
    [[nodiscard]] bool is_shutdown() const noexcept;

private:
    struct cancellation_state
    {
        mutable std::mutex reason_mutex;
        std::string reason;
        std::atomic_bool requested{false};
    };

    struct queued_job
    {
        std::size_t id{};
        job_descriptor job;
        std::shared_ptr<cancellation_state> cancellation;
    };

    static bool request_cancel(const std::shared_ptr<cancellation_state>& state, std::string reason);
    static std::string cancellation_reason(const std::shared_ptr<cancellation_state>& state);
    void emit_cancelled(std::size_t id, const std::shared_ptr<cancellation_state>& state) const;

    void worker_loop();
    void emit_event(std::size_t id,
                    job_status status,
                    float progress = -1.0f,
                    std::string message = {},
                    std::optional<std::string> error = {}) const;

    job_processor processor_;
    job_callback callback_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<queued_job> queue_;
    std::unordered_map<std::size_t, std::shared_ptr<cancellation_state>> running_;
    std::vector<std::thread> workers_;
    std::atomic<std::size_t> next_id_{0};
    bool shutting_down_{false};
};
} // namespace stemsmith
