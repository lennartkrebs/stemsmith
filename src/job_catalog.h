#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "stemsmith/service.h"

namespace stemsmith
{
struct job_overrides
{
    std::optional<model_profile_id> profile{};
    std::optional<std::vector<std::string>> stems_filter{};
};

/**
 * @brief Manages a catalog of jobs to be processed.
 */
class job_catalog
{
public:
    using exists_function = std::function<bool(const std::filesystem::path&)>;

    explicit job_catalog(job_template base_config = {}, exists_function exists_provider = nullptr);

    std::expected<std::size_t, std::string> add_file(const std::filesystem::path& path,
                                                     const job_overrides& overrides,
                                                     std::filesystem::path output_dir);

    [[nodiscard]] const std::vector<job_descriptor>& jobs() const noexcept
    {
        return jobs_;
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return jobs_.size();
    }
    [[nodiscard]] bool empty() const noexcept
    {
        return jobs_.empty();
    }
    void release(const std::filesystem::path& path);

private:
    job_template base_config_;
    exists_function exists_;
    std::vector<job_descriptor> jobs_;
    std::unordered_set<std::filesystem::path> seen_paths_;

    [[nodiscard]] static std::filesystem::path normalize(const std::filesystem::path& path);
    [[nodiscard]] std::expected<job_template, std::string> apply_overrides(const job_overrides& overrides) const;
};
} // namespace stemsmith
