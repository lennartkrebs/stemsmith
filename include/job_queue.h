#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>
#include <semaphore>

namespace stemsmith {

struct job
{
    std::string id;
    std::string input_path;
    std::string output_path;
    std::string model_name;
    std::string mode; // "hq" or "fast"

    std::string status = "queued"; // "queued", "processing", "completed", "failed"
    std::atomic<float> progress{0.0f}; // 0.0 to 1.0
    std::string error_message;
    std::vector<std::string> stems;
};

class job_queue
{
public:
    using progress_callback_t = std::function<void(const job&)>;

    explicit job_queue(size_t max_jobs = std::thread::hardware_concurrency());
    ~job_queue();

    // Disable copy and move semantics
    job_queue(const job_queue&) = delete;
    job_queue& operator=(const job_queue&) = delete;
    job_queue(job_queue&&) = delete;
    job_queue& operator=(job_queue&&) = delete;

    void push(const std::shared_ptr<job>& job);

    progress_callback_t on_progress;
    progress_callback_t on_error;
    progress_callback_t on_complete;

private:
    void worker_thread();
    void run(const std::shared_ptr<job>& job) const;
    void join_all();

    std::mutex queue_mutex_;
    std::vector<std::shared_ptr<job>> job_queue_;
    std::vector<std::thread> workers_;
    std::atomic<bool> stop_{false};
    std::counting_semaphore<std::numeric_limits<int>::max()> semaphore_;
};

} // namespace stemsmith