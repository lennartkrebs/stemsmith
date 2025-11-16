#include "stemsmith/job_catalog.h"

#include <system_error>
#include <utility>

namespace
{
bool stem_supported(const std::string& stem, const stemsmith::model_profile& profile)
{
    for (std::size_t i = 0; i < profile.stem_count; ++i)
    {
        if (profile.stems[i] == stem)
        {
            return true;
        }
    }
    return false;
}
} // namespace

namespace stemsmith
{
job_catalog::job_catalog(job_config base_config, exists_function exists_provider)
    : base_config_(std::move(base_config))
{
    if (exists_provider)
    {
        exists_ = std::move(exists_provider);
    }
    else
    {
        exists_ = [](const std::filesystem::path& path)
        {
            std::error_code ec;
            return std::filesystem::exists(path, ec);
        };
    }
}

std::expected<std::size_t, std::string> job_catalog::add_file(const std::filesystem::path& path,
                                                              const job_overrides& overrides)
{
    if (path.empty())
    {
        return std::unexpected("Input path must not be empty");
    }

    const auto normalized = normalize(path);

    if (!exists_(normalized))
    {
        return std::unexpected("Input file does not exist: " + normalized.string());
    }

    if (seen_paths_.contains(normalized))
    {
        return std::unexpected("Input path already enqueued: " + normalized.string());
    }

    auto config_result = apply_overrides(overrides);
    if (!config_result)
    {
        return std::unexpected(config_result.error());
    }

    auto config = std::move(config_result).value();
    jobs_.push_back({normalized, std::move(config)});
    seen_paths_.insert(normalized);
    return jobs_.size() - 1;
}

std::filesystem::path job_catalog::normalize(const std::filesystem::path& path)
{
    if (path.empty())
    {
        return path;
    }
    return path.lexically_normal();
}

std::expected<job_config, std::string> job_catalog::apply_overrides(
    const job_overrides& overrides) const
{
    job_config config = base_config_;

    if (overrides.profile)
    {
        config.profile = *overrides.profile;
    }

    const auto profile = lookup_profile(config.profile);
    if (!profile)
    {
        return std::unexpected("Unknown model profile id");
    }
    const auto& resolved_profile = *profile;

    if (overrides.stems_filter)
    {
        std::vector<std::string> stems = *overrides.stems_filter;
        for (const auto& stem : stems)
        {
            if (!stem_supported(stem, resolved_profile))
            {
                return std::unexpected("Unsupported stem override: " + stem);
            }
        }
        config.stems_filter = std::move(stems);
    }

    return config;
}

void job_catalog::release(const std::filesystem::path& path)
{
    const auto normalized = normalize(path);
    seen_paths_.erase(normalized);
}
} // namespace stemsmith
