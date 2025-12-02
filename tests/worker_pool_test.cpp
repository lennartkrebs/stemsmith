#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <gtest/gtest.h>
#include <latch>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "job_catalog.h"
#include "worker_pool.h"

namespace
{
stemsmith::job_descriptor make_job(const std::string& path)
{
    return stemsmith::job_descriptor{std::filesystem::path{path}, stemsmith::job_template{}, {}};
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

    std::latch start_latch(1);
    worker_pool pool(
        1,
        [&](const job_descriptor& job, const std::atomic_bool& stop_flag)
        {
            start_latch.wait();
            ASSERT_FALSE(stop_flag.load());
            std::lock_guard lock(processed_mutex);
            processed_paths.push_back(job.input_path.string());
        },
        [&](const job_event& event)
        {
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

    start_latch.count_down();

    {
        std::unique_lock lock(events_mutex);
        ASSERT_TRUE(events_cv.wait_for(lock, std::chrono::milliseconds(100), [&] { return completed == 2; }));
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
        const auto statuses_for = [&](std::size_t id)
        {
            std::vector<job_status> statuses;
            for (const auto& event : events)
            {
                if (event.id == id)
                {
                    statuses.push_back(event.status);
                }
            }
            return statuses;
        };

        const auto first_statuses = statuses_for(first_id);
        ASSERT_EQ(first_statuses.size(), 3U);
        EXPECT_EQ(first_statuses[0], job_status::queued);
        EXPECT_EQ(first_statuses[1], job_status::running);
        EXPECT_EQ(first_statuses[2], job_status::completed);

        const auto second_statuses = statuses_for(second_id);
        ASSERT_EQ(second_statuses.size(), 3U);
        EXPECT_EQ(second_statuses[0], job_status::queued);
        EXPECT_EQ(second_statuses[1], job_status::running);
        EXPECT_EQ(second_statuses[2], job_status::completed);
    }
}

TEST(worker_pool_test, cancels_pending_jobs_on_shutdown)
{
    std::mutex events_mutex;
    std::condition_variable events_cv;
    std::vector<job_event> events;
    bool first_job_running = false;

    std::atomic processor_calls{0};

    worker_pool pool(
        1,
        [&](const job_descriptor&, const std::atomic_bool& stop_flag)
        {
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
        [&](const job_event& event)
        {
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
        ASSERT_TRUE(events_cv.wait_for(lock, std::chrono::seconds(2), [&] { return first_job_running; }));
    }

    pool.shutdown();

    EXPECT_EQ(processor_calls.load(), 1);

    std::lock_guard lock(events_mutex);
    const auto cancelled = std::ranges::find_if(
        events,
        [&](const job_event& event) { return event.id == second_id && event.status == job_status::cancelled; });
    ASSERT_NE(cancelled, events.end());

    const auto running_cancelled = std::ranges::find_if(
        events,
        [&](const job_event& event) { return event.id == first_id && event.status == job_status::cancelled; });
    ASSERT_NE(running_cancelled, events.end());
    ASSERT_TRUE(running_cancelled->error.has_value());
    EXPECT_NE(running_cancelled->error->find("Worker pool shutting down"), std::string::npos);
}

TEST(worker_pool_test, rejects_enqueue_after_shutdown)
{
    int processed{0};
    std::mutex processed_mutex;
    std::condition_variable processed_cv;

    worker_pool pool(1,
                     [&](const job_descriptor&, const std::atomic_bool& stop_flag)
                     {
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
        ASSERT_TRUE(processed_cv.wait_for(lock, std::chrono::seconds(2), [&] { return processed == 1; }));
    }

    pool.shutdown();

    EXPECT_EQ(processed, 1);
    EXPECT_TRUE(pool.is_shutdown());

    const auto second_id = pool.enqueue(make_job("/music/second.wav"));
    EXPECT_EQ(second_id, static_cast<std::size_t>(-1));
}

TEST(worker_pool_test, cancel_queued_job_emits_event)
{
    std::mutex events_mutex;
    std::condition_variable events_cv;
    std::vector<job_event> events;
    std::atomic_bool first_job_started{false};
    std::atomic_bool allow_exit{false};

    worker_pool pool(
        1,
        [&](const job_descriptor&, const std::atomic_bool&)
        {
            first_job_started.store(true);
            while (!allow_exit.load())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        },
        [&](const job_event& event)
        {
            std::lock_guard lock(events_mutex);
            events.push_back(event);
            events_cv.notify_all();
        });

    const auto first_id = pool.enqueue(make_job("/music/first.wav"));
    const auto second_id = pool.enqueue(make_job("/music/queued.wav"));

    while (!first_job_started.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_TRUE(pool.cancel(second_id, "User cancelled job"));

    {
        std::unique_lock lock(events_mutex);
        ASSERT_TRUE(events_cv.wait_for(lock,
                                       std::chrono::seconds(2),
                                       [&]
                                       {
                                           return std::ranges::any_of(
                                               events,
                                               [&](const job_event& evt)
                                               { return evt.id == second_id && evt.status == job_status::cancelled; });
                                       }));
    }

    allow_exit.store(true);
    {
        std::unique_lock lock(events_mutex);
        ASSERT_TRUE(events_cv.wait_for(lock,
                                       std::chrono::seconds(2),
                                       [&]
                                       {
                                           return std::ranges::any_of(
                                               events,
                                               [&](const job_event& evt)
                                               { return evt.id == first_id && evt.status == job_status::completed; });
                                       }));
    }
    pool.shutdown();

    const auto queued_cancelled = std::ranges::find_if(
        events,
        [&](const job_event& event) { return event.id == second_id && event.status == job_status::cancelled; });
    ASSERT_NE(queued_cancelled, events.end());
    ASSERT_TRUE(queued_cancelled->error.has_value());
    EXPECT_NE(queued_cancelled->error->find("User cancelled job"), std::string::npos);

    const auto first_completed = std::ranges::find_if(
        events,
        [&](const job_event& event) { return event.id == first_id && event.status == job_status::completed; });
    ASSERT_NE(first_completed, events.end());
}

TEST(worker_pool_test, cancel_running_job_sets_stop_flag)
{
    std::mutex events_mutex;
    std::condition_variable events_cv;
    bool cancelled = false;
    std::atomic_bool stop_observed{false};
    std::latch running_latch(1);

    worker_pool pool(
        1,
        [&](const job_descriptor&, const std::atomic_bool& stop_flag)
        {
            running_latch.count_down();
            while (!stop_flag.load())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            stop_observed.store(true);
        },
        [&](const job_event& event)
        {
            if (event.status != job_status::cancelled)
            {
                return;
            }
            std::lock_guard lock(events_mutex);
            cancelled = true;
            events_cv.notify_all();
            ASSERT_TRUE(event.error.has_value());
            EXPECT_NE(event.error->find("User requested stop"), std::string::npos);
        });

    const auto job_id = pool.enqueue(make_job("/music/running.wav"));
    running_latch.wait();

    EXPECT_TRUE(pool.cancel(job_id, "User requested stop"));

    {
        std::unique_lock lock(events_mutex);
        ASSERT_TRUE(events_cv.wait_for(lock, std::chrono::seconds(2), [&] { return cancelled; }));
    }

    pool.shutdown();
    EXPECT_TRUE(stop_observed.load());
}

} // namespace stemsmith
