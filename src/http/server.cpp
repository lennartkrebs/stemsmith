#include "server.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>

namespace stemsmith::http
{

namespace
{
constexpr std::string_view to_string(job_status status)
{
    switch (status)
    {
    case job_status::queued:
        return "queued";
    case job_status::running:
        return "running";
    case job_status::completed:
        return "completed";
    case job_status::failed:
        return "failed";
    case job_status::cancelled:
        return "cancelled";
    }
    return "unknown";
}
} // namespace

std::string job_registry::next_id()
{
    return std::to_string(next_id_.fetch_add(1));
    ;
}

void job_registry::add(const std::string& id, job_handle handle)
{
    std::lock_guard lock(mutex_);
    job_state state;
    state.last_event.id = handle.id();
    state.last_event.status = job_status::queued;
    state.handle = std::move(handle);
    jobs_.emplace(id, std::move(state));
}

void job_registry::update(const std::string& id, const job_descriptor& desc, const job_event& ev)
{
    std::lock_guard lock(mutex_);
    const auto it = jobs_.find(id);
    if (it == jobs_.end())
    {
        return;
    }

    it->second.last_event = ev;
    if (ev.status == job_status::completed || ev.status == job_status::failed)
    {
        it->second.output_dir = desc.output_dir;
    }
}

std::optional<job_state> job_registry::get(const std::string& id) const
{
    std::lock_guard lock(mutex_);
    if (const auto it = jobs_.find(id); it != jobs_.end())
    {
        return it->second;
    }

    return std::nullopt;
}

server::server(const config& cfg) : config_(cfg) {}

server::~server()
{
    stop();
}

void server::start()
{
    if (running_.exchange(true))
    {
        return;
    }

    thread_ = std::thread([this] { run(); });
}

void server::stop()
{
    if (!running_.exchange(false))
    {
        return;
    }

    app_.stop();

    if (thread_.joinable())
    {
        thread_.join();
    }
}

void server::run()
{
    runtime_config runtime{};
    // Maybe assert rather than defaulting?
    runtime.cache.root = config_.cache_root.empty() ? "build/model_cache" : config_.cache_root;
    runtime.output_root = config_.output_root.empty() ? "build/output" : config_.output_root;
    runtime.worker_count = config_.worker_count;

    if (auto created = service::create(std::move(runtime)); created)
    {
        svc_ = std::move(*created);
    }
    else
    {
        svc_.reset();
    }

    register_routes();
    app_.loglevel(crow::LogLevel::Warning);
    app_.bindaddr(config_.bind_address).port(config_.port).multithreaded().run();
}

crow::response server::handle_post_job(const crow::request& req)
{
    if (!svc_)
    {
        return crow::response{crow::status::SERVICE_UNAVAILABLE, R"({"error":"service not ready"})"};
    }

    if (const auto content_type = req.get_header_value("Content-Type");
        content_type.find("multipart/form-data") == std::string::npos)
    {
        return crow::response{crow::status::BAD_REQUEST, R"({"error":"multipart/form-data required"})"};
    }

    crow::multipart::message msg(req);
    auto it = msg.part_map.find("file");
    if (it == msg.part_map.end())
    {
        return crow::response{crow::status::BAD_REQUEST, R"({"error":"file field required"})"};
    }

    const auto& [headers, header_body] = it->second;
    const auto& [_, params] = crow::multipart::get_header_object(headers, "Content-Disposition");

    std::string filename;
    if (const auto name_it = params.find("filename"); name_it != params.end())
    {
        filename = name_it->second;
    }

    auto ends_with = [](const std::string& value, const std::string& suffix)
    {
        return value.size() >= suffix.size() && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    };

    if (filename.empty() || !(ends_with(filename, ".wav") || ends_with(filename, ".WAV")))
    {
        return crow::response{crow::status::BAD_REQUEST, R"({"error":"WAV input required"})"};
    }

    const auto job_id = registry_.next_id();
    const auto uploads_root =
        config_.output_root.empty() ? std::filesystem::path("build/uploads") : config_.output_root / "uploads";
    std::error_code ec;
    std::filesystem::create_directories(uploads_root, ec);
    if (ec)
    {
        return crow::response{crow::status::INTERNAL_SERVER_ERROR, R"({"error":"failed to prepare upload dir"})"};
    }

    const auto target_path = uploads_root / (job_id + "-" + filename);
    std::ofstream out(target_path, std::ios::binary);
    out.write(header_body.data(), static_cast<std::streamsize>(header_body.size()));
    if (!out)
    {
        return crow::response{crow::status::INTERNAL_SERVER_ERROR, R"({"error":"failed to save upload"})"};
    }

    job_request job{};
    job.input_path = target_path;
    job.observer.callback = [this, job_id](const job_descriptor& desc, const job_event& ev)
    { registry_.update(job_id, desc, ev); };

    const auto handle = svc_->submit(std::move(job));
    if (!handle)
    {
        std::filesystem::remove(target_path, ec);
        crow::json::wvalue body;
        body["error"] = handle.error();
        return crow::response{crow::status::BAD_REQUEST, body};
    }

    // Store the job handle for later status queries
    registry_.add(job_id, *handle);

    crow::json::wvalue body;
    body["id"] = job_id;
    return crow::response{crow::status::ACCEPTED, body};
}

crow::response server::handle_get_job(const std::string& id) const
{
    if (const auto state = registry_.get(id))
    {
        crow::json::wvalue body;
        body["id"] = id;
        body["status"] = to_string(state->last_event.status).data();
        body["progress"] = state->last_event.progress;

        if (!state->output_dir.empty())
        {
            body["output_dir"] = state->output_dir.string();
        }

        if (state->last_event.error)
        {
            body["error"] = *state->last_event.error;
        }

        return crow::response{crow::status::OK, body};
    }
    return crow::response{crow::status::NOT_FOUND, R"({"error":"job not found"})"};
}

crow::response server::handle_download(const std::string& id) const
{
    const auto state = registry_.get(id);
    if (!state)
    {
        return crow::response{crow::status::NOT_FOUND, R"({"error":"job not found"})"};
    }

    if (state->last_event.status != job_status::completed)
    {
        return crow::response{crow::status::CONFLICT, R"({"error":"job not completed"})"};
    }

    if (state->output_dir.empty())
    {
        return crow::response{crow::status::INTERNAL_SERVER_ERROR, R"({"error":"missing output path"})"};
    }

    return crow::response{crow::status::NOT_IMPLEMENTED, R"({"error":"download packaging not implemented"})"};
}

void server::register_routes()
{
    CROW_ROUTE(app_, "/health")(
        []
        {
            crow::json::wvalue payload;
            payload["status"] = "ok";
            return crow::response{crow::status::OK, payload};
        });

    CROW_ROUTE(app_, "/")(
        []
        {
            crow::json::wvalue payload;
            payload["message"] = "Welcome to the StemSmith Job Server";
            return crow::response{crow::status::OK, payload};
        });

    CROW_ROUTE(app_, "/jobs")
        .methods(crow::HTTPMethod::POST)([&](const crow::request& request) { return handle_post_job(request); });

    CROW_ROUTE(app_, "/jobs/<string>")([&](const std::string& job_id) { return handle_get_job(job_id); });

    CROW_ROUTE(app_, "/jobs/<string>/download")([&](const std::string& job_id) { return handle_download(job_id); });
}

} // namespace stemsmith::http
