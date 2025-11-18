#include "stemsmith/separation_engine.h"

#include <algorithm>
#include <string_view>
#include <vector>

namespace stemsmith
{

namespace
{
std::filesystem::path job_output_directory(const std::filesystem::path& root, const job_descriptor& job)
{
    const auto stem = job.input_path.stem();
    return root / stem;
}
} // namespace

separation_engine::separation_engine(model_cache& cache,
                                     std::filesystem::path output_root,
                                     audio_loader loader,
                                     audio_writer writer)
    : output_root_(std::move(output_root))
    , model_session_pool_(cache)
    , loader_(std::move(loader))
    , writer_(std::move(writer))
{
}

separation_engine::separation_engine(model_session_pool&& pool,
                                     std::filesystem::path output_root,
                                     audio_loader loader,
                                     audio_writer writer)
    : output_root_(std::move(output_root))
    , model_session_pool_(std::move(pool))
    , loader_(std::move(loader))
    , writer_(std::move(writer))
{
}

std::expected<std::filesystem::path, std::string> separation_engine::process(const job_descriptor& job,
                                                                             demucscpp::ProgressCallback progress_cb)
{
    if (!loader_)
    {
        return std::unexpected("No audio loader configured");
    }
    if (!writer_)
    {
        return std::unexpected("No audio writer configured");
    }

    auto audio = loader_(job.input_path);
    if (!audio)
    {
        return std::unexpected(audio.error());
    }

    auto session_handle = model_session_pool_.acquire(job.config.profile);
    if (!session_handle)
    {
        return std::unexpected(session_handle.error());
    }

    std::vector<std::string_view> filter_views;
    if (!job.config.stems_filter.empty())
    {
        filter_views.reserve(job.config.stems_filter.size());
        for (const auto& stem : job.config.stems_filter)
        {
            filter_views.emplace_back(stem);
        }
    }

    std::span<const std::string_view> filter_span;
    if (!filter_views.empty())
    {
        filter_span = {filter_views.data(), filter_views.size()};
    }

    auto result = session_handle->get()->separate(*audio, filter_span, std::move(progress_cb));
    if (!result)
    {
        return std::unexpected(result.error());
    }

    auto job_dir = job_output_directory(output_root_, job);
    std::error_code ec;
    std::filesystem::create_directories(job_dir, ec);
    if (ec)
    {
        return std::unexpected("Failed to create output directory: " + ec.message());
    }

    for (auto& [stem_name, buffer] : result->stems)
    {
        const auto stem_path = job_dir / (stem_name + ".wav");
        if (const auto status = writer_(stem_path, buffer); !status)
        {
            return std::unexpected(status.error());
        }
    }

    return job_dir;
}

} // namespace stemsmith
