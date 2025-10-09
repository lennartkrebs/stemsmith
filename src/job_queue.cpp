#include "job_queue.h"
#include "job_builder.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace stemsmith {

job_queue::job_queue(const size_t max_jobs)
{
    workers_.reserve(max_jobs);
    for (size_t i = 0; i < max_jobs; ++i)
    {
        workers_.emplace_back([this]{ worker_thread(); });
    }
}

job_queue::~job_queue()
{
    stop_.store(true, std::memory_order_release);
    cv_.notify_all();
    join_all();
}

void job_queue::push(const std::shared_ptr<job>& job)
{
    if (stop_.load(std::memory_order_acquire) || !job)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        job_queue_.emplace_back(job);

        notify(*job);
    }

    cv_.notify_one();
}

void job_queue::worker_thread()
{
    while (true)
    {
        std::shared_ptr<job> job;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this]
            {
                return stop_.load(std::memory_order_acquire) || !job_queue_.empty();
            });

            if (stop_.load(std::memory_order_acquire))
            {
                break;
            }

            job = job_queue_.front();
            job_queue_.erase(job_queue_.begin());
        }

        run(job);
    }
}

void job_queue::run(const std::shared_ptr<job>& job) const
{
    namespace fs = std::filesystem;
    if (!job) return;

    try
    {
        job->state.store(job_state::running, std::memory_order_release);
        notify(*job);

        fs::create_directories(job->output_path);
        const std::vector<std::string> stem_names = {"vocals", "drums", "bass", "other"};
        job->stems.clear();

        for (const auto& stem : stem_names)
        {
            std::string output_file = fs::path(job->output_path) / (fs::path(job->input_path).stem().string() + "_" + stem + ".wav");
            // Simulate processing time
            std::this_thread::sleep_for(std::chrono::seconds(1));
            job->stems.push_back(output_file);
            job->progress += 0.25f;

            std::ofstream ofs(output_file);
            ofs << "Simulated " << stem << " stem data for " << job->input_path << "\n";
            ofs.close();

            notify(*job);
        }

        job->state.store(job_state::completed, std::memory_order_release);

        notify(*job);
    }
    catch (std::exception& e) {
        job->state.store(job_state::failed, std::memory_order_release);
        job->error_message = e.what();

        notify(*job);
    }
}

void job_queue::notify(const job& job) const
{
    if (progress_callback)
    {
        progress_callback(job);
    }
}

void job_queue::join_all()
{
    for (auto& worker : workers_)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
}

} // namespace stemsmith