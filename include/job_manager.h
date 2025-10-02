#pragma once

#include "job_queue.h"
#include "job_builder.h"

#include <map>
#include <shared_mutex>
#include <functional>
#include <atomic>

namespace stemsmith {

class job_manager {
public:
    using job_ptr = std::shared_ptr<job>;
    using job_update_callback = std::function<void(const job_ptr&)>;

    explicit job_manager(size_t worker_threads = std::thread::hardware_concurrency());
    ~job_manager();

    static job_builder create_job() { return {}; };

    std::string submit_job(const job_parameters& parameters);

    job_ptr get_job(const std::string& id) const;
    std::vector<job_ptr> list_jobs() const;

    // Subscribe to job updates (progress, complete, error). Returns a subscription id.
    uint64_t subscribe(job_update_callback cb);
    void unsubscribe(uint64_t subscription_id);

private:
    std::unique_ptr<job_queue> queue_;
    mutable std::shared_mutex jobs_mutex_;
    std::map<std::string, job_ptr> jobs_;
    std::atomic<uint64_t> job_counter_{0};

    // listeners
    mutable std::shared_mutex listeners_mutex_;
    std::map<uint64_t, job_update_callback> listeners_;
    std::atomic<uint64_t> listener_counter_{0};

    void notify_listeners(const job_ptr& j) const;
};

} // namespace stemsmith