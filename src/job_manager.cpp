#include "job_manager.h"

#include <sstream>
#include <chrono>
#include <ranges>

namespace {
std::string make_job_id(uint64_t counter) {
    return "job_id_" + std::to_string(counter);
}
}

namespace stemsmith {

job_manager::job_manager(size_t worker_threads)
    : queue_(std::make_unique<job_queue>(worker_threads))
{
    queue_->progress_callback = [this](const job& j) {
        if (const auto job = get_job(j.id)) {
            notify_listeners(job);
        }
    };
}

job_manager::~job_manager() = default;

std::string job_manager::submit_job(const job_parameters& parameters)
{
    if (parameters.input_path.empty())
    {
        throw std::invalid_argument("Input path is required");
    }

    if (parameters.output_path.empty())
    {
        throw std::invalid_argument("Output path is required");
    }

    const auto id = make_job_id(job_counter_.fetch_add(1, std::memory_order_relaxed));
    auto j = std::make_shared<job>();
    j->id = id;
    j->input_path = parameters.input_path;
    j->output_path = parameters.output_path;
    j->model_name = parameters.model;
    j->mode = parameters.mode;
    j->state.store(job_state::queued, std::memory_order_release);
    j->progress.store(0.f, std::memory_order_release);

    {
        std::unique_lock lk(jobs_mutex_);
        jobs_.emplace(id, j);
    }

    queue_->push(j);
    return id;
}

job_manager::job_ptr job_manager::get_job(const std::string& id) const {
    std::shared_lock lk(jobs_mutex_);
    auto it = jobs_.find(id);
    if (it == jobs_.end()) return nullptr;
    return it->second;
}

std::vector<job_manager::job_ptr> job_manager::list_jobs() const {
    std::vector<job_ptr> out;
    std::shared_lock lk(jobs_mutex_);
    out.reserve(jobs_.size());
    for (const auto& val : jobs_ | std::views::values) out.push_back(val);
    return out;
}

uint64_t job_manager::subscribe(job_update_callback cb) {
    const auto id = listener_counter_.fetch_add(1, std::memory_order_relaxed);
    std::unique_lock lk(listeners_mutex_);
    listeners_.emplace(id, std::move(cb));
    return id;
}

void job_manager::unsubscribe(uint64_t subscription_id) {
    std::unique_lock lk(listeners_mutex_);
    listeners_.erase(subscription_id);
}

void job_manager::notify_listeners(const job_ptr& j) const {
    std::shared_lock lk(listeners_mutex_);
    for (const auto& callback : listeners_ | std::views::values) {
        try {
            if (callback) callback(j);
        } catch (...) {
            // swallow exceptions from listeners
        }
    }
}

} // namespace stemsmith