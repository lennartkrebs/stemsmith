/**
 * @file weight_fetcher.h
 * @brief Interface for supplying custom model-weight downloaders.
 *
 * Implement ::stemsmith::weight_fetcher if you need to override the default
 * HTTP client (e.g., to integrate with a custom cache or offline storage).
 */
#pragma once
#include <expected>
#include <filesystem>
#include <functional>
#include <string_view>

namespace stemsmith
{

/**
 * @brief Interface for fetching model weights from a remote source.
 * Per default, weights are expected to be downloaded via HTTP(S). But
 * you may implement custom fetchers to retrieve weights from other sources,
 * e.g. local network shares, cloud storage, etc.
 */
struct weight_fetcher
{
    using progress_callback = std::function<void(std::size_t bytes_downloaded, std::size_t total_bytes)>;
    virtual ~weight_fetcher() = default;
    virtual std::expected<void, std::string> fetch_weights(std::string_view url,
                                                           const std::filesystem::path& destination,
                                                           progress_callback progress) = 0;
};

} // namespace stemsmith
