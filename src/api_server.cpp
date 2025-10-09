#include "api_server.h"
#include "job_queue.h"
#include <utility>
#include <memory>
#include <algorithm>

#include "job_manager.h"
#include "job_builder.h"

namespace stemsmith {

api_server::api_server(server_config config, std::shared_ptr<job_manager> job_manager)
    : config_(std::move(config))
    , job_manager_(std::move(job_manager))
{
    // Let's get rid of the info spam for now
    app_.loglevel(crow::LogLevel::Warning);

    wire_callbacks();
    routes();
}

api_server::~api_server()
{
    stop();
}

void api_server::run()
{
    server_thread_ = std::thread([this]()
    {
        app_.bindaddr(config_.bind_address).port(config_.port).concurrency(config_.http_thread_count).run();
    });
}

void api_server::stop()
{
    app_.stop();

    if (server_thread_.joinable())
    {
        server_thread_.join();
    }
}

bool api_server::is_running() const
{
    return server_thread_.joinable();
}

void api_server::wire_callbacks()
{
}

void api_server::routes()
{
    CROW_ROUTE(app_, "/")([]() {
        return "Stemsmith Live Server is running.";
    });

    CROW_ROUTE(app_, "/health").methods(crow::HTTPMethod::Get)([]
    {
        return crow::response(200, "OK");
    });

    // POST /api/jobs - Create a new job
    CROW_ROUTE(app_, "/api/jobs").methods(crow::HTTPMethod::Post)([this](const crow::request& req) {
        using nlohmann::json;
        try
        {
            const auto body = json::parse(req.body);
            job_builder builder;
            const auto parameters = builder
                .with_input(body.value("input_path", ""))
                .with_output(body.value("output_path", ""))
                .with_model(body.value("model", "htdemucs"))
                .with_mode(body.value("mode", "fast"))
                .build();

            if (parameters.input_path.empty() || parameters.output_path.empty())
            {
                return crow::response(400, json({{"error", "Missing required fields"}}).dump());
            }

            std::string job_id;
            try {
                job_id = job_manager_->submit_job(parameters);
            } catch (std::exception& e) {
                return crow::response(500, json({{"error", e.what()}}).dump());
            }
            auto job = job_manager_->get_job(job_id);
            return crow::response(200, json({
                {"job_id", job->id},
                {"status", job->status_string()}
            }).dump());
        }
        catch (json::parse_error&)
        {
            return crow::response(400, json({{"error", "Invalid JSON"}}).dump());
        }
    });

    // GET /api/jobs - List all jobs
    CROW_ROUTE(app_, "/api/jobs").methods(crow::HTTPMethod::Get)([this]() {
        using nlohmann::json;
        json jobs_array = json::array();

        for (const auto& job : job_manager_->list_jobs())
        {
            jobs_array.push_back({
                {"id", job->id},
                {"status", job->status_string()},
                {"progress", job->progress.load()},
                {"input_path", job->input_path},
                {"output_path", job->output_path}
            });
        }

        return crow::response(200, json({{"jobs", jobs_array}}).dump());
    });

    // GET /api/jobs/<string> - Get specific job status
    CROW_ROUTE(app_, "/api/jobs/<string>").methods(crow::HTTPMethod::Get)([this](const std::string& job_id) {
        using nlohmann::json;

        auto job = job_manager_->get_job(job_id);
        if (!job)
        {
            return crow::response(404, json({{"error", "Job not found"}}).dump());
        }

        json response = {
            {"id", job->id},
            {"status", job->status_string()},
            {"progress", job->progress.load()},
            {"input_path", job->input_path},
            {"output_path", job->output_path},
            {"model_name", job->model_name},
            {"mode", job->mode}
        };

        if (!job->error_message.empty())
        {
            response["error"] = job->error_message;
        }

        if (!job->stems.empty())
        {
            response["stems"] = job->stems;
        }

        return crow::response(200, response.dump());
    });

    // WebSocket endpoint
    CROW_WEBSOCKET_ROUTE(app_, "/ws")
        .onopen([this](crow::websocket::connection& conn) {
            auto new_client = std::make_unique<client>();
            new_client->websocket = &conn;

            std::lock_guard lock(server_mutex_);
            clients_.insert(std::move(new_client));

            using nlohmann::json;
            const auto hello = json({
                {"type", "hello"},
                {"message", "Connected to StemSmith WebSocket Server"}
            });
            conn.send_text(hello.dump());
        })
        .onmessage([](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
            // Handle WebSocket messages (e.g., subscribe to job updates)
            if (!is_binary)
            {
                try
                {
                    using nlohmann::json;
                    auto msg = json::parse(data);
                    auto action = msg.value("action", "");

                    if (action == "subscribe")
                    {
                        auto job_id = msg.value("job_id", "");
                        // Add subscription logic here
                    }
                }
                catch (...) {}
            }
        })
        .onclose([this](crow::websocket::connection& conn, const std::string& reason, uint16_t code) {
            std::lock_guard<std::mutex> lock(server_mutex_);
            const auto it = std::ranges::find_if(clients_,
                                           [&conn](const std::unique_ptr<client>& c) {
                                               return c->websocket == &conn;
                                           });
            if (it != clients_.end()) {
                clients_.erase(it);
            }
        });
}

void api_server::broadcast_job_update(const nlohmann::json& message, const std::string& job_id)
{
    std::lock_guard lock(server_mutex_);
    for (const auto& client : clients_)
    {
        if (client && client->subscribed_jobs.contains(job_id) && client->websocket)
        {
            client->websocket->send_text(message.dump());
        }
    }
}

} // namespace stemsmith

