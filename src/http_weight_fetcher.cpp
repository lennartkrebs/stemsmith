#include "http_weight_fetcher.h"

#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace stemsmith
{
namespace
{
struct progress_payload
{
    weight_fetcher::progress_callback callback;
};

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* stream = static_cast<std::ofstream*>(userdata);
    stream->write(ptr, static_cast<std::streamsize>(size * nmemb));
    return size * nmemb;
}

int curl_progress_callback(void* clientp,
                           curl_off_t dltotal,
                           curl_off_t dlnow,
                           curl_off_t /*ultotal*/,
                           curl_off_t /*ulnow*/)
{
    if (const auto* payload = static_cast<progress_payload*>(clientp); payload && payload->callback)
    {
        payload->callback(static_cast<std::size_t>(dlnow), static_cast<std::size_t>(dltotal));
    }
    return 0;
}
} // namespace

http_weight_fetcher::http_weight_fetcher(std::chrono::seconds timeout) : timeout_(timeout)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

http_weight_fetcher::~http_weight_fetcher()
{
    curl_global_cleanup();
}

std::expected<void, std::string> http_weight_fetcher::fetch_weights(std::string_view url,
                                                                    const std::filesystem::path& destination,
                                                                    progress_callback progress)
{
    std::error_code ec;
    std::filesystem::create_directories(destination.parent_path(), ec);
    if (ec)
    {
        return std::unexpected("Failed to create directory: " + ec.message());
    }

    std::ofstream output(destination, std::ios::binary);
    if (!output)
    {
        return std::unexpected("Unable to open destination: " + destination.string());
    }

    CURL* handle = curl_easy_init();
    if (!handle)
    {
        return std::unexpected("Failed to initialize libcurl");
    }

    progress_payload payload{progress};

    curl_easy_setopt(handle, CURLOPT_URL, std::string(url).c_str());
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &output);
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, timeout_.count());
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, timeout_.count());

    if (progress)
    {
        curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION, curl_progress_callback);
        curl_easy_setopt(handle, CURLOPT_XFERINFODATA, &payload);
    }
    else
    {
        curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 1L);
    }

    const auto result = curl_easy_perform(handle);
    curl_easy_cleanup(handle);
    output.close();

    if (result != CURLE_OK)
    {
        return std::unexpected(std::string{"curl error: "} + curl_easy_strerror(result));
    }

    return {};
}

} // namespace stemsmith
