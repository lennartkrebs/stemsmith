#pragma once

#include "job_queue.h"
#include <map>
#include <shared_mutex>
#include <functional>
#include <string_view>

namespace stemsmith {

class job_manager {
public:
    using job_ptr = std::shared_ptr<job>;
    using job_update_callback = std::function<void(const job_ptr&)>;

    explicit job_manager(size_t worker_threads = std::thread::hardware_concurrency());
    ~job_manager();

    // Job management
    std::string submit_job(
        std::string_view input_path,
        std::string_view output_path,
        std::string_view model_name = "htdemucs",
        std::string_view mode = "fast");

    job_ptr get_job(std::string_view job_id) const;
    std::vector<job_ptr> get_all_jobs() const;
    std::vector<job_ptr> get_jobs_by_status(std::string_view status) const;
    bool cancel_job(std::string_view job_id);

    void on_job_submitted(job_update_callback callback);
    void on_job_progress(job_update_callback callback);
    void on_job_completed(job_update_callback callback);
    void on_job_failed(job_update_callback callback);

private:
    std::string generate_job_id();
    void setup_queue_callbacks();
    void notify_callbacks(const std::vector<job_update_callback>& callbacks, const job_ptr& job);

    std::unique_ptr<job_queue> queue_;
    mutable std::shared_mutex jobs_mutex_;
    std::map<std::string, job_ptr> jobs_;
    std::atomic<uint64_t> job_counter_{0};

    // Callback storage
    std::vector<job_update_callback> on_submitted_callbacks_;
    std::vector<job_update_callback> on_progress_callbacks_;
    std::vector<job_update_callback> on_completed_callbacks_;
    std::vector<job_update_callback> on_failed_callbacks_;
    mutable std::shared_mutex callbacks_mutex_;
};

} // namespace stemsmith