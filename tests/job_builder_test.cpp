#include <gtest/gtest.h>

#include "job_builder.h"

using namespace stemsmith;

TEST(job_builder, build_success)
{
    job_builder b;
    const auto [input_path, output_path, model, mode] = b
        .with_input("/tmp/in.wav")
        .with_output("/tmp/out")
        .with_model("htdemucs")
        .with_mode("fast")\
        .build();

    EXPECT_EQ(input_path, "/tmp/in.wav");
    EXPECT_EQ(output_path, "/tmp/out");
    EXPECT_EQ(model, "htdemucs");
    EXPECT_EQ(mode, "fast");
}

TEST(job_builder, missing_input_throws)
{
    job_builder b;
    b.with_output("/tmp/out");
    EXPECT_THROW(b.build(), std::invalid_argument);
}

TEST(job_builder, missing_output_throws)
{
    job_builder b;
    b.with_input("/tmp/in.wav");
    EXPECT_THROW(b.build(), std::invalid_argument);
}

