#include "job_queue.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace stemsmith {

job_queue::job_queue(const size_t max_jobs)
    : workers_(max_jobs)
    , semaphore_(0)
{
    for (size_t i = 0; i < max_jobs; ++i)
    {
        workers_.emplace_back([this]{ worker_thread(); });
    }
}

job_queue::~job_queue()
{
    stop_ = true;
    semaphore_.release(static_cast<int>(workers_.size()));
    join_all();
}

void job_queue::push(const std::shared_ptr<job>& job)
{
    if (stop_.load(std::memory_order_acquire))
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        job_queue_.emplace_back(job);
    }

    semaphore_.release(1);
}

void job_queue::worker_thread()
{
    for (;;)
    {
        semaphore_.acquire();

        if (stop_.load(std::memory_order_acquire))
        {
            break;
        }

        std::shared_ptr<job> job;
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (!job_queue_.empty())
            {
                job = job_queue_.front();
                job_queue_.erase(job_queue_.begin());
            }
        }

        run(job);
    }
}

void job_queue::run(const std::shared_ptr<job>& job) const
{
    namespace fs = std::filesystem;

    if (!job)
    {
        return;
    }

    try
    {
        job->status = "running";
        job->progress = 0.0f;

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

            if (on_progress)
            {
                on_progress(*job);
            }

            std::ofstream ofs(output_file);
            ofs << "Simulated " << stem << " stem data for " << job->input_path << "\n";

            ofs.close();
        }

        job->status = "completed";
        job->progress = 1.0f;

        if (on_complete)
        {
            on_complete(*job);
        }
    }
    catch (std::exception& e)
    {
        job->status = "error";
        job->error_message = e.what();
        if (on_error)
        {
            on_error(*job);
        }
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