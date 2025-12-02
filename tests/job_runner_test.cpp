#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <future>
#include <gtest/gtest.h>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "job_runner.h"
#include "support/fake_session.h"

namespace
{
std::filesystem::path write_temp_wav()
{
    const auto temp = std::filesystem::temp_directory_path() / "stemsmith-job.wav";
    std::ofstream out(temp);
    out << "data";
    out.close();
    return temp;
}
} // namespace

namespace stemsmith
{
TEST(job_runner_test, resolves_future_on_completion)
{
    std::vector<std::filesystem::path> writes;
    auto writer = [&](const std::filesystem::path& path, const audio_buffer&) -> std::expected<void, std::string>
    {
        writes.push_back(path);
        return {};
    };
    auto loader = [](const std::filesystem::path&) -> std::expected<audio_buffer, std::string>
    { return test::make_buffer(4); };

    model_session_pool pool([](model_profile_id id) -> std::expected<std::unique_ptr<model_session>, std::string>
                            { return test::make_stub_session(id); });

    const auto output_root = std::filesystem::temp_directory_path() / "stemsmith-job-output";
    std::filesystem::remove_all(output_root);

    separation_engine engine(std::move(pool), output_root, loader, writer);
    std::vector<std::string> progress_messages;
    std::vector<job_event> events;
    job_runner runner(std::move(engine),
                      job_template{},
                      1,
                      [&](const job_descriptor& job, const job_event& event)
                      {
                          events.push_back(event);
                          if (event.progress >= 0.0f)
                          {
                              progress_messages.push_back(job.input_path.string() + ":" +
                                                          std::to_string(event.progress) + ":" + event.message);
                          }
                      });

    const auto input_path_wav = write_temp_wav();
    job_request request;
    request.input_path = input_path_wav;
    auto submit_result = runner.submit(request);
    ASSERT_TRUE(submit_result.has_value());

    auto handle = std::move(submit_result.value());
    const auto [input_path, output_dir, status, error] = handle.result().get();

    EXPECT_EQ(status, job_status::completed);
    EXPECT_EQ(input_path, input_path.lexically_normal());
    EXPECT_EQ(output_dir, output_root / input_path.stem());
    EXPECT_FALSE(error.has_value());
    EXPECT_EQ(writes.size(), 6U);
    EXPECT_FALSE(progress_messages.empty());
    EXPECT_TRUE(std::ranges::any_of(events, [](const job_event& evt) { return evt.status == job_status::completed; }));
}

TEST(job_runner_test, emits_progress_events_in_order)
{
    auto loader = [](const std::filesystem::path&) -> std::expected<audio_buffer, std::string>
    { return test::make_buffer(4); };

    auto writer = [](const std::filesystem::path&, const audio_buffer&) -> std::expected<void, std::string>
    { return {}; };

    model_session_pool pool([](model_profile_id id) -> std::expected<std::unique_ptr<model_session>, std::string>
                            { return test::make_stub_session(id); });

    const auto output_root = std::filesystem::temp_directory_path() / "stemsmith-job-progress";
    std::filesystem::remove_all(output_root);

    separation_engine engine(std::move(pool), output_root, loader, writer);
    std::vector<float> progress_values;
    job_runner runner(std::move(engine),
                      job_template{},
                      1,
                      [&](const job_descriptor&, const job_event& event)
                      {
                          if (event.progress >= 0.0f)
                          {
                              progress_values.push_back(event.progress);
                          }
                      });

    const auto input_path = write_temp_wav();
    job_request request;
    request.input_path = input_path;
    auto submit_result = runner.submit(request);
    ASSERT_TRUE(submit_result.has_value());

    // Wait for completion
    const auto future = submit_result->result();
    const auto status = future.get().status;
    ASSERT_EQ(status, job_status::completed);
    const std::vector expected{0.0f, 0.25f, 0.5f, 1.0f};
    ASSERT_EQ(progress_values, expected);
}

TEST(job_runner_test, reports_status_flow)
{
    auto loader = [](const std::filesystem::path&) -> std::expected<audio_buffer, std::string>
    { return test::make_buffer(4); };
    auto writer = [](const std::filesystem::path&, const audio_buffer&) -> std::expected<void, std::string>
    { return {}; };

    model_session_pool pool([](model_profile_id id) -> std::expected<std::unique_ptr<model_session>, std::string>
                            { return test::make_stub_session(id); });

    const auto output_root = std::filesystem::temp_directory_path() / "stemsmith-job-status";
    std::filesystem::remove_all(output_root);

    separation_engine engine(std::move(pool), output_root, loader, writer);
    std::vector<job_event> events;
    job_runner runner(std::move(engine),
                      job_template{},
                      1,
                      [&](const job_descriptor&, const job_event& event) { events.push_back(event); });

    const auto input_path = write_temp_wav();
    job_request request;
    request.input_path = input_path;
    auto submit_result = runner.submit(request);
    ASSERT_TRUE(submit_result.has_value());

    const auto future = submit_result->result();
    const auto status = future.get().status;
    ASSERT_EQ(status, job_status::completed);
    std::vector<job_status> timeline;
    for (const auto& evt : events)
    {
        if (evt.progress < 0.0f)
        {
            timeline.push_back(evt.status);
        }
    }
    ASSERT_GE(timeline.size(), 3U);
    std::vector<job_status> expected{job_status::queued, job_status::running, job_status::completed};
    EXPECT_TRUE(std::equal(expected.begin(), expected.end(), timeline.begin()));
}

TEST(job_runner_test, propagates_engine_errors_to_future)
{
    auto loader = [](const std::filesystem::path&) -> std::expected<audio_buffer, std::string>
    { return test::make_buffer(4); };

    auto writer = [](const std::filesystem::path&, const audio_buffer&) -> std::expected<void, std::string>
    { return std::unexpected("writer failed"); };

    model_session_pool pool([](model_profile_id id) -> std::expected<std::unique_ptr<model_session>, std::string>
                            { return test::make_stub_session(id); });

    const auto output_root = std::filesystem::temp_directory_path() / "stemsmith-job-output";
    std::filesystem::remove_all(output_root);

    separation_engine engine(std::move(pool), output_root, loader, writer);
    job_runner runner(std::move(engine), job_template{}, 1);

    const auto input_path = write_temp_wav();
    job_request request;
    request.input_path = input_path;
    auto submit_result = runner.submit(request);
    ASSERT_TRUE(submit_result.has_value());

    auto result = submit_result->result().get();
    EXPECT_EQ(result.status, job_status::failed);
    ASSERT_TRUE(result.error.has_value());
    const auto error = result.error.value();
    EXPECT_NE(error.find("writer failed"), std::string::npos);
}

TEST(job_runner_test, request_observer_receives_events)
{
    auto loader = [](const std::filesystem::path&) -> std::expected<audio_buffer, std::string>
    { return test::make_buffer(4); };
    auto writer = [](const std::filesystem::path&, const audio_buffer&) -> std::expected<void, std::string>
    { return {}; };

    model_session_pool pool([](model_profile_id id) -> std::expected<std::unique_ptr<model_session>, std::string>
                            { return test::make_stub_session(id); });

    const auto output_root = std::filesystem::temp_directory_path() / "stemsmith-job-request-observer";
    std::filesystem::remove_all(output_root);

    separation_engine engine(std::move(pool), output_root, loader, writer);
    job_runner runner(std::move(engine), job_template{}, 1);

    std::vector<job_status> statuses;
    std::mutex statuses_mutex;
    job_observer observer;
    observer.callback = [&](const job_descriptor&, const job_event& event)
    {
        if (event.progress < 0.0f)
        {
            std::lock_guard lock(statuses_mutex);
            statuses.push_back(event.status);
        }
    };

    const auto input_path = write_temp_wav();
    job_request request;
    request.input_path = input_path;
    request.observer = std::move(observer);
    auto handle_expected = runner.submit(request);
    ASSERT_TRUE(handle_expected.has_value());

    const auto result = handle_expected->result().get();
    EXPECT_EQ(result.status, job_status::completed);

    std::lock_guard lock(statuses_mutex);
    ASSERT_GE(statuses.size(), 3U);
    EXPECT_EQ(statuses.front(), job_status::queued);
    EXPECT_EQ(statuses.back(), job_status::completed);
}

TEST(job_runner_test, handle_observer_receives_events)
{
    auto loader = [](const std::filesystem::path&) -> std::expected<audio_buffer, std::string>
    { return test::make_buffer(4); };

    std::mutex writer_mutex;
    std::condition_variable writer_cv;
    bool allow_write = false;
    auto writer = [&](const std::filesystem::path&, const audio_buffer&) -> std::expected<void, std::string>
    {
        std::unique_lock lock(writer_mutex);
        writer_cv.wait(lock, [&] { return allow_write; });
        return {};
    };

    model_session_pool pool([](model_profile_id id) -> std::expected<std::unique_ptr<model_session>, std::string>
                            { return test::make_stub_session(id); });

    const auto output_root = std::filesystem::temp_directory_path() / "stemsmith-job-handle-observer";
    std::filesystem::remove_all(output_root);

    separation_engine engine(std::move(pool), output_root, loader, writer);
    job_runner runner(std::move(engine), job_template{}, 1);

    const auto input_path = write_temp_wav();
    job_request request;
    request.input_path = input_path;
    auto handle_expected = runner.submit(request);
    ASSERT_TRUE(handle_expected.has_value());

    std::mutex observer_mutex;
    std::condition_variable observer_cv;
    bool saw_completion = false;
    job_observer observer;
    observer.callback = [&](const job_descriptor&, const job_event& event)
    {
        if (event.status == job_status::completed)
        {
            std::lock_guard lock(observer_mutex);
            saw_completion = true;
            observer_cv.notify_all();
        }
    };

    handle_expected->set_observer(std::move(observer));

    {
        std::lock_guard lock(writer_mutex);
        allow_write = true;
    }
    writer_cv.notify_all();

    const auto result = handle_expected->result().get();
    EXPECT_EQ(result.status, job_status::completed);

    std::unique_lock lock(observer_mutex);
    ASSERT_TRUE(observer_cv.wait_for(lock, std::chrono::seconds(2), [&] { return saw_completion; }));
}

TEST(job_runner_test, handle_cancel_cancels_pending_job)
{
    auto loader = [](const std::filesystem::path&) -> std::expected<audio_buffer, std::string>
    { return test::make_buffer(4); };

    std::mutex writer_mutex;
    std::condition_variable writer_cv;
    bool allow_writes = false;
    auto writer = [&](const std::filesystem::path&, const audio_buffer&) -> std::expected<void, std::string>
    {
        std::unique_lock lock(writer_mutex);
        writer_cv.wait(lock, [&] { return allow_writes; });
        return {};
    };

    model_session_pool pool([](model_profile_id id) -> std::expected<std::unique_ptr<model_session>, std::string>
                            { return test::make_stub_session(id); });

    const auto output_root = std::filesystem::temp_directory_path() / "stemsmith-job-handle";
    std::filesystem::remove_all(output_root);

    separation_engine engine(std::move(pool), output_root, loader, writer);
    std::mutex events_mutex;
    std::condition_variable events_cv;
    bool first_job_running = false;
    bool second_job_cancelled = false;
    std::filesystem::create_directories(output_root);

    const auto first_input = output_root / "first.wav";
    const auto second_input = output_root / "second.wav";
    {
        std::ofstream out(first_input);
        out << "data";
    }
    {
        std::ofstream out(second_input);
        out << "data";
    }

    job_runner runner(std::move(engine),
                      job_template{},
                      1,
                      [&](const job_descriptor& job, const job_event& event)
                      {
                          std::lock_guard lock(events_mutex);
                          if (job.input_path == first_input && event.status == job_status::running &&
                              event.progress < 0.0f)
                          {
                              first_job_running = true;
                              events_cv.notify_all();
                          }
                          if (job.input_path == second_input && event.status == job_status::cancelled)
                          {
                              second_job_cancelled = true;
                              events_cv.notify_all();
                          }
                      });

    job_request first_request;
    first_request.input_path = first_input;
    auto first_handle_expected = runner.submit(first_request);
    ASSERT_TRUE(first_handle_expected.has_value());
    job_request second_request;
    second_request.input_path = second_input;
    auto second_handle_expected = runner.submit(second_request);
    ASSERT_TRUE(second_handle_expected.has_value());
    auto first_handle = std::move(first_handle_expected.value());
    auto second_handle = std::move(second_handle_expected.value());

    {
        std::unique_lock lock(events_mutex);
        ASSERT_TRUE(events_cv.wait_for(lock, std::chrono::seconds(2), [&] { return first_job_running; }));
    }

    auto cancel_result = second_handle.cancel("Second job cancelled");
    ASSERT_TRUE(cancel_result.has_value());

    {
        std::unique_lock lock(events_mutex);
        ASSERT_TRUE(events_cv.wait_for(lock, std::chrono::seconds(2), [&] { return second_job_cancelled; }));
    }

    const auto cancelled = second_handle.result().get();
    EXPECT_EQ(cancelled.status, job_status::cancelled);
    ASSERT_TRUE(cancelled.error.has_value());
    EXPECT_NE(cancelled.error->find("Second job cancelled"), std::string::npos);

    {
        std::lock_guard lock(writer_mutex);
        allow_writes = true;
    }
    writer_cv.notify_all();

    const auto completed = first_handle.result().get();
    EXPECT_EQ(completed.status, job_status::completed);

    std::filesystem::remove_all(output_root);
}
} // namespace stemsmith
