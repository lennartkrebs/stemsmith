#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <atomic>

namespace stemsmith
{

struct job_descriptor;

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

    [[nodiscard]] std::size_t enqueue(job_descriptor job) const;
    void shutdown() const;
    [[nodiscard]] bool is_shutdown() const noexcept;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};
} // namespace stemsmith
