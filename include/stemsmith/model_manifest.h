#pragma once

#include "stemsmith/job_catalog.h"

#include <expected>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace stemsmith
{
struct model_manifest_entry
{
    model_profile_id profile;
    std::string profile_key;
    std::string filename;
    std::string url;
    std::uint64_t size_bytes{};
    std::string sha256;
};

/**
 * @brief Manifest of available Demucs models.
 */
class model_manifest
{
public:
    model_manifest() = default;
    explicit model_manifest(std::vector<model_manifest_entry> entries);

    static std::expected<model_manifest, std::string> load_default();
    static std::expected<model_manifest, std::string>
    from_file(const std::filesystem::path& path);

    [[nodiscard]] const model_manifest_entry* find(model_profile_id profile) const;

private:
    std::map<model_profile_id, model_manifest_entry> entries_;
};
} // namespace stemsmith
