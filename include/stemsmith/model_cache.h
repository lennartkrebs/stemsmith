#pragma once

#include "stemsmith/job_catalog.h"
#include "stemsmith/model_manifest.h"

#include <expected>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace stemsmith
{
struct weight_fetcher;

struct model_handle
{
    model_profile_id profile;
    std::filesystem::path weights_path;
    std::string sha256;
    std::uint64_t size_bytes{};
    bool was_cached{false};
};

/**
 * @brief Manages downloading and caching of Demucs model weights.
 */
class model_cache
{
public:
    static std::expected<model_cache, std::string> create(std::filesystem::path cache_root, std::shared_ptr<weight_fetcher> fetcher);

    model_cache(std::filesystem::path cache_root,
                std::shared_ptr<weight_fetcher> fetcher,
                model_manifest manifest);

    // Disable copy, enable move
    model_cache(model_cache&&) noexcept = default;
    model_cache& operator=(model_cache&&) noexcept = default;
    model_cache(const model_cache&) = delete;
    model_cache& operator=(const model_cache&) = delete;

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return cache_root_; }

    std::expected<model_handle, std::string> ensure_ready(model_profile_id profile);
    std::expected<void, std::string> purge(model_profile_id profile);
    std::expected<void, std::string> purge_all();

    static std::expected<bool, std::string> verify_checksum(const std::filesystem::path& path, const model_manifest_entry& entry);
private:
    std::filesystem::path cache_root_;
    std::shared_ptr<weight_fetcher> fetcher_;
    model_manifest manifest_;

    struct profile_state
    {
        std::mutex mutex;
    };
    std::map<model_profile_id, std::shared_ptr<profile_state>> profile_states_;

    profile_state& state_for(model_profile_id profile);
    std::expected<model_handle, std::string> hydrate(model_profile_id profile, const model_manifest_entry& entry);
    std::expected<model_handle, std::string> download_and_stage(model_profile_id profile, const model_manifest_entry& entry);
};
} // namespace stemsmith
