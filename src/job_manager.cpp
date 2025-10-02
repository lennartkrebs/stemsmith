#include "job_manager.h"

#include <sstream>
#include <chrono>
#include <ranges>

namespace {
std::string make_job_id(uint64_t counter) {
    using clock = std::chrono::system_clock;
    auto now = clock::now();
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    std::ostringstream oss;
    oss << "job_" << secs << '_' << counter;
    return oss.str();
}
}

namespace stemsmith {

job_manager::job_manager(size_t worker_threads)
    : queue_(std::make_unique<job_queue>(worker_threads))
{
    // Wire queue callbacks to notify subscribers about job updates
    queue_->on_progress = [this](const job& j) {
        auto ptr = get_job(j.id);
        if (ptr) notify_listeners(ptr);
    };
    queue_->on_complete = [this](const job& j) {
        auto ptr = get_job(j.id);
        if (ptr) notify_listeners(ptr);
    };
    queue_->on_error = [this](const job& j) {
        auto ptr = get_job(j.id);
        if (ptr) notify_listeners(ptr);
    };
}

job_manager::~job_manager() = default;

std::string job_manager::submit_job(const job_parameters& parameters)
{
    if (parameters.input_path.empty()) {
        throw std::invalid_argument("input_path required");
    }
    if (parameters.output_path.empty()) {
        throw std::invalid_argument("output_path required");
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
    const auto id = listener_counter_.fetch_add(1, std::memory_order_relaxed) + 1;
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
    for (const auto& val : listeners_ | std::views::values) {
        try {
            if (val) val(j);
        } catch (...) {
            // swallow exceptions from listeners
        }
    }
}

} // namespace stemsmith