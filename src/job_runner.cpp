#include "stemsmith/job_runner.h"

#include <stdexcept>

namespace stemsmith
{

namespace
{
separation_engine make_engine(model_cache& cache, std::filesystem::path output_root)
{
    return separation_engine(cache, std::move(output_root));
}
} // namespace

job_runner::job_runner(job_config base_config, model_cache& cache, std::filesystem::path output_root, std::size_t worker_count, std::function<void(const job_descriptor&, float, const std::string&)> progress, std::function<void(const job_event&, const job_descriptor&)> status_callback)
    : job_runner(std::move(base_config), make_engine(cache, std::move(output_root)), worker_count, std::move(progress), std::move(status_callback))
{}

job_runner::job_runner(job_config base_config, separation_engine engine, std::size_t worker_count, std::function<void(const job_descriptor&, float, const std::string&)> progress, std::function<void(const job_event&, const job_descriptor&)> status_callback)
    : catalog_(std::move(base_config))
    , engine_(std::move(engine))
    , pool_(worker_count,
            [this](const job_descriptor& job, const std::atomic_bool& stop_flag) {
                process_job(job, stop_flag);
            },
            [this](const job_event& event) { handle_event(event); })
    , progress_(std::move(progress))
    , status_callback_(std::move(status_callback))
{}

std::expected<std::future<job_result>, std::string> job_runner::submit(const std::filesystem::path& path, const job_overrides& overrides)
{
    auto add_result = catalog_.add_file(path, overrides);
    if (!add_result)
    {
        return std::unexpected(add_result.error());
    }

    const auto& job = catalog_.jobs().at(add_result.value());
    const auto context = std::make_shared<job_context>();
    context->job = job;
    auto future = context->promise.get_future();

    {
        std::lock_guard lock(mutex_);
        contexts_[job.input_path] = context;
    }

    const auto job_id = pool_.enqueue(job);
    if (job_id == static_cast<std::size_t>(-1))
    {
        std::lock_guard lock(mutex_);
        contexts_.erase(job.input_path);
        return std::unexpected("Worker pool is shut down");
    }

    std::vector<job_event> pending;
    {
        std::lock_guard lock(mutex_);
        paths_by_id_[job_id] = job.input_path;
        context->job_id = job_id;
        if (auto pending_it = pending_events_.find(job_id); pending_it != pending_events_.end())
        {
            pending = std::move(pending_it->second);
            pending_events_.erase(pending_it);
        }
    }

    for (const auto& event : pending)
    {
        handle_event(event);
    }

    return future;
}

void job_runner::process_job(const job_descriptor& job, const std::atomic_bool& stop_flag)
{
    if (stop_flag.load())
    {
        return;
    }

    demucscpp::ProgressCallback cb;
    if (progress_)
    {
        job_descriptor job_copy = job;
        cb = [this, job_copy](float pct, const std::string& message) {
            progress_(job_copy, pct, message);
        };
    }

    const auto result = engine_.process(job, std::move(cb));
    const auto context = context_for(job.input_path);

    if (!result)
    {
        if (context)
        {
            context->error = result.error();
        }
        throw std::runtime_error(result.error());
    }

    if (context)
    {
        context->output_dir = result.value();
    }
}

void job_runner::handle_event(const job_event& event)
{
    std::shared_ptr<job_context> context;
    std::filesystem::path input_path;

    {
        std::lock_guard lock(mutex_);
        auto path_it = paths_by_id_.find(event.id);
        if (path_it == paths_by_id_.end())
        {
            pending_events_[event.id].push_back(event);
            return;
        }

        input_path = path_it->second;
        if (auto ctx_it = contexts_.find(input_path); ctx_it != contexts_.end())
        {
            context = ctx_it->second;
        }

        if (event.status == job_status::completed ||
            event.status == job_status::failed ||
            event.status == job_status::cancelled)
        {
            paths_by_id_.erase(path_it);
            contexts_.erase(input_path);
            catalog_.release(input_path);
        }
    }

    if (context && status_callback_)
    {
        status_callback_(event, context->job);
    }

    if (!context)
    {
        return;
    }

    switch (event.status)
    {
    case job_status::completed: {
        job_result result;
        result.input_path = input_path;
        result.status = job_status::completed;
        result.output_dir = context->output_dir.value_or(std::filesystem::path{});
        context->promise.set_value(std::move(result));
        break;
    }
    case job_status::failed:
    case job_status::cancelled: {
        job_result result;
        result.input_path = input_path;
        result.status = event.status;
        if (context->error)
        {
            result.error = context->error;
        }
        else if (event.error)
        {
            result.error = event.error;
        }
        context->promise.set_value(std::move(result));
        break;
    }
    default:
        break;
    }
}

std::shared_ptr<job_runner::job_context> job_runner::context_for(const std::filesystem::path& path) const
{
    std::lock_guard lock(mutex_);
    const auto it = contexts_.find(path);
    if (it == contexts_.end())
    {
        return {};
    }
    return it->second;
}

} // namespace stemsmith
