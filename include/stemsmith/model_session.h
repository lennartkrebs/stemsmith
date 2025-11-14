#pragma once

#include "stemsmith/audio_buffer.h"
#include "stemsmith/job_config.h"
#include "stemsmith/model_cache.h"

#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "model.hpp"

namespace stemsmith
{

struct separation_result
{
    std::vector<std::pair<std::string, audio_buffer>> stems;
};

class model_session
{
public:
    using weight_resolver = std::function<std::expected<std::filesystem::path, std::string>()>;
    using loader_function = std::function<std::expected<void, std::string>(demucscpp::demucs_model&, const std::filesystem::path&)>;
    using inference_function = std::function<Eigen::Tensor3dXf(const demucscpp::demucs_model&, const Eigen::MatrixXf&, demucscpp::ProgressCallback)>;

    model_session(model_profile profile, model_cache& cache);
    model_session(model_profile profile, weight_resolver resolver, loader_function loader, inference_function inference);

    std::expected<separation_result, std::string> separate(const audio_buffer& input, std::span<const std::string_view> stems_to_extract = {});

private:
    std::expected<demucscpp::demucs_model*, std::string> ensure_model_loaded();
    std::expected<std::vector<std::size_t>, std::string> resolve_stem_indices(std::span<const std::string_view> stems) const;

    model_profile profile_;
    weight_resolver resolver_;
    loader_function loader_;
    inference_function inference_;
    std::unique_ptr<demucscpp::demucs_model> model_;
};

} // namespace stemsmith
