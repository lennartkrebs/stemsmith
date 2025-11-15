#pragma once

#include "stemsmith/audio_buffer.h"
#include "stemsmith/audio_io.h"
#include "stemsmith/job_catalog.h"
#include "stemsmith/model_cache.h"
#include "stemsmith/model_session_pool.h"

#include <expected>
#include <filesystem>
#include <functional>

namespace stemsmith
{

class separation_engine
{
public:
    using audio_loader = std::function<std::expected<audio_buffer, std::string>(const std::filesystem::path&)>;
    using audio_writer = std::function<std::expected<void, std::string>(const std::filesystem::path&, const audio_buffer&)>;

    separation_engine(model_cache& cache,
                      std::filesystem::path output_root,
                      audio_loader loader = load_audio_file,
                      audio_writer writer = [](const std::filesystem::path& path, const audio_buffer& buffer) {
                          return write_audio_file(path, buffer);
                      });

    separation_engine(model_session_pool&& pool,
                      std::filesystem::path output_root,
                      audio_loader loader,
                      audio_writer writer);

    [[nodiscard]] std::expected<std::filesystem::path, std::string>
    process(const job_descriptor& job,
            demucscpp::ProgressCallback progress_cb = {});

private:
    std::filesystem::path output_root_;
    model_session_pool pool_;
    audio_loader loader_;
    audio_writer writer_;
};

} // namespace stemsmith
