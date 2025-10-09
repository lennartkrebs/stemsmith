#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>
#include <semaphore>

namespace stemsmith {

struct job;

class job_queue
{
public:
    using progress_callback_t = std::function<void(const job&)>;

    explicit job_queue(size_t max_jobs = std::thread::hardware_concurrency());
    ~job_queue();

    job_queue(const job_queue&) = delete;
    job_queue& operator=(const job_queue&) = delete;
    job_queue(job_queue&&) = delete;
    job_queue& operator=(job_queue&&) = delete;

    void push(const std::shared_ptr<job>& job);

    progress_callback_t progress_callback;
private:
    void notify(const job& job) const;
    void worker_thread();
    void run(const std::shared_ptr<job>& job) const;
    void join_all();

    std::vector<std::thread> workers_;
    std::vector<std::shared_ptr<job>> job_queue_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_{false};
};

} // namespace stemsmith