#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include "job_queue.h"
#include <filesystem>
#include <api_server.h>

int main()
{
    namespace fs = std::filesystem;

    // Create a temporary directory for testing
    std::string temp_dir = fs::temp_directory_path() / "stemsmith_test";
    fs::create_directories(temp_dir);

    auto config = stemsmith::server_config();
    config.port = 8080;
    config.bind_address = "127.0.0.1";

    const auto queue = std::make_shared<stemsmith::job_queue>(4);

    std::cout << "Starting StemSmith Server..." << std::endl;
    auto server = std::make_unique<stemsmith::api_server>(config, queue);
    server->run();

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Testing single job submission
    std::cout << "\nTesting single job submission" << std::endl;
    std::cout << "---------------------------------" << std::endl;

    auto job1 = std::make_shared<stemsmith::job>();
    job1->id = "test_job_001";
    job1->input_path = temp_dir + "/test_input.wav";
    job1->output_path = temp_dir + "/output_001";
    job1->model_name = "htdemucs";
    job1->mode = "hq";

    // Create dummy input file
    std::ofstream(job1->input_path) << "test audio data";

    // Variables to track callback execution
    std::atomic<bool> job_completed{false};
    std::mutex cv_mutex;
    std::condition_variable cv;
    int progress_count = 0;

    // Set up callbacks
    queue->on_progress = [&progress_count, &cv_mutex](const stemsmith::job& job)
    {
        std::lock_guard<std::mutex> lock(cv_mutex);
        progress_count++;
        std::cout << "[PROGRESS] Job " << job.id
            << " - Status: " << job.status
            << " - Progress: " << (job.progress.load() * 100) << "%\n";
    };

    queue->on_complete = [&job_completed, &cv](const stemsmith::job& job)
    {
        std::cout << "[COMPLETE] Job " << job.id
            << " - Status: " << job.status
            << " - Progress: " << (job.progress.load() * 100) << "%\n";
        std::cout << "Generated stems:\n";
        for (const auto& stem : job.stems)
        {
            std::cout << "  - " << stem << "\n";
        }
        job_completed = true;
        cv.notify_one();
    };

    queue->on_error = [](const stemsmith::job& job)
    {
        std::cout << "[ERROR] Job " << job.id
            << " - Error: " << job.error_message << "\n";
    };

    queue->push(job1);

    // Wait for job completion with timeout
    {
        std::unique_lock<std::mutex> lock(cv_mutex);
        if (cv.wait_for(lock, std::chrono::seconds(10), [&job_completed]
        {
            return job_completed.load();
        }))
        {
            std::cout << "\n✅ Job completed successfully!\n";
            std::cout << "Progress callbacks fired: " << progress_count << " times\n";
        }
        else
        {
            std::cout << "\n❌ Job timed out after 10 seconds\n";
        }
    }

    // Submit multiple jobs to test concurrent processing
    std::cout << "\n\nTesting multiple concurrent jobs\n";
    std::cout << "---------------------------------\n";

    std::atomic<int> completed_jobs{0};
    const int num_jobs = 3;

    for (int i = 0; i < num_jobs; i++)
    {
        auto job = std::make_shared<stemsmith::job>();
        job->id = "batch_job_" + std::to_string(i + 1);
        job->input_path = "/path/to/input/song" + std::to_string(i + 1) + ".wav";
        job->output_path = "/path/to/output/batch/";
        job->model_name = "htdemucs";
        job->mode = "fast";

        queue->push(job);
        std::cout << "Submitted: " << job->id << "\n";
    }

    // Redefine on_complete to count completed jobs
    queue->on_complete = [&completed_jobs, &cv](const stemsmith::job& job)
    {
        std::cout << "[BATCH COMPLETE] " << job.id << " finished\n";
        completed_jobs++;
        cv.notify_one();
    };

    // Wait for all jobs to complete
    {
        std::unique_lock<std::mutex> lock(cv_mutex);
        if (cv.wait_for(lock, std::chrono::seconds(20), [&completed_jobs, num_jobs]
        {
            return completed_jobs.load() >= num_jobs;
        }))
        {
            std::cout << "\n✅ All " << num_jobs << " jobs completed!\n";
        }
        else
        {
            std::cout << "\n❌ Not all jobs completed. Finished: "
                << completed_jobs.load() << "/" << num_jobs << "\n";
        }
    }

    std::cout << "\nTest completed. Shutting down...\n";

    return 0;
}
