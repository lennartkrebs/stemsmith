#include <gtest/gtest.h>

#include "job_builder.h"

using namespace stemsmith;

TEST(Job, StatusStringDefaultQueued)
{
    job j;
    // default constructed state is queued as per header
    EXPECT_EQ(j.status_string(), std::string("queued"));
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
