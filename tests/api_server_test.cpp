#include <gtest/gtest.h>

#include "api_server.h"
#include "job_manager.h"
#include "job_builder.h"

#include <nlohmann/json.hpp>
#include <asio.hpp>
#include <filesystem>
#include <fstream>
#include <random>
#include <thread>

using namespace stemsmith;
using nlohmann::json;

namespace {
std::string make_temp_input(int idx) {
    namespace fs = std::filesystem;
    const auto file = fs::temp_directory_path() / ("server_input_" + std::to_string(idx) + ".wav");
    std::ofstream ofs(file.string());
    ofs << "dummy wav data";
    return file.string();
}

std::string make_temp_output_dir(int idx) {
    namespace fs = std::filesystem;
    const auto dir = fs::temp_directory_path() / ("server_out_" + std::to_string(idx));
    fs::create_directories(dir);
    return dir.string();
}

struct http_response {
    int status = 0;
    std::string body;
};

http_response http_request(int port, const std::string& method, const std::string& path, const std::string& body = "", const std::string& content_type = "application/json") {
    using asio::ip::tcp;
    asio::io_context io;
    tcp::resolver resolver(io);
    http_response result;

    std::string port_str = std::to_string(port);
    auto endpoints = resolver.resolve("127.0.0.1", port_str);
    tcp::socket socket(io);
    asio::connect(socket, endpoints);

    std::ostringstream req;
    req << method << " " << path << " HTTP/1.1\r\n";
    req << "Host: 127.0.0.1:" << port_str << "\r\n";
    req << "User-Agent: stemsmith-test\r\n";
    if (!body.empty()) {
        req << "Content-Type: " << content_type << "\r\n";
        req << "Content-Length: " << body.size() << "\r\n";
    }
    req << "Connection: close\r\n\r\n";
    if (!body.empty()) req << body;

    auto req_str = req.str();
    asio::write(socket, asio::buffer(req_str));

    asio::streambuf response_buf;
    asio::error_code ec;
    asio::read(socket, response_buf, ec); // expect EOF

    std::istream is(&response_buf);
    std::string status_line;
    std::getline(is, status_line); // e.g. HTTP/1.1 200 OK
    if (!status_line.empty()) {
        std::istringstream sl(status_line);
        std::string http_version; sl >> http_version; sl >> result.status; // ignore reason phrase
    }

    // Read headers until blank line
    std::string header_line;
    while (std::getline(is, header_line)) {
        if (header_line == "\r" || header_line.empty()) break;
    }

    // Remaining is body
    std::ostringstream body_stream;
    body_stream << is.rdbuf();
    result.body = body_stream.str();
    return result;
}

bool wait_for_health(int port, std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < timeout) {
        try {
            auto resp = http_request(port, "GET", "/health");
            if (resp.status == 200) return true;
        } catch (...) {}
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

json get_job_json(int port, const std::string& id) {
    auto resp = http_request(port, "GET", "/api/jobs/" + id);
    EXPECT_EQ(resp.status, 200) << "Failed to fetch job detail for " << id;
    return json::parse(resp.body);
}
}

TEST(api_server, start_server_and_submit_jobs) {
    int base_port = 20000 + static_cast<int>(std::chrono::steady_clock::now().time_since_epoch().count() % 10000);

    auto mgr = std::make_shared<job_manager>(8);
    server_config cfg;
    cfg.bind_address = "127.0.0.1";
    cfg.port = base_port;
    cfg.http_thread_count = 4;
    api_server server(cfg, mgr);
    server.run();

    ASSERT_TRUE(wait_for_health(base_port)) << "Server did not become healthy";

    // Submit multiple jobs via HTTP
    const int job_count = 4; // keep runtime low
    std::vector<std::string> job_ids;

    const auto id = mgr->subscribe([](const std::shared_ptr<job>& job)
    {
        if (!job) return;
        std::cout << "[JOB UPDATE] ID: " << job->id << " Status: " << job->status_string() << " Progress: " << job->progress.load() << " on thread: " << std::this_thread::get_id() << std::endl;
    });

    for (int i = 0; i < job_count; ++i) {
        json payload = {
            {"input_path", make_temp_input(i)},
            {"output_path", make_temp_output_dir(i)},
            {"model_name", "htdemucs"},
            {"mode", i % 2 == 0 ? "fast" : "hq"}
        };
        auto resp = http_request(base_port, "POST", "/api/jobs", payload.dump());
        ASSERT_EQ(resp.status, 200) << "Job submission failed: " << resp.body;
        auto body = json::parse(resp.body);
        ASSERT_TRUE(body.contains("job_id"));
        ASSERT_TRUE(body.contains("status"));
        EXPECT_EQ(body["status"], "queued");
        job_ids.push_back(body["job_id"].get<std::string>());
    }

    //std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    //mgr->unsubscribe(id);

    // Poll until all jobs completed
    auto start = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(10);
    std::set<std::string> completed;

    while (std::chrono::steady_clock::now() - start < timeout && completed.size() < job_ids.size()) {
        for (auto &id : job_ids) {
            if (completed.contains(id)) continue;
            auto job_json = get_job_json(base_port, id);
            auto status = job_json.value("status", "");
            if (status == "completed") {
                completed.insert(id);
                // Basic JSON mapping checks
                EXPECT_TRUE(job_json.contains("progress"));
                EXPECT_TRUE(job_json.contains("input_path"));
                EXPECT_TRUE(job_json.contains("output_path"));
                EXPECT_TRUE(job_json.contains("model_name"));
                EXPECT_TRUE(job_json.contains("mode"));
                EXPECT_TRUE(job_json.contains("stems"));
                EXPECT_EQ(job_json["stems"].size(), 4);
                EXPECT_FLOAT_EQ(job_json["progress"].get<float>(), 1.0f);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    EXPECT_EQ(completed.size(), job_ids.size()) << "Not all jobs completed in time";
    server.stop();
}

