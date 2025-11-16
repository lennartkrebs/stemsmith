#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <future>
#include <optional>
#include <string>
#include <vector>

#include "stemsmith/job_runner.h"
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
    auto writer = [&](const std::filesystem::path& path, const audio_buffer&) -> std::expected<void, std::string> {
        writes.push_back(path);
        return {};
    };
    auto loader = [](const std::filesystem::path&) -> std::expected<audio_buffer, std::string> {
        return test::make_buffer(4);
    };

    model_session_pool pool(
        [](model_profile_id id) -> std::expected<std::unique_ptr<model_session>, std::string> {
            return test::make_stub_session(id);
        });

    const auto output_root = std::filesystem::temp_directory_path() / "stemsmith-job-output";
    std::filesystem::remove_all(output_root);

    separation_engine engine(std::move(pool), output_root, loader, writer);
    std::vector<std::string> progress_messages;
    std::vector<job_event> events;
    job_runner runner({}, std::move(engine), 1, [&](const job_descriptor& job, const job_event& event) {
        events.push_back(event);
        if (event.progress >= 0.0f)
        {
            progress_messages.push_back(job.input_path.string() + ":" + std::to_string(event.progress) + ":" + event.message);
        }
    });

    const auto input_path_wav = write_temp_wav();
    auto submit_result = runner.submit(input_path_wav);
    ASSERT_TRUE(submit_result.has_value());

    auto future = std::move(submit_result.value());
    const auto [input_path, output_dir, status, error] = future.get();

    EXPECT_EQ(status, job_status::completed);
    EXPECT_EQ(input_path, input_path.lexically_normal());
    EXPECT_EQ(output_dir, output_root / input_path.stem());
    EXPECT_FALSE(error.has_value());
    EXPECT_EQ(writes.size(), 6U);
    EXPECT_FALSE(progress_messages.empty());
    EXPECT_TRUE(std::any_of(events.begin(), events.end(), [](const job_event& evt) {
        return evt.status == job_status::completed;
    }));
}

TEST(job_runner_test, emits_progress_events_in_order)
{
    auto loader = [](const std::filesystem::path&) -> std::expected<audio_buffer, std::string> {
        return test::make_buffer(4);
    };

    auto writer = [](const std::filesystem::path&, const audio_buffer&) -> std::expected<void, std::string> {
        return {};
    };

    model_session_pool pool(
        [](model_profile_id id) -> std::expected<std::unique_ptr<model_session>, std::string> {
            return test::make_stub_session(id);
        });

    const auto output_root = std::filesystem::temp_directory_path() / "stemsmith-job-progress";
    std::filesystem::remove_all(output_root);

    separation_engine engine(std::move(pool), output_root, loader, writer);
    std::vector<std::string> progress_events;
    job_runner runner({}, std::move(engine), 1, [&](const job_descriptor& job, const job_event& event) {
        if (event.progress >= 0.0f)
        {
            progress_events.push_back(job.input_path.filename().string() + ":" + std::to_string(event.progress) + ":" + event.message);
        }
    });

    const auto input_path = write_temp_wav();
    auto submit_result = runner.submit(input_path);
    ASSERT_TRUE(submit_result.has_value());

    submit_result->get(); // wait for completion
    ASSERT_FALSE(progress_events.empty());
    EXPECT_TRUE(std::ranges::any_of(progress_events,
                            [](const std::string& evt) {
                                return evt.find("stub") != std::string::npos;
                            }));
}

TEST(job_runner_test, reports_status_flow)
{
    auto loader = [](const std::filesystem::path&) -> std::expected<audio_buffer, std::string> {
        return test::make_buffer(4);
    };
    auto writer = [](const std::filesystem::path&, const audio_buffer&) -> std::expected<void, std::string> {
        return {};
    };

    model_session_pool pool(
        [](model_profile_id id) -> std::expected<std::unique_ptr<model_session>, std::string> {
            return test::make_stub_session(id);
        });

    const auto output_root = std::filesystem::temp_directory_path() / "stemsmith-job-status";
    std::filesystem::remove_all(output_root);

    separation_engine engine(std::move(pool), output_root, loader, writer);
    std::vector<job_event> events;
    job_runner runner({}, std::move(engine), 1, [&](const job_descriptor&, const job_event& event) {
        events.push_back(event);
    });

    const auto input_path = write_temp_wav();
    auto submit_result = runner.submit(input_path);
    ASSERT_TRUE(submit_result.has_value());

    submit_result->get();
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

TEST(job_runner_test, forwards_progress_updates)
{
    auto loader = [](const std::filesystem::path&) -> std::expected<audio_buffer, std::string> {
        return test::make_buffer(4);
    };
    auto writer = [](const std::filesystem::path&, const audio_buffer&) -> std::expected<void, std::string> {
        return {};
    };

    model_session_pool pool(
        [](model_profile_id id) -> std::expected<std::unique_ptr<model_session>, std::string> {
            return test::make_stub_session(id, 4, 0.0f);
        });

    const auto output_root = std::filesystem::temp_directory_path() / "stemsmith-job-progress-flow";
    std::filesystem::remove_all(output_root);

    separation_engine engine(std::move(pool), output_root, loader, writer);
    std::vector<float> progress_values;
    job_runner runner({}, std::move(engine), 1, [&](const job_descriptor&, const job_event& event) {
        if (event.progress >= 0.0f)
        {
            progress_values.push_back(event.progress);
        }
    });

    const auto input_path = write_temp_wav();
    auto submit_result = runner.submit(input_path);
    ASSERT_TRUE(submit_result.has_value());
    submit_result->get();

    ASSERT_FALSE(progress_values.empty());
    EXPECT_TRUE(progress_values.front() >= 0.0f);
    EXPECT_TRUE(progress_values.back() <= 1.0f);
}

TEST(job_runner_test, propagates_engine_errors_to_future)
{
auto loader = [](const std::filesystem::path&) -> std::expected<audio_buffer, std::string> {
        return test::make_buffer(4);
    };

    auto writer = [](const std::filesystem::path&, const audio_buffer&) -> std::expected<void, std::string> {
        return std::unexpected("writer failed");
    };

    model_session_pool pool(
        [](model_profile_id id) -> std::expected<std::unique_ptr<model_session>, std::string> {
            return test::make_stub_session(id);
        });

    const auto output_root = std::filesystem::temp_directory_path() / "stemsmith-job-output";
    std::filesystem::remove_all(output_root);

    separation_engine engine(std::move(pool), output_root, loader, writer);
    job_runner runner({}, std::move(engine), 1);

    const auto input_path = write_temp_wav();
    auto submit_result = runner.submit(input_path);
    ASSERT_TRUE(submit_result.has_value());

    auto future = std::move(submit_result.value());
    auto result = future.get();
    EXPECT_EQ(result.status, job_status::failed);
    ASSERT_TRUE(result.error.has_value());
    const auto error = result.error.value();
    EXPECT_NE(error.find("writer failed"), std::string::npos);
}
} // namespace stemsmith
