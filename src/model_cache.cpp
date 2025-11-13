#include "stemsmith/model_cache.h"

#include "stemsmith/model_manifest.h"
#include "stemsmith/weight_fetcher.h"

#include <expected>
#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

#include "picosha2.h"

namespace
{
using stemsmith::model_manifest_entry;

std::filesystem::path model_path(const std::filesystem::path& root, const model_manifest_entry& entry)
{
    return root / entry.profile_key / entry.filename;
}

std::expected<bool, std::string> file_ready(const std::filesystem::path& path, const model_manifest_entry& entry)
{
    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
    {
        return false;
    }
    if (ec)
    {
        return std::unexpected("Failed to inspect model file: " + ec.message());
    }

    if (entry.size_bytes > 0)
    {
        const auto size = std::filesystem::file_size(path, ec);
        if (ec)
        {
            return std::unexpected("Failed to read model file size: " + ec.message());
        }
        if (size != entry.size_bytes)
        {
            return false;
        }
    }

    auto checksum = stemsmith::model_cache::verify_checksum(path, entry);
    if (!checksum)
    {
        return std::unexpected(checksum.error());
    }
    if (!checksum.value())
    {
        std::filesystem::remove(path, ec);
        return false;
    }
    return true;
}
} // namespace

namespace stemsmith
{
std::expected<model_cache, std::string> model_cache::create(std::filesystem::path cache_root, std::shared_ptr<weight_fetcher> fetcher)
{
    if (!fetcher)
    {
        return std::unexpected("weight_fetcher is null");
    }

    auto manifest = model_manifest::load_default();
    if (!manifest)
    {
        return std::unexpected(manifest.error());
    }
    return model_cache{std::move(cache_root), std::move(fetcher), std::move(manifest.value())};
}

model_cache::model_cache(std::filesystem::path cache_root, std::shared_ptr<weight_fetcher> fetcher, model_manifest manifest)
    : cache_root_(std::move(cache_root))
    , fetcher_(std::move(fetcher))
    , manifest_(std::move(manifest))
{}

std::expected<model_handle, std::string> model_cache::ensure_ready(model_profile_id profile)
{
    if (!fetcher_)
    {
        return std::unexpected("weight_fetcher is null");
    }

    const auto* entry = manifest_.find(profile);
    if (!entry)
    {
        return std::unexpected("Profile missing from manifest");
    }

    auto result = hydrate(profile, *entry);
    return result;
}

std::expected<void, std::string> model_cache::purge(model_profile_id profile) const
{
    const auto* entry = manifest_.find(profile);
    if (!entry)
    {
        return std::unexpected("Profile missing from manifest");
    }

    std::error_code ec;
    std::filesystem::remove_all(cache_root_ / entry->profile_key, ec);
    if (ec)
    {
        return std::unexpected("Failed to purge cache entry: " + ec.message());
    }
    return {};
}

std::expected<void, std::string> model_cache::purge_all() const
{
    std::error_code ec;
    std::filesystem::remove_all(cache_root_, ec);
    if (ec)
    {
        return std::unexpected("Failed to purge cache root: " + ec.message());
    }
    return {};
}

model_cache::profile_state& model_cache::state_for(model_profile_id profile)
{
    auto it = profile_states_.find(profile);
    if (it == profile_states_.end())
    {
        auto state = std::make_unique<profile_state>();
        it = profile_states_.emplace(profile, std::move(state)).first;
    }
    return *it->second;
}

std::expected<model_handle, std::string> model_cache::hydrate(model_profile_id profile, const model_manifest_entry& entry)
{
    const auto path = model_path(cache_root_, entry);

    auto ready = file_ready(path, entry);
    if (!ready)
    {
        return std::unexpected(ready.error());
    }
    if (ready.value())
    {
        return model_handle{profile, path, entry.sha256, entry.size_bytes, true};
    }

    auto& [mutex] = state_for(profile);
    std::unique_lock lock(mutex);

    ready = file_ready(path, entry);
    if (!ready)
    {
        return std::unexpected(ready.error());
    }
    if (ready.value())
    {
        return model_handle{profile, path, entry.sha256, entry.size_bytes, true};
    }

    return download_and_stage(profile, entry);
}

std::expected<model_handle, std::string> model_cache::download_and_stage(model_profile_id profile, const model_manifest_entry& entry) const
{
    const auto target_path = model_path(cache_root_, entry);
    std::filesystem::path staging = target_path;
    staging += ".tmp";

    std::error_code ec;
    std::filesystem::create_directories(target_path.parent_path(), ec);
    if (ec)
    {
        return std::unexpected("Failed to create cache directories: " + ec.message());
    }

    std::filesystem::remove(staging, ec); // ignore failure

    if (const auto fetch = fetcher_->fetch_weights(entry.url, staging); !fetch)
    {
        std::filesystem::remove(staging, ec);
        return std::unexpected(fetch.error());
    }

    if (entry.size_bytes > 0)
    {
        const auto downloaded_size = std::filesystem::file_size(staging, ec);
        if (ec)
        {
            std::filesystem::remove(staging, ec);
            return std::unexpected("Failed to inspect downloaded weights: " + ec.message());
        }
        if (downloaded_size != entry.size_bytes)
        {
            std::filesystem::remove(staging, ec);
            return std::unexpected("Downloaded weights size mismatch");
        }
    }

    const auto ready = verify_checksum(staging, entry);
    if (!ready)
    {
        std::filesystem::remove(staging, ec);
        return std::unexpected(ready.error());
    }
    if (!ready.value())
    {
        std::filesystem::remove(staging, ec);
        return std::unexpected("Checksum mismatch for downloaded weights");
    }

    std::filesystem::remove(target_path, ec);
    if (ec && ec != std::make_error_code(std::errc::no_such_file_or_directory))
    {
        std::filesystem::remove(staging, ec);
        return std::unexpected("Failed to replace existing weights: " + ec.message());
    }

    ec.clear();
    std::filesystem::rename(staging, target_path, ec);
    if (ec)
    {
        std::filesystem::remove(staging, ec);
        return std::unexpected("Failed to finalize cached weights: " + ec.message());
    }

    return model_handle{profile, target_path, entry.sha256, entry.size_bytes, false};
}

std::expected<bool, std::string> model_cache::verify_checksum(const std::filesystem::path& path, const model_manifest_entry& entry)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return std::unexpected("Unable to open weights for checksum: " + path.string());
    }

    const std::vector<unsigned char> buffer(std::istreambuf_iterator(input), {});
    const auto hash = picosha2::hash256_hex_string(buffer);
    return hash == entry.sha256;
}
} // namespace stemsmith
