#include <gtest/gtest.h>

#include "stemsmith/bootstrap.h"

namespace
{
TEST(bootstrap_test, demucs_dependencies_ready)
{
    // Guard rail: verify the Demucs vendor tree links before project tests run.
    EXPECT_TRUE(stemsmith::demucs_dependencies_ready())
        << "Demucs cross-transformer scaffolding failed to initialize. "
           "Check FetchContent dependencies and OpenMP configuration.";
}
} // namespace
