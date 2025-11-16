#pragma once
#include <expected>
#include <filesystem>
#include <functional>
#include <string_view>

namespace stemsmith
{

struct weight_fetcher
{
    using progress_callback = std::function<void(std::size_t bytes_downloaded, std::size_t total_bytes)>;
    virtual ~weight_fetcher() = default;
    virtual std::expected<void, std::string> fetch_weights(std::string_view url,
                                                           const std::filesystem::path& destination,
                                                           progress_callback progress = {}) = 0;
};

} // namespace stemsmith
