#pragma once

#include <string>
#include <thread>
#include <crow/include/crow_all.h>
#include "nlohmann/json.hpp"

namespace stemsmith {

class job_manager;
struct job;

struct server_config
{
    std::string bind_address = "0.0.0.0";
    int port = 8080;
    int http_thread_count = 4;
};

class api_server
{
public:
    api_server(server_config config, std::shared_ptr<job_manager> job_manager);
    ~api_server();

    void run();
    void stop();
    [[nodiscard]] bool is_running() const;

private:
    struct client
    {
        crow::websocket::connection* websocket = nullptr;
        std::set<std::string> subscribed_jobs;
    };

    void wire_callbacks();
    void routes();
    void broadcast_job_update(const nlohmann::json& message, const std::string& job_id);

    server_config config_;
    std::shared_ptr<job_manager> job_manager_;
    crow::SimpleApp app_;

    std::mutex server_mutex_;
    std::set<std::unique_ptr<client>> clients_;

    std::thread server_thread_;
    std::atomic<bool> running_{false};
};

} // namespace stemsmith