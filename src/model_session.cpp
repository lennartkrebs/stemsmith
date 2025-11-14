#include "stemsmith/model_session.h"

#include <algorithm>
#include <stdexcept>

#include <Eigen/Dense>

namespace
{
constexpr int kExpectedChannels = 2;
constexpr int kExpectedSampleRate = demucscpp::SUPPORTED_SAMPLE_RATE;
}

namespace stemsmith
{

namespace
{
std::expected<void, std::string> default_loader(demucscpp::demucs_model& model, const std::filesystem::path& weights_path)
{
    if (!demucscpp::load_demucs_model(weights_path.string(), &model))
    {
        return std::unexpected("Failed to load Demucs weights: " + weights_path.string());
    }
    return {};
}

Eigen::Tensor3dXf default_inference(const demucscpp::demucs_model& model, const Eigen::MatrixXf& audio, const demucscpp::ProgressCallback& cb)
{
    return demucscpp::demucs_inference(model, audio, cb);
}
} // namespace

model_session::model_session(model_profile profile, model_cache& cache)
    : model_session(
        profile,
        [&cache, profile]() -> std::expected<std::filesystem::path, std::string> {
            auto handle = cache.ensure_ready(profile.id);
            if (!handle)
            {
                return std::unexpected(handle.error());
            }
            return handle->weights_path;
        },
        default_loader,
        default_inference)
{
}

model_session::model_session(model_profile profile, weight_resolver resolver, loader_function loader, inference_function inference)
    : profile_(profile)
    , resolver_(std::move(resolver))
    , loader_(std::move(loader))
    , inference_(std::move(inference))
{}

std::expected<demucscpp::demucs_model*, std::string> model_session::ensure_model_loaded()
{
    if (model_)
    {
        return model_.get();
    }

    if (!resolver_ || !loader_ || !inference_)
    {
        return std::unexpected("Model session is missing its runtime hooks");
    }

    auto weights_path = resolver_();
    if (!weights_path)
    {
        return std::unexpected(weights_path.error());
    }

    auto model = std::make_unique<demucscpp::demucs_model>();
    if (auto load_status = loader_(*model, weights_path.value()); !load_status)
    {
        return std::unexpected(load_status.error());
    }

    model_ = std::move(model);
    return model_.get();
}

std::expected<std::vector<std::size_t>, std::string> model_session::resolve_stem_indices(std::span<const std::string_view> stems) const
{
    std::vector<std::size_t> indices;
    if (stems.empty())
    {
        indices.reserve(profile_.stem_count);
        for (std::size_t i = 0; i < profile_.stem_count; ++i)
        {
            indices.push_back(i);
        }
        return indices;
    }

    indices.reserve(stems.size());
    for (const auto requested : stems)
    {
        const auto it = std::find_if(profile_.stems.begin(), profile_.stems.begin() + profile_.stem_count,
            [&](std::string_view stem) { return stem == requested; });

        if (it == profile_.stems.begin() + profile_.stem_count)
        {
            return std::unexpected("Unknown stem requested: " + std::string{requested});
        }
        indices.push_back(static_cast<std::size_t>(it - profile_.stems.begin()));
    }

    return indices;
}

std::expected<separation_result, std::string> model_session::separate(const audio_buffer& input, std::span<const std::string_view> stems_to_extract)
{
    if (input.channels != kExpectedChannels)
    {
        return std::unexpected("Model session expects stereo input");
    }

    if (input.sample_rate != kExpectedSampleRate)
    {
        return std::unexpected("Input sample rate does not match Demucs requirements");
    }

    const auto indices = resolve_stem_indices(stems_to_extract);
    if (!indices)
    {
        return std::unexpected(indices.error());
    }

    auto model = ensure_model_loaded();
    if (!model)
    {
        return std::unexpected(model.error());
    }

    const std::size_t frames = input.frame_count();
    Eigen::MatrixXf audio_matrix(kExpectedChannels, static_cast<int>(frames));
    for (std::size_t i = 0; i < frames; ++i)
    {
        audio_matrix(0, static_cast<int>(i)) = input.samples[i * 2];
        audio_matrix(1, static_cast<int>(i)) = input.samples[i * 2 + 1];
    }

    const auto outputs = inference_(*model.value(), audio_matrix, demucscpp::ProgressCallback{}); // Empty callback for now

    if (outputs.dimension(2) != static_cast<Eigen::Index>(frames))
    {
        return std::unexpected("Demucs output length mismatch");
    }

    separation_result result;
    result.stems.reserve(indices->size());

    for (const auto idx : *indices)
    {
        if (idx >= static_cast<std::size_t>(outputs.dimension(0)))
        {
            return std::unexpected("Demucs returned fewer stems than expected");
        }

        audio_buffer stem_buffer;
        stem_buffer.sample_rate = input.sample_rate;
        stem_buffer.channels = kExpectedChannels;
        stem_buffer.samples.resize(frames * kExpectedChannels);

        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            for (int ch = 0; ch < kExpectedChannels; ++ch)
            {
                stem_buffer.samples[frame * kExpectedChannels + ch] = outputs(idx, ch, static_cast<Eigen::Index>(frame));
            }
        }

        result.stems.emplace_back(std::string{profile_.stems[idx]}, std::move(stem_buffer));
    }

    return result;
}

} // namespace stemsmith
