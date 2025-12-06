// clang-format off
#include <exception>
#include <asio.hpp>
// clang-format on
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

namespace stemsmith::http
{
class server_test_hook
{
public:
    static crow::response post_job(server& srv, const crow::request& req)
    {
        return srv.handle_post_job(req);
    }

    static void set_submit_override(server& srv,
                                    std::function<std::expected<job_handle, std::string>(job_request)> func)
    {
        srv.submit_override_ = std::move(func);
    }
};
} // namespace stemsmith::http

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

TEST(http_server_test, post_jobs_rejects_missing_file)
{
    stemsmith::http::config cfg;
    stemsmith::http::server srv(cfg);
    stemsmith::http::server_test_hook::set_submit_override(srv,
                                                           [](stemsmith::job_request)
                                                           { return std::unexpected<std::string>(""); });

    const std::string body = "--BOUNDARY--\r\n";
    crow::request req;
    req.body = body;
    req.add_header("Content-Type", "multipart/form-data; boundary=BOUNDARY");

    const auto resp = stemsmith::http::server_test_hook::post_job(srv, req);
    EXPECT_EQ(resp.code, crow::status::BAD_REQUEST);
    EXPECT_NE(resp.body.find(R"({"error":"file field required"})"), std::string::npos);
}

TEST(http_server_test, post_jobs_rejects_non_wav)
{
    stemsmith::http::config cfg;
    stemsmith::http::server srv(cfg);
    stemsmith::http::server_test_hook::set_submit_override(srv,
                                                           [](stemsmith::job_request)
                                                           { return std::unexpected<std::string>(""); });

    const std::string part_body = "abc";
    std::string body;
    body += "--BOUNDARY\r\n";
    body += "Content-Disposition: form-data; name=\"file\"; filename=\"file.mp3\"\r\n";
    body += "Content-Type: audio/mpeg\r\n\r\n";
    body += part_body;
    body += "\r\n--BOUNDARY--\r\n";

    crow::request req;
    req.body = body;
    req.add_header("Content-Type", "multipart/form-data; boundary=BOUNDARY");

    const auto resp = stemsmith::http::server_test_hook::post_job(srv, req);
    EXPECT_EQ(resp.code, crow::status::BAD_REQUEST);
    EXPECT_NE(resp.body.find(R"({"error":"WAV input required"})"), std::string::npos);
}

TEST(http_server_test, post_jobs_rejects_bad_config_json)
{
    stemsmith::http::config cfg;
    stemsmith::http::server srv(cfg);
    stemsmith::http::server_test_hook::set_submit_override(srv,
                                                           [](stemsmith::job_request)
                                                           { return stemsmith::job_handle{}; });

    const std::string file_body = "RIFF....WAVE"; // minimal marker content
    std::string body;
    body += "--BOUNDARY\r\n";
    body += "Content-Disposition: form-data; name=\"file\"; filename=\"file.wav\"\r\n";
    body += "Content-Type: audio/wav\r\n\r\n";
    body += file_body;
    body += "\r\n";
    body += "--BOUNDARY\r\n";
    body += "Content-Disposition: form-data; name=\"config\"; filename=\"config.json\"\r\n";
    body += "Content-Type: application/json\r\n\r\n";
    body += "not-json";
    body += "\r\n--BOUNDARY--\r\n";

    crow::request req;
    req.body = body;
    req.add_header("Content-Type", "multipart/form-data; boundary=BOUNDARY");

    const auto resp = stemsmith::http::server_test_hook::post_job(srv, req);
    EXPECT_EQ(resp.code, crow::status::BAD_REQUEST);
}

TEST(http_server_test, post_jobs_rejects_large_file)
{
    stemsmith::http::config cfg;
    stemsmith::http::server srv(cfg);
    stemsmith::http::server_test_hook::set_submit_override(srv,
                                                           [](stemsmith::job_request)
                                                           { return stemsmith::job_handle{}; });

    constexpr std::size_t too_big = 100 * 1024 * 1024 + 1;
    std::string file_body(too_big, 'x');

    std::string body;
    body.reserve(file_body.size() + 256);
    body += "--BOUNDARY\r\n";
    body += "Content-Disposition: form-data; name=\"file\"; filename=\"file.wav\"\r\n";
    body += "Content-Type: audio/wav\r\n\r\n";
    body += file_body;
    body += "\r\n--BOUNDARY--\r\n";

    crow::request req;
    req.body = std::move(body);
    req.add_header("Content-Type", "multipart/form-data; boundary=BOUNDARY");

    const auto resp = stemsmith::http::server_test_hook::post_job(srv, req);
    EXPECT_EQ(resp.code, crow::status::PAYLOAD_TOO_LARGE);
}

TEST(http_server_test, post_jobs_accepts_valid_wav_and_config)
{
    stemsmith::http::config cfg;
    stemsmith::http::server srv(cfg);

    bool submit_called = false;
    stemsmith::http::server_test_hook::set_submit_override(
        srv,
        [&](const stemsmith::job_request& req) -> std::expected<stemsmith::job_handle, std::string>
        {
            submit_called = true;
            EXPECT_TRUE(req.profile.has_value());
            EXPECT_TRUE(req.stems.has_value());
            EXPECT_EQ(req.stems->size(), 1u);
            EXPECT_EQ(req.stems->front(), "vocals");
            return stemsmith::job_handle{};
        });

    const std::string file_body = "RIFF....WAVE";
    const std::string config_json = R"({"model":"balanced-six-stem","stems":["vocals"]})";

    std::string body;
    body += "--BOUNDARY\r\n";
    body += "Content-Disposition: form-data; name=\"file\"; filename=\"file.wav\"\r\n";
    body += "Content-Type: audio/wav\r\n\r\n";
    body += file_body;
    body += "\r\n";
    body += "--BOUNDARY\r\n";
    body += "Content-Disposition: form-data; name=\"config\"; filename=\"config.json\"\r\n";
    body += "Content-Type: application/json\r\n\r\n";
    body += config_json;
    body += "\r\n--BOUNDARY--\r\n";

    crow::request req;
    req.body = body;
    req.add_header("Content-Type", "multipart/form-data; boundary=BOUNDARY");

    const auto resp = stemsmith::http::server_test_hook::post_job(srv, req);
    EXPECT_EQ(resp.code, crow::status::ACCEPTED);
    EXPECT_TRUE(submit_called);
}

TEST(http_server_test, service_unavailable_when_no_service)
{
    stemsmith::http::config cfg;
    stemsmith::http::server srv(cfg);

    const std::string part_body = "RIFF....WAVEfmt "; // partial WAV header
    std::string body;
    body += "--BOUNDARY\r\n";
    body += "Content-Disposition: form-data; name=\"file\"; filename=\"file.wav\"\r\n";
    body += "Content-Type: audio/wav\r\n\r\n";
    body += part_body;
    body += "\r\n--BOUNDARY--\r\n";

    crow::request req;
    req.body = body;
    req.add_header("Content-Type", "multipart/form-data; boundary=BOUNDARY");

    const auto resp = stemsmith::http::server_test_hook::post_job(srv, req);
    EXPECT_EQ(resp.code, crow::status::SERVICE_UNAVAILABLE);
}
