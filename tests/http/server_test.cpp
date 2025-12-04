#include <chrono>
#include <cstddef>
#include <curl/curl.h>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <thread>

#include "http/server.h"

namespace
{

std::uint16_t pick_ephemeral_port()
{
    asio::io_context io;
    const asio::ip::tcp::acceptor acceptor(io, {asio::ip::make_address("127.0.0.1"), 0});
    return acceptor.local_endpoint().port();
}

struct http_response
{
    long status{0};
    std::string body;
};

std::optional<http_response> http_get(const std::string& url)
{
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        return std::nullopt;
    }

    std::string buffer;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(
        curl,
        CURLOPT_WRITEFUNCTION,
        +[](char* ptr, std::size_t size, std::size_t nmemb, void* userdata)
        {
            const auto total = size * nmemb;
            auto* out = static_cast<std::string*>(userdata);
            out->append(ptr, total);
            return total;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 500L);

    if (const auto rc = curl_easy_perform(curl); rc != CURLE_OK)
    {
        curl_easy_cleanup(curl);
        return std::nullopt;
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    return http_response{status, std::move(buffer)};
}

class curl_global_guard
{
public:
    curl_global_guard()
    {
        curl_global_init(CURL_GLOBAL_ALL);
    }
    ~curl_global_guard()
    {
        curl_global_cleanup();
    }
};

} // namespace

TEST(http_server_test, health_endpoint_returns_ok)
{
    curl_global_guard curl_guard;
    const auto port = pick_ephemeral_port();

    stemsmith::http::config cfg;
    cfg.bind_address = "127.0.0.1";
    cfg.port = port;

    stemsmith::http::server server(cfg);
    server.start();

    const auto url = "http://127.0.0.1:" + std::to_string(port) + "/health";

    std::optional<http_response> resp;
    for (int attempt = 0; attempt < 10 && !resp; ++attempt)
    {
        resp = http_get(url);
        if (!resp)
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    server.stop();

    ASSERT_TRUE(resp.has_value()) << "Server did not respond to /health";
    EXPECT_EQ(resp->status, 200);
    EXPECT_NE(resp->body.find("\"status\":\"ok\""), std::string::npos);

    const auto resp_after_stop = http_get(url);
    EXPECT_FALSE(resp_after_stop.has_value()) << "Server responded to /health after stop";
}

TEST(http_server_test, root_endpoint_returns_message)
{
    curl_global_guard curl_guard;
    const auto port = pick_ephemeral_port();

    stemsmith::http::config cfg;
    cfg.bind_address = "127.0.0.1";
    cfg.port = port;

    stemsmith::http::server server(cfg);
    server.start();

    const auto url = "http://127.0.0.1:" + std::to_string(port) + "/";

    std::optional<http_response> resp;
    for (int attempt = 0; attempt < 10 && !resp; ++attempt)
    {
        resp = http_get(url);
        if (!resp)
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    server.stop();

    ASSERT_TRUE(resp.has_value()) << "Server did not respond to /";
    EXPECT_EQ(resp->status, 200);
    EXPECT_NE(resp->body.find("\"message\":\"Welcome to the StemSmith Job Server\""), std::string::npos);
}
