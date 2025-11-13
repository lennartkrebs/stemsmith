#include "stemsmith/audio_io.h"

#include <libnyquist/Common.h>
#include <libnyquist/Decoders.h>
#include <libnyquist/Encoders.h>
#include <samplerate.h>

#include <expected>
#include <filesystem>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#if defined(__clang__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#include "dsp.hpp"
#if defined(__clang__)
#    pragma clang diagnostic pop
#endif

namespace
{
using stemsmith::audio_buffer;
using stemsmith::audio_format;

constexpr int TARGET_NUM_CHANNELS = 2;
constexpr int TARGET_SAMPLE_RATE = demucscpp::SUPPORTED_SAMPLE_RATE;

std::expected<std::vector<float>, std::string> ensure_supported_channels(const nqr::AudioData& data)
{
    if (data.channelCount == TARGET_NUM_CHANNELS)
    {
        return data.samples;
    }

    if (data.channelCount == 1)
    {
        const auto frames = data.samples.size();
        std::vector<float> stereo;
        stereo.reserve(frames * TARGET_NUM_CHANNELS);
        for (const auto sample : data.samples)
        {
            stereo.push_back(sample);
            stereo.push_back(sample);
        }
        return stereo;
    }

    return std::unexpected("Only mono or stereo inputs are supported");
}

std::expected<std::vector<float>, std::string> resample_if_needed(const std::vector<float>& samples, std::size_t channels, int source_rate, int target_rate)
{
    if (source_rate == target_rate || samples.empty())
    {
        return samples;
    }

    if (source_rate <= 0 || target_rate <= 0)
    {
        return std::unexpected("Invalid sample rate");
    }

    SRC_DATA request{};
    request.data_in = samples.data();

    const auto input_frames = samples.size() / channels;
    const double ratio = static_cast<double>(target_rate) / static_cast<double>(source_rate);
    const auto max_output_frames = static_cast<std::size_t>(std::ceil(input_frames * ratio)) + 8;

    std::vector<float> output(max_output_frames * channels);

    request.data_out = output.data();
    request.src_ratio = ratio;
    request.input_frames = static_cast<long>(input_frames);
    request.output_frames = static_cast<long>(max_output_frames);
    request.end_of_input = 1;

    if (const int result = src_simple(&request, SRC_SINC_BEST_QUALITY, static_cast<int>(channels)); result != 0)
    {
        return std::unexpected(src_strerror(result));
    }

    output.resize(static_cast<std::size_t>(request.output_frames_gen) * channels);
    return output;
}
} // namespace

namespace stemsmith
{
std::expected<audio_buffer, std::string> load_audio_file(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path))
    {
        return std::unexpected("Audio file does not exist: " + path.string());
    }

    const auto file_data = std::make_shared<nqr::AudioData>();

    nqr::NyquistIO loader;
    loader.Load(file_data.get(), path.string());

    if (file_data->samples.empty())
    {
        return std::unexpected("Failed to load audio");
    }

    if (file_data->channelCount <= 0)
    {
        return std::unexpected("Input file has no channels");
    }

    auto samples = ensure_supported_channels(*file_data);
    if (!samples)
    {
        return std::unexpected(samples.error());
    }

    const auto resampled = resample_if_needed(*samples, TARGET_NUM_CHANNELS, file_data->sampleRate, TARGET_SAMPLE_RATE);
    if (!resampled)
    {
        return std::unexpected(resampled.error());
    }

    audio_buffer buffer;
    buffer.sample_rate = TARGET_SAMPLE_RATE;
    buffer.channels = TARGET_NUM_CHANNELS;
    buffer.samples = std::move(resampled.value());

    return buffer;
}

std::expected<void, std::string> write_audio_file(const std::filesystem::path& path, const audio_buffer& buffer, audio_format format)
{
    if (format != audio_format::wav)
    {
        return std::unexpected("Only WAV outputs are supported");
    }

    if (buffer.channels != TARGET_NUM_CHANNELS)
    {
        return std::unexpected("Audio writer expects stereo PCM data");
    }

    if (const auto parent = path.parent_path(); !parent.empty())
    {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec)
        {
            return std::unexpected("Failed to create output directory: " + ec.message());
        }
    }

    const auto file_data = std::make_shared<nqr::AudioData>();
    assert(file_data);
    file_data->sampleRate = buffer.sample_rate;
    file_data->channelCount = static_cast<int>(buffer.channels);
    file_data->samples = buffer.samples;

    const int status = encode_wav_to_disk(
        {file_data->channelCount, nqr::PCM_FLT, nqr::DITHER_TRIANGLE},
        file_data.get(),
        path.string());

    if (status != nqr::EncoderError::NoError)
    {
        return std::unexpected("Failed to write audio: " + std::to_string(status));
    }

    return {};
}
} // namespace stemsmith
