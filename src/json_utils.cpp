#include "json_utils.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace stemsmith::utils
{
std::expected<nlohmann::json, std::string> load_json_file(const std::filesystem::path& path,
                                                          std::optional<std::string> expected_extension)
{
    if (expected_extension.has_value() && path.extension() != expected_extension.value())
    {
        return std::unexpected("Unexpected file extension: " + path.string());
    }

    std::ifstream input(path);
    if (!input)
    {
        return std::unexpected("Unable to open JSON file: " + path.string());
    }

    try
    {
        nlohmann::json doc;
        input >> doc;
        return doc;
    }
    catch (const std::exception& err)
    {
        return std::unexpected(std::string{"Failed to parse JSON file: "} + err.what());
    }
}
} // namespace stemsmith::utils