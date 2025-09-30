#pragma once

#include <string>
#include <crow/include/crow_all.h>
#include "job_queue.h"

namespace stemsmith {

class job_queue;

struct server_config
{
    std::string bind_address = "0.0.0.0";
    int port = 8080;
    int http_thread_count = 4;
};

class server
{
public:
    server(const server_config& config, std::shared_ptr<job_queue> job_queue);

private:
    struct client
    {
        crow::websocket::connection* websocket = nullptr;
        std::set<std::string> subscribed_jobs;
    };

    
};

} // namespace stemsmith