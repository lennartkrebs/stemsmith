#include "stemsmith/stemsmith.h"

#include "stemsmith/http_weight_fetcher.h"

#include <chrono>
#include <filesystem>
#include <future>
#include <iomanip>
#include <iostream>
#include <string>

namespace
{
void print_progress_bar(float progress, const std::string& message)
{
    constexpr int kBarWidth = 40;
    const int filled = static_cast<int>(progress * kBarWidth);
    std::cout << '\r' << std::setw(3) << static_cast<int>(progress * 100) << "% [";
    for (int i = 0; i < kBarWidth; ++i)
    {
        if (i < filled)
        {
            std::cout << '#';
        }
        else
        {
            std::cout << '-';
        }
    }
    std::cout << "] " << message << std::flush;
    if (progress >= 1.0f)
    {
        std::cout << std::endl;
    }
}
}

int main()
{
    using namespace stemsmith;

    const std::filesystem::path cache_root = "build/example_cache";
    const std::filesystem::path output_root = "build/example_output";
#ifdef STEMSMITH_SOURCE_DIR
    const std::filesystem::path input_track = std::filesystem::path{STEMSMITH_SOURCE_DIR} / "data/test_files/example_track.wav";
#endif

    job_config config;

    auto fetcher = std::make_shared<http_weight_fetcher>();
    auto service_result = service::create(config,
                                          cache_root,
                                          output_root,
                                          fetcher,
                                          1,
                                          [](const job_descriptor& job, const job_event& event) {
                                              if (event.progress >= 0.0f)
                                              {
                                                  print_progress_bar(event.progress, event.message.empty() ? job.input_path.filename().string() : event.message);
                                              }
                                              else if (event.status == job_status::queued)
                                              {
                                                  std::cout << "Queued: " << job.input_path << std::endl;
                                              }
                                          });
    if (!service_result)
    {
        std::cerr << "Failed to initialize Stemsmith service: " << service_result.error() << std::endl;
        return 1;
    }

    auto stemsmith_service = std::move(service_result.value());

    if (!std::filesystem::exists(input_track))
    {
        std::cerr << "Input track not found: " << input_track << std::endl;
        return 1;
    }

    auto future = stemsmith_service->runner().submit(input_track);
    if (!future)
    {
        std::cerr << "Unable to submit job: " << future.error() << std::endl;
        return 1;
    }

    const auto result = future->get();
    if (result.status != job_status::completed)
    {
        std::cerr << "Separation failed: " << (result.error ? *result.error : "unknown error") << std::endl;
        return 1;
    }

    std::cout << "Stems written to " << result.output_dir << std::endl;
    return 0;
}
