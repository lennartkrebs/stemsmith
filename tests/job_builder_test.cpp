#include <gtest/gtest.h>

#include "job_builder.h"

using namespace stemsmith;

TEST(JobBuilder, BuildSuccess)
{
    job_builder b;
    auto params = b.with_input("/tmp/in.wav").with_output("/tmp/out").with_model("htdemucs").with_mode("fast").build();
    EXPECT_EQ(params.input_path, "/tmp/in.wav");
    EXPECT_EQ(params.output_path, "/tmp/out");
    EXPECT_EQ(params.model, "htdemucs");
    EXPECT_EQ(params.mode, "fast");
}

TEST(JobBuilder, MissingInputThrows)
{
    job_builder b;
    b.with_output("/tmp/out");
    EXPECT_THROW(b.build(), std::invalid_argument);
}

TEST(JobBuilder, MissingOutputThrows)
{
    job_builder b;
    b.with_input("/tmp/in.wav");
    EXPECT_THROW(b.build(), std::invalid_argument);
}

