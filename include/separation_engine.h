#pragma once

#include <string_view>
#include <vector>
#include <future>
#include <model.hpp>

namespace stemsmith {

class separation_engine
{
public:
    using job_id_t = uint32_t;

    struct stem_buffer
    {
        std::string name;
        std::vector<float> pcm;
    };

    enum class job_status
    {
        pending,
        processing,
        completed,
        failed,
        cancelled
    };

    struct worker
    {
        std::thread thread;
        demucscpp::demucs_model model{};
        std::atomic<bool> busy{false};
    };

    struct job
    {
        job_id_t id;
        std::vector<float> input_pcm;
        size_t pcm_frames;
        std::promise<std::vector<stem_buffer>> promise;
        std::atomic<job_status> status{job_status::pending};
        std::vector<stem_buffer> results;
        std::atomic<bool> cancelled{false};
    };

    explicit separation_engine(std::string_view model_path, size_t num_workers = 4);
    ~separation_engine();

    job_id_t submit(const std::vector<float>& pcm, size_t pcm_frames);
    std::future<std::vector<stem_buffer>> submit_async(const std::vector<float>& pcm, size_t pcm_frames);

    job_status get_status(job_id_t jobId) const;

    bool retrieve(job_id_t jobId, std::vector<stem_buffer>& outStems);
    bool cancel(job_id_t jobId);

private:
    void worker_loop(worker& w);

    std::string_view model_path;
    std::vector<worker> workers;

    std::unordered_map<job_id_t, std::shared_ptr<job>> jobs;
    std::mutex jobs_mutex;
    std::atomic<job_id_t> next_job_id{1};
    std::atomic<bool> shutdown{false};
    std::condition_variable job_cv;
};

} // namespace stemsmith