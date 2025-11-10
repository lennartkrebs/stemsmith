#pragma once
#include <expected>
#include "nlohmann/json.hpp"

namespace stemsmith::utils
{
std::expected<nlohmann::json, std::string> load_json_file(const std::filesystem::path& path, std::optional<std::string> expected_extension = ".json");
}
