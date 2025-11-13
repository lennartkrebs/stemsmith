#include "stemsmith/model_manifest.h"

#include "stemsmith/job_config.h"
#include "stemsmith/json_utils.h"

#include <expected>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace
{
std::string expand_template(std::string_view tpl, std::string_view placeholder, std::string_view value)
{
    std::string result(tpl);
    if (const auto pos = result.find(placeholder); pos != std::string::npos)
    {
        result.replace(pos, placeholder.size(), value);
    }
    return result;
}
} // namespace

namespace stemsmith
{
model_manifest::model_manifest(std::vector<model_manifest_entry> entries)
{
    for (auto& entry : entries)
    {
        entries_.insert_or_assign(entry.profile, std::move(entry));
    }
}

std::expected<model_manifest, std::string>
model_manifest::load_default()
{
#ifndef STEMSMITH_DATA_DIR
#error "STEMSMITH_DATA_DIR is not defined"
#endif
    const std::filesystem::path manifest_path = std::filesystem::path{STEMSMITH_DATA_DIR} / "model_manifest.json";
    return from_file(manifest_path);
}

std::expected<model_manifest, std::string> model_manifest::from_file(const std::filesystem::path& path)
{
    const auto doc = utils::load_json_file(path);
    if (!doc)
    {
        return std::unexpected(doc.error());
    }

    if (!doc->contains("models") || !(*doc)["models"].is_array())
    {
        return std::unexpected("Manifest is missing the models array");
    }

    std::string url_template;
    if (doc->contains("source") && (*doc)["source"].contains("url_template"))
    {
        url_template = (*doc)["source"]["url_template"].get<std::string>();
    }

    std::vector<model_manifest_entry> entries;
    for (const auto& item : (*doc)["models"])
    {
        if (!item.contains("profile") || !item.contains("filename") || !item.contains("sha256"))
        {
            return std::unexpected("Manifest entry missing required fields");
        }

        const auto profile_key = item["profile"].get<std::string>();
        const auto filename = item["filename"].get<std::string>();
        const auto sha = item["sha256"].get<std::string>();
        const auto size = item.value("size_bytes", 0ULL);

        const auto profile = lookup_profile(profile_key);
        if (!profile)
        {
            return std::unexpected("Unknown profile in manifest: " + profile_key);
        }
        std::string url;
        if (item.contains("url"))
        {
            url = item["url"].get<std::string>();
        }
        else if (!url_template.empty())
        {
            url = expand_template(url_template, "{filename}", filename);
        }
        else
        {
            return std::unexpected("No URL specified for manifest entry: " + profile_key);
        }

        entries.push_back(model_manifest_entry{profile->id, profile_key, filename, url, size, sha});
    }

    return model_manifest{std::move(entries)};
}

const model_manifest_entry* model_manifest::find(model_profile_id profile) const
{
    const auto it = entries_.find(profile);
    return it == entries_.end() ? nullptr : &it->second;
}
} // namespace stemsmith
