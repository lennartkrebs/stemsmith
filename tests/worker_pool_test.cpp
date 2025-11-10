#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "stemsmith/job_catalog.h"
#include "stemsmith/worker_pool.h"

namespace
{
stemsmith::job_descriptor make_job(const std::string& path)
{
    return stemsmith::job_descriptor{
        std::filesystem::path{path}, stemsmith::job_config{}};
}
} // namespace

namespace stemsmith
{
TEST(worker_pool_test, process_jobs_and_emit_events)
{
    std::mutex events_mutex;
    std::condition_variable events_cv;
    std::vector<job_event> events;
    std::vector<std::string> processed_paths;
    std::mutex processed_mutex;
    std::size_t completed = 0;

    const worker_pool pool(
        1,
        [&](const job_descriptor& job, const std::atomic_bool& stop_flag) {
            ASSERT_FALSE(stop_flag.load());
            std::lock_guard lock(processed_mutex);
            processed_paths.push_back(job.input_path.string());
        },
        [&](const job_event& event) {
            std::lock_guard lock(events_mutex);
            events.push_back(event);
            if (event.status == job_status::completed)
            {
                ++completed;
                events_cv.notify_all();
            }
        });

    const auto first_id = pool.enqueue(make_job("/music/first.wav"));
    const auto second_id = pool.enqueue(make_job("/music/second.wav"));

    {
        std::unique_lock lock(events_mutex);
        ASSERT_TRUE(events_cv.wait_for(lock,
            std::chrono::milliseconds(200),
            [&] { return completed == 2; }));
    }

    pool.shutdown();

    {
        std::lock_guard lock(processed_mutex);
        const std::vector<std::string> expected{"/music/first.wav", "/music/second.wav"};
        EXPECT_EQ(processed_paths, expected);
    }

    {
        std::lock_guard lock(events_mutex);
        ASSERT_EQ(events.size(), 6U);
        EXPECT_EQ(events[0].id, first_id);
        EXPECT_EQ(events[0].status, job_status::queued);
        EXPECT_EQ(events[1].id, second_id);
        EXPECT_EQ(events[1].status, job_status::queued);
        EXPECT_EQ(events[2].id, first_id);
        EXPECT_EQ(events[2].status, job_status::running);
        EXPECT_EQ(events[3].id, first_id);
        EXPECT_EQ(events[3].status, job_status::completed);
        EXPECT_EQ(events[4].id, second_id);
        EXPECT_EQ(events[4].status, job_status::running);
        EXPECT_EQ(events[5].id, second_id);
        EXPECT_EQ(events[5].status, job_status::completed);
    }
}

TEST(worker_pool_test, cancels_pending_jobs_on_shutdown)
{
    std::mutex events_mutex;
    std::condition_variable events_cv;
    std::vector<job_event> events;
    bool first_job_running = false;

    std::atomic processor_calls{0};

    const worker_pool pool(
        1,
        [&](const job_descriptor&, const std::atomic_bool& stop_flag) {
            processor_calls.fetch_add(1);
            {
                std::lock_guard lock(events_mutex);
                first_job_running = true;
                events_cv.notify_all();
            }

            while (!stop_flag.load())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            throw std::runtime_error("stop requested");
        },
        [&](const job_event& event) {
            std::lock_guard lock(events_mutex);
            events.push_back(event);
            if (event.status == job_status::running && event.id == 0)
            {
                first_job_running = true;
                events_cv.notify_all();
            }
        });

    const auto first_id = pool.enqueue(make_job("/music/running.wav"));
    const auto second_id = pool.enqueue(make_job("/music/queued.wav"));

    {
        std::unique_lock lock(events_mutex);
        ASSERT_TRUE(events_cv.wait_for(lock,
            std::chrono::milliseconds(200),
            [&] { return first_job_running; }));
    }

    pool.shutdown();

    EXPECT_EQ(processor_calls.load(), 1);

    std::lock_guard lock(events_mutex);
    const auto cancelled = std::ranges::find_if(events,
                                                [&](const job_event& event) {
                                                    return event.id == second_id && event.status == job_status::cancelled;
                                                });
    ASSERT_NE(cancelled, events.end());

    const auto failed = std::ranges::find_if(events,
                                             [&](const job_event& event) {
                                                 return event.id == first_id && event.status == job_status::failed;
                                             });
    ASSERT_NE(failed, events.end());
    ASSERT_TRUE(failed->error.has_value());
    EXPECT_NE(failed->error->find("stop requested"), std::string::npos);
}

TEST(worker_pool_test, rejects_enqueue_after_shutdown)
{
    int processed{0};
    std::mutex processed_mutex;
    std::condition_variable processed_cv;

    const worker_pool pool(
        1,
        [&](const job_descriptor&, const std::atomic_bool& stop_flag) {
            EXPECT_FALSE(stop_flag.load());
            {
                std::lock_guard lock(processed_mutex);
                processed = 1;
            }
            processed_cv.notify_all();
        });

    const auto first_id = pool.enqueue(make_job("/music/first.wav"));
    EXPECT_EQ(first_id, 0U);

    {
        std::unique_lock lock(processed_mutex);
        ASSERT_TRUE(processed_cv.wait_for(lock,
            std::chrono::milliseconds(200),
            [&] { return processed == 1; }));
    }

    pool.shutdown();

    EXPECT_EQ(processed, 1);
    EXPECT_TRUE(pool.is_shutdown());

    const auto second_id = pool.enqueue(make_job("/music/second.wav"));
    EXPECT_EQ(second_id, static_cast<std::size_t>(-1));
}

} // namespace stemsmith
