#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace stemsmith
{
enum class model_profile_id
{
    balanced_four_stem,
    balanced_six_stem
};

struct model_profile
{
    model_profile_id id;
    std::string_view key; // stable config key, e.g. "balanced-six-stem"
    std::string_view label; // human readable label for UIs
    std::string_view weight_filename; // Demucs weight file name
    std::array<std::string_view, 6> stems; // ordered list of stems for the profile
    std::size_t stem_count;
};

std::optional<model_profile> lookup_profile(model_profile_id id);
std::optional<model_profile> lookup_profile(std::string_view key);

/**
 * @brief Configuration for a single separation job.
 */
struct job_config
{
    model_profile_id profile{model_profile_id::balanced_six_stem};
    std::vector<std::string> stems_filter{}; // optional subset, empty -> all
    std::filesystem::path cache_root{"build/cache"};

    [[nodiscard]] std::vector<std::string> resolved_stems() const;
    static std::expected<job_config, std::string> from_file(const std::filesystem::path& path);
};

} // namespace stemsmith
