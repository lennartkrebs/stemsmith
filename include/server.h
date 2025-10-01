#pragma once

#include <string>
#include <thread>
#include <crow/include/crow_all.h>
#include "nlohmann/json.hpp"

namespace stemsmith {

class job_queue;
struct job;

struct server_config
{
    std::string bind_address = "0.0.0.0";
    int port = 8080;
    int http_thread_count = 4;
};

class server
{
public:
    server(server_config config, std::shared_ptr<job_queue> job_queue);
    ~server();

    void run();
    void stop();

private:
    struct client
    {
        crow::websocket::connection* websocket = nullptr;
        std::set<std::string> subscribed_jobs;
    };

    static std::string make_job_id() ;

    void wire_callbacks();
    void routes();
    void broadcast_job_update(const nlohmann::json& message, const std::string& job_id);

    server_config config_;
    std::shared_ptr<job_queue> job_queue_;
    crow::SimpleApp app_;

    std::mutex server_mutex_;
    std::map<std::string, std::shared_ptr<job>> jobs_;
    std::set<std::unique_ptr<client>> clients_;

    std::thread job_broadcast_thread_;
};

} // namespace stemsmith