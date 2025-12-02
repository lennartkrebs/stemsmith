#include "stemsmith/job_config.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "json_utils.h"

namespace
{
std::expected<std::vector<std::string>, std::string> parse_stems(const nlohmann::json& doc)
{
    if (!doc.contains("stems"))
    {
        return std::vector<std::string>{};
    }

    const auto& value = doc.at("stems");
    if (!value.is_array())
    {
        return std::unexpected("stems must be an array");
    }

    std::vector<std::string> stems;
    stems.reserve(value.size());
    for (const auto& entry : value)
    {
        if (!entry.is_string())
        {
            return std::unexpected("stems entries must be strings");
        }
        stems.push_back(entry.get<std::string>());
    }
    return stems;
}

consteval stemsmith::model_profile make_profile(stemsmith::model_profile_id id,
                                                std::string_view key,
                                                std::string_view label,
                                                std::string_view filename,
                                                std::initializer_list<std::string_view> stems)
{
    std::array<std::string_view, 6> buffer{};
    std::size_t count = 0;
    for (const auto stem : stems)
    {
        buffer[count++] = stem;
    }
    return stemsmith::model_profile{id, key, label, filename, buffer, count};
}

constexpr std::array k_profiles{make_profile(stemsmith::model_profile_id::balanced_four_stem,
                                             "balanced-four-stem",
                                             "Balanced 4-Stem",
                                             "ggml-model-htdemucs-4s-f16.bin",
                                             {"drums", "bass", "other", "vocals"}),
                                make_profile(stemsmith::model_profile_id::balanced_six_stem,
                                             "balanced-six-stem",
                                             "Balanced 6-Stem",
                                             "ggml-model-htdemucs-6s-f16.bin",
                                             {"drums", "bass", "other", "vocals", "piano", "guitar"})};

const stemsmith::model_profile* find_profile(stemsmith::model_profile_id id)
{
    const auto it =
        std::ranges::find_if(k_profiles, [id](const stemsmith::model_profile& profile) { return profile.id == id; });
    return it == k_profiles.end() ? nullptr : &*it;
}

const stemsmith::model_profile* find_profile(std::string_view key)
{
    const auto it =
        std::ranges::find_if(k_profiles, [key](const stemsmith::model_profile& profile) { return profile.key == key; });
    return it == k_profiles.end() ? nullptr : &*it;
}

bool stem_in_profile(std::string_view stem, const stemsmith::model_profile& profile)
{
    return std::find(profile.stems.begin(), profile.stems.begin() + profile.stem_count, stem) !=
           profile.stems.begin() + profile.stem_count;
}
} // namespace

namespace stemsmith
{
std::optional<model_profile> lookup_profile(model_profile_id id)
{
    if (const auto* profile = find_profile(id))
    {
        return *profile;
    }
    return std::nullopt;
}

std::optional<model_profile> lookup_profile(std::string_view key)
{
    if (const auto* profile = find_profile(key))
    {
        return *profile;
    }
    return std::nullopt;
}

std::expected<job_template, std::string> job_template::from_file(const std::filesystem::path& path)
{
    const auto doc_result = utils::load_json_file(path);
    if (!doc_result)
    {
        return std::unexpected(doc_result.error());
    }
    const auto doc = std::move(doc_result).value();

    job_template config;

    auto* active_profile = find_profile(config.profile);
    if (!active_profile)
    {
        return std::unexpected("Default model profile is not registered");
    }

    if (doc.contains("model"))
    {
        if (!doc["model"].is_string())
        {
            return std::unexpected("model must be a string");
        }

        const auto key = doc["model"].get<std::string>();
        if (const auto* override_profile = find_profile(key))
        {
            config.profile = override_profile->id;
            active_profile = override_profile;
        }
        else
        {
            return std::unexpected("Unknown model profile: " + key);
        }
    }

    auto stems_result = parse_stems(doc);
    if (!stems_result)
    {
        return std::unexpected(stems_result.error());
    }

    if (auto stems = std::move(stems_result).value(); !stems.empty())
    {
        for (const auto& stem : stems)
        {
            if (!stem_in_profile(stem, *active_profile))
            {
                return std::unexpected("Unsupported stem: " + stem);
            }
        }
        config.stems_filter = std::move(stems);
    }

    return config;
}

std::vector<std::string> job_template::resolved_stems() const
{
    if (!stems_filter.empty())
    {
        return stems_filter;
    }

    const auto active_profile = lookup_profile(profile);
    if (!active_profile)
    {
        return {};
    }
    std::vector<std::string> result;
    result.reserve(active_profile->stem_count);
    for (std::size_t i = 0; i < active_profile->stem_count; ++i)
    {
        result.emplace_back(active_profile->stems[i]);
    }
    return result;
}
} // namespace stemsmith
