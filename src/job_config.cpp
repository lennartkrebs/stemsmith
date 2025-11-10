#include "stemsmith/job_config.h"

#include "stemsmith/json_utils.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace
{
std::string expand_env(const std::string& input)
{
    std::string output;
    output.reserve(input.size());

    for (std::size_t i = 0; i < input.size();)
    {
        if (input[i] != '$')
        {
            output.push_back(input[i]);
            ++i;
            continue;
        }

        if (i + 1 >= input.size())
        {
            output.push_back('$');
            break;
        }

        std::string key;
        std::size_t consumed = 0;

        if (input[i + 1] == '{')
        {
            const auto end = input.find('}', i + 2);
            if (end == std::string::npos)
            {
                output.push_back('$');
                ++i;
                continue;
            }
            key = input.substr(i + 2, end - (i + 2));
            consumed = end - i + 1;
        }
        else
        {
            std::size_t j = i + 1;
            while (j < input.size() &&
                   (std::isalnum(static_cast<unsigned char>(input[j])) ||
                    input[j] == '_' || input[j] == '-'))
            {
                ++j;
            }
            key = input.substr(i + 1, j - (i + 1));
            consumed = j - i;
        }

        if (const auto* value = std::getenv(key.c_str()); value != nullptr)
        {
            output.append(value);
        }
        else
        {
            output.append("$");
            output.append(key);
        }

        i += consumed;
    }

    return output;
}

std::expected<std::vector<std::string>, std::string>
parse_stems(const nlohmann::json& doc)
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

consteval stemsmith::model_profile make_profile(
    stemsmith::model_profile_id id,
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

constexpr std::array k_profiles{
    make_profile(stemsmith::model_profile_id::balanced_four_stem,
                 "balanced-four-stem",
                 "Balanced 4-Stem",
                 "ggml-model-htdemucs-4s-f16.bin",
                 {"vocals", "drums", "bass", "other"}),
    make_profile(stemsmith::model_profile_id::balanced_six_stem,
                 "balanced-six-stem",
                 "Balanced 6-Stem",
                 "ggml-model-htdemucs-6s-f16.bin",
                 {"vocals", "drums", "bass", "piano", "guitar",
                  "other"})};

const stemsmith::model_profile* find_profile(stemsmith::model_profile_id id)
{
    const auto it = std::ranges::find_if(k_profiles,
                                   [id](const stemsmith::model_profile& profile) {
                                       return profile.id == id;
                                   });
    return it == k_profiles.end() ? nullptr : &*it;
}

const stemsmith::model_profile* find_profile(std::string_view key)
{
    const auto it = std::ranges::find_if(k_profiles,
                           [key](const stemsmith::model_profile& profile) {
                               return profile.key == key;
                           });
    return it == k_profiles.end() ? nullptr : &*it;
}

bool stem_in_profile(std::string_view stem,
                     const stemsmith::model_profile& profile)
{
    return std::find(profile.stems.begin(),
                     profile.stems.begin() + profile.stem_count, stem) !=
           profile.stems.begin() + profile.stem_count;
}
} // namespace

namespace stemsmith
{
const model_profile& lookup_profile(model_profile_id id)
{
    if (const auto* profile = find_profile(id))
    {
        return *profile;
    }
    throw std::runtime_error("Unknown model profile id");
}

const model_profile& lookup_profile(std::string_view key)
{
    if (const auto* profile = find_profile(key))
    {
        return *profile;
    }
    throw std::runtime_error("Unknown model profile: " + std::string{key});
}

std::expected<job_config, std::string> job_config::from_file(const std::filesystem::path& path)
{
    const auto doc_result = utils::load_json_file(path);
    if (!doc_result)
    {
        return std::unexpected(doc_result.error());
    }
    const auto doc = std::move(doc_result).value();

    job_config config;

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

    if (doc.contains("cache_root"))
    {
        if (!doc["cache_root"].is_string())
        {
            return std::unexpected("cache_root must be a string");
        }

        const auto expanded = expand_env(doc["cache_root"].get<std::string>());
        config.cache_root = std::filesystem::path{expanded};
    }

    return config;
}

std::vector<std::string> job_config::resolved_stems() const
{
    if (!stems_filter.empty())
    {
        return stems_filter;
    }

    const auto& active_profile = lookup_profile(profile);
    std::vector<std::string> result;
    result.reserve(active_profile.stem_count);
    for (std::size_t i = 0; i < active_profile.stem_count; ++i)
    {
        result.emplace_back(active_profile.stems[i]);
    }
    return result;
}
} // namespace stemsmith
