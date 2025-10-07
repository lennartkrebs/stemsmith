#include <gtest/gtest.h>
#include <job_manager.h>
#include <job_builder.h>

#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <mutex>
#include <condition_variable>

using namespace stemsmith;

namespace {
std::string make_temp_input(int idx) {
    namespace fs = std::filesystem;
    auto dir = fs::temp_directory_path();
    auto file = dir / ("stemsmith_input_" + std::to_string(idx) + ".wav");
    std::ofstream ofs(file.string());
    ofs << "dummy wav data";
    return file.string();
}

std::string make_temp_output_dir(int idx) {
    namespace fs = std::filesystem;
    auto dir = fs::temp_directory_path() / ("stemsmith_out_" + std::to_string(idx));
    fs::create_directories(dir);
    return dir.string();
}
}

TEST(job_manager, submit_and_complete_multiple_jobs_concurrently) {
    const int worker_threads = 3;
    auto mgr = std::make_shared<job_manager>(worker_threads);

    const int job_count = 4; // intentionally > worker_threads to exercise queueing
    std::vector<std::string> ids;
    ids.reserve(job_count);

    std::mutex mtx;
    std::condition_variable cv;
    int completed = 0;

    std::unordered_map<std::string, std::thread::id> job_thread;

    // Subscribe to observe state transitions from worker threads
    auto sub_id = mgr->subscribe([&](const job_manager::job_ptr &j){
        if (!j) return;
        // Record first thread id we see for this job when it starts running
        if (j->state.load() == job_state::running) {
            std::lock_guard lk(mtx);
            job_thread.try_emplace(j->id, std::this_thread::get_id());
        }
        if (j->state.load() == job_state::completed) {
            std::lock_guard lk(mtx);
            completed++;
            cv.notify_all();
        }
    });

    // Submit jobs
    for (int i = 0; i < job_count; ++i) {
        job_parameters params;
        params.input_path = make_temp_input(i);
        params.output_path = make_temp_output_dir(i);
        params.model = "htdemucs";
        params.mode = i % 2 == 0 ? "fast" : "hq";
        ids.push_back(mgr->submit_job(params));
    }

    // Wait for all to complete with timeout safeguard
    {
        std::unique_lock lk(mtx);
        ASSERT_TRUE(cv.wait_for(lk, std::chrono::seconds(30), [&]{return completed == job_count;}));
    }

    // Validate each job object
    for (size_t i = 0; i < ids.size(); ++i) {
        auto j = mgr->get_job(ids[i]);
        ASSERT_TRUE(j) << "Job not found: " << ids[i];
        EXPECT_EQ(j->state.load(), job_state::completed);
        EXPECT_FLOAT_EQ(j->progress.load(), 1.0f);
        EXPECT_EQ(j->stems.size(), 4u); // simulated stems
        EXPECT_FALSE(j->input_path.empty());
        EXPECT_FALSE(j->output_path.empty());
    }

    // Concurrency evidence: at least two distinct worker thread ids processed jobs
    {
        std::lock_guard lk(mtx);
        std::set<std::thread::id> distinct;
        for (auto &p : job_thread) distinct.insert(p.second);
        EXPECT_GE(distinct.size(), 2u) << "Expected at least 2 worker threads handling jobs";
    }

    mgr->unsubscribe(sub_id);
}

TEST(job_manager, job_parameters_propagated) {
    auto mgr = std::make_shared<job_manager>(2);
    job_parameters p;
    p.input_path = make_temp_input(100);
    p.output_path = make_temp_output_dir(100);
    p.model = "custom_model";
    p.mode = "hq";

    auto id = mgr->submit_job(p);
    auto j = mgr->get_job(id);
    ASSERT_TRUE(j);
    EXPECT_EQ(j->input_path, p.input_path);
    EXPECT_EQ(j->output_path, p.output_path);
    EXPECT_EQ(j->model_name, p.model);
    EXPECT_EQ(j->mode, p.mode);
}

