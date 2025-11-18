#include "stemsmith/job_runner.h"

#include <stdexcept>

namespace stemsmith
{

namespace
{
separation_engine make_engine(model_cache& cache, std::filesystem::path output_root)
{
    return {cache, std::move(output_root)};
}
} // namespace

job_runner::job_runner(job_config base_config,
                       model_cache& cache,
                       std::filesystem::path output_root,
                       std::size_t worker_count,
                       std::function<void(const job_descriptor&, const job_event&)> event_callback)
    : job_runner(std::move(base_config),
                 make_engine(cache, std::move(output_root)),
                 worker_count,
                 std::move(event_callback))
{
}

job_runner::job_runner(job_config base_config,
                       separation_engine engine,
                       std::size_t worker_count,
                       std::function<void(const job_descriptor&, const job_event&)> event_callback)
    : catalog_(std::move(base_config))
    , engine_(std::move(engine))
    , event_callback_(std::move(event_callback))
    , pool_(
          worker_count,
          [this](const job_descriptor& job, const std::atomic_bool& stop_flag) { process_job(job, stop_flag); },
          [this](const job_event& event) { handle_event(event); })
{
}

std::expected<job_handle, std::string> job_runner::submit(const std::filesystem::path& path,
                                                          const job_overrides& overrides,
                                                          job_observer observer)
{
    auto add_result = catalog_.add_file(path, overrides);
    if (!add_result)
    {
        return std::unexpected(add_result.error());
    }

    const auto& job = catalog_.jobs().at(add_result.value());
    const auto context = std::make_shared<job_context>();
    context->job = job;
    auto shared_future = context->promise.get_future().share();

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

    auto handle_state = std::make_shared<job_handle_state>();
    handle_state->job = job;
    handle_state->job_id = job_id;
    handle_state->future = shared_future;
    handle_state->pool = &pool_;

    std::vector<job_event> pending;
    {
        std::lock_guard lock(mutex_);
        paths_by_id_[job_id] = job.input_path;
        context->job_id = job_id;
        context->observer = std::move(observer);
        context->handle_state = handle_state;
        if (const auto pending_it = pending_events_.find(job_id); pending_it != pending_events_.end())
        {
            pending = std::move(pending_it->second);
            pending_events_.erase(pending_it);
        }
    }

    for (const auto& event : pending)
    {
        handle_event(event);
    }

    return job_handle(std::move(handle_state));
}

void job_runner::process_job(const job_descriptor& job, const std::atomic_bool& stop_flag)
{
    if (stop_flag.load())
    {
        return;
    }

    const job_descriptor& job_copy = job;
    demucscpp::ProgressCallback cb = [this, job_copy, &stop_flag](float pct, const std::string& message)
    {
        if (const auto ctx = context_for(job_copy.input_path))
        {
            job_event evt;
            evt.id = ctx->job_id;
            evt.status = job_status::running;
            evt.progress = pct;
            evt.message = message;
            notify_observers(ctx, evt);
        }

        if (stop_flag.load())
        {
            throw std::runtime_error("Job cancelled");
        }
    };

    const auto result = engine_.process(job, std::move(cb));
    if (stop_flag.load())
    {
        return;
    }
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

        if (event.status == job_status::completed || event.status == job_status::failed ||
            event.status == job_status::cancelled)
        {
            paths_by_id_.erase(path_it);
            contexts_.erase(input_path);
            catalog_.release(input_path);
        }
    }

    if (!context)
    {
        return;
    }

    notify_observers(context, event);

    switch (event.status)
    {
    case job_status::completed:
    {
        job_result result;
        result.input_path = input_path;
        result.status = job_status::completed;
        result.output_dir = context->output_dir.value_or(std::filesystem::path{});
        context->promise.set_value(std::move(result));
        break;
    }
    case job_status::failed:
    case job_status::cancelled:
    {
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

void job_runner::notify_observers(const std::shared_ptr<job_context>& context, const job_event& event) const
{
    if (!context)
    {
        return;
    }

    if (event_callback_)
    {
        event_callback_(context->job, event);
    }

    if (context->observer.callback)
    {
        context->observer.callback(context->job, event);
    }

    if (const auto handle_state = context->handle_state.lock())
    {
        handle_state->notify(context->job, event);
    }
}

} // namespace stemsmith
