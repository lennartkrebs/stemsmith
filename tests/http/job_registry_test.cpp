#include <gtest/gtest.h>

#include "http/server.h"

TEST(job_registry_test, adds_and_retrieves_job)
{
    using namespace stemsmith;

    http::job_registry registry;
    const auto id = registry.next_id();
    registry.add(id, job_handle{}, std::filesystem::path("/tmp/upload.wav"));

    const auto state = registry.get(id);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->last_event.status, job_status::queued);
}

TEST(job_registry_test, updates_status_and_output_path)
{
    using namespace stemsmith;

    http::job_registry registry;
    const auto id = registry.next_id();
    registry.add(id, job_handle{}, std::filesystem::path("/tmp/upload.wav"));

    job_descriptor desc;
    desc.output_dir = std::filesystem::path("/tmp/output");

    job_event ev;
    ev.status = job_status::completed;
    ev.progress = 1.0f;
    registry.update(id, desc, ev);

    const auto state = registry.get(id);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->last_event.status, job_status::completed);
    EXPECT_EQ(state->last_event.progress, 1.0f);
    EXPECT_EQ(state->output_dir, desc.output_dir);
}
