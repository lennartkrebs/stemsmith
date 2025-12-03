#include <filesystem>
#include <format>
#include <iostream>

#include "stemsmith/stemsmith.h"

namespace
{
constexpr auto kExampleTrackDir = "data/test_files/stemsmith_demo_track.wav";
const std::filesystem::path kModelCacheRoot = "build/model_cache";
const std::filesystem::path kOutputDir = "build/output_simple_separation";
#ifdef STEMSMITH_SOURCE_DIR
const std::filesystem::path kExampleTrackPath = std::filesystem::path{STEMSMITH_SOURCE_DIR} / kExampleTrackDir;
#endif

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

    // Create the Stemsmith service
    runtime_config runtime;
    runtime.cache.root = kModelCacheRoot;
    runtime.output_root = kOutputDir;

    runtime.on_job_event = [](const job_descriptor& job, const job_event& evt)
    {
        const auto name = job.input_path.filename().string();
        const auto progress = evt.progress >= 0.0f ? std::format("{:>5.1f}%", evt.progress * 100.0f) : "  n/a";
        const auto message = evt.message.empty() ? std::string{} : std::format(" {}", evt.message);
        std::cout << std::format("[{}] {:<9} {}{}", name, status_to_string(evt.status), progress, message) << std::endl;
    };

    auto get_service = service::create(std::move(runtime));

    if (!get_service)
    {
        std::cerr << "Failed to create Stemsmith service: " << get_service.error() << std::endl;
        return 1;
    }

    auto& stemsmith_service = *get_service;

    job_request request;
#ifdef STEMSMITH_SOURCE_DIR
    request.input_path = kExampleTrackPath;
#else
    request.input_path = kExampleTrackDir;
#endif

    request.stems = {"drums", "bass", "vocals"};            // Separate only drums, bass, and vocals
    request.profile = model_profile_id::balanced_four_stem; // Use the 4-stem model profile

    const auto submit = stemsmith_service->submit(request);
    if (!submit)
    {
        std::cerr << "Failed to submit job: " << submit.error() << std::endl;
        return 1;
    }

    auto result = submit->result().get();
    if (result.status != job_status::completed)
    {
        std::cerr << "Job failed: " << (result.error ? *result.error : "unknown error") << std::endl;
        return 1;
    }

    std::cout << "Separated stems written to: " << result.output_dir << std::endl;
    return 0;
}
