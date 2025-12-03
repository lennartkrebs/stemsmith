#include <filesystem>
#include <format>
#include <iostream>

#include "stemsmith/stemsmith.h"

namespace
{
constexpr auto kExampleTrack = "data/test_files/stemsmith_demo_track.wav";
const std::filesystem::path kModelCacheRoot = "build/model_cache";
const std::filesystem::path kOutputDir = "build/output_observer_separation";
#ifdef STEMSMITH_SOURCE_DIR
const std::filesystem::path kExampleTrackPath = std::filesystem::path{STEMSMITH_SOURCE_DIR} / kExampleTrack;
#endif
const std::filesystem::path kCopiedTrack = kOutputDir / "tmp" / "stemsmith_demo_track_copy.wav";

constexpr std::string_view status_to_string(stemsmith::job_status status)
{
    using namespace stemsmith;
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
    default:
        return "unknown";
    }
}
} // namespace

int main()
{
    using namespace stemsmith;

    runtime_config runtime;
    runtime.cache.root = kModelCacheRoot;
    runtime.output_root = kOutputDir;
    runtime.worker_count = 2; // ensure both jobs can run concurrently

    auto get_service = service::create(std::move(runtime));
    if (!get_service)
    {
        std::cerr << "Failed to create Stemsmith service: " << get_service.error() << std::endl;
        return 1;
    }

    auto& stemsmith_service = *get_service;

    // Prepare two distinct input files to avoid catalog deduplication.
    std::filesystem::path source_track =
#ifdef STEMSMITH_SOURCE_DIR
        kExampleTrackPath;
#else
        kExampleTrack;
#endif

    std::filesystem::create_directories(kCopiedTrack.parent_path());
    std::error_code copy_ec;
    std::filesystem::copy_file(source_track, kCopiedTrack, std::filesystem::copy_options::overwrite_existing, copy_ec);
    if (copy_ec)
    {
        std::cerr << "Failed to create temporary copy of demo track: " << copy_ec.message() << std::endl;
        return 1;
    }

    // Submit two jobs for the same track with different stem selections and output folders.
    std::vector<job_handle> handles;
    handles.reserve(2);

    const auto submit_job =
        [&stemsmith_service](std::string_view label,
                             std::filesystem::path input_path,
                             std::filesystem::path output_subdir,
                             std::vector<std::string> stems) -> std::expected<job_handle, std::string>
    {
        job_request req;
        req.input_path = std::move(input_path);
        req.output_subdir = std::move(output_subdir);
        req.stems = std::move(stems);

        req.observer.callback = [label](const job_descriptor& job, const job_event& evt)
        {
            const auto name = job.input_path.filename().string();
            const auto progress = evt.progress >= 0.0f ? std::format("{:>5.1f}%", evt.progress * 100.0f) : "  n/a";
            const auto message = evt.message.empty() ? std::string{} : std::format(" {}", evt.message);
            std::cout << std::format("[{} {}] {:<9} {}{}", label, name, status_to_string(evt.status), progress, message)
                      << std::endl;
        };

        return stemsmith_service->submit(std::move(req));
    };

    auto first = submit_job("jobA", source_track, "jobA", {"drums", "bass", "vocals"});
    if (!first)
    {
        std::cerr << "Failed to submit job A: " << first.error() << std::endl;
        return 1;
    }
    handles.push_back(std::move(*first));

    auto second = submit_job("jobB", kCopiedTrack, "jobB", {}); // request all stems
    if (!second)
    {
        std::cerr << "Failed to submit job B: " << second.error() << std::endl;
        return 1;
    }
    handles.push_back(std::move(*second));

    bool ok = true;
    for (auto& handle : handles)
    {
        if (const auto result = handle.result().get(); result.status != job_status::completed)
        {
            std::cerr << "Job failed: " << (result.error ? *result.error : "unknown error") << std::endl;
            ok = false;
        }
        else
        {
            std::cout << "Separated stems written to: " << result.output_dir << std::endl;
        }
    }

    return ok ? 0 : 1;
}
