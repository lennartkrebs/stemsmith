#pragma once

#include <atomic>
// clang-format off
#include <exception>
#include <functional>
#include <crow/include/crow_all.h>
// clang-format on
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

#include "stemsmith/job_result.h"
#include "stemsmith/service.h"

namespace stemsmith::http
{

struct config
{
    std::string bind_address{"0.0.0.0"};
    std::uint16_t port{8345};
    std::filesystem::path cache_root{};
    std::filesystem::path output_root{};
    std::optional<size_t> worker_count{std::nullopt};
};

struct job_state
{
    job_handle handle;
    job_event last_event{};
    std::filesystem::path output_dir{};
    std::filesystem::path upload_path{};
};

class job_registry
{
public:
    [[nodiscard]] std::string next_id();
    void add(const std::string& id, job_handle handle, std::filesystem::path upload_path);
    void update(const std::string& id, const job_descriptor& desc, const job_event& ev);

    [[nodiscard]] std::optional<job_state> get(const std::string& id) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, job_state> jobs_;
    std::atomic<std::uint64_t> next_id_{1};
};

/**
 * @brief HTTP server for StemSmith job submission and status querying.
 */
class server
{
public:
    explicit server(config cfg);
    ~server();

    server(const server&) = delete;
    server& operator=(const server&) = delete;
    server(server&&) = delete;
    server& operator=(server&&) = delete;

    void start();
    void stop();

private:
    void run();
    void register_routes();
    crow::response handle_post_job(const crow::request& req);
    crow::response handle_get_job(const std::string& id) const;
    crow::response handle_delete_job(const std::string& id);
    crow::response handle_download(const std::string& id) const;

    config config_{};
    std::unique_ptr<service> svc_;
    job_registry registry_;
    crow::App<crow::CORSHandler> app_;
    std::thread thread_;
    std::atomic<bool> running_{false};

    // For ease of testing
    std::function<std::expected<job_handle, std::string>(job_request)> submit_override_{};
    friend class server_test_hook;
};

} // namespace stemsmith::http
