#include "audio_io.h"
#include <libnyquist/Common.h>
#include <libnyquist/Decoders.h>
#include <libnyquist/Encoders.h>
#include "dsp.hpp"

namespace stemsmith::audio_io
{

static Eigen::MatrixXf duplicate_or_trim_to_stereo(const std::shared_ptr<nqr::AudioData>& audio_data)
{
    const size_t frames = audio_data->samples.size() / audio_data->channelCount;
    Eigen::MatrixXf result(2, frames);
    if (audio_data->channelCount == 1) {
        for (size_t i = 0; i < frames; ++i) {
            float v = audio_data->samples[i];
            result(0, i) = v;
            result(1, i) = v;
        }
    } else if (audio_data->channelCount == 2) {
        for (size_t i = 0; i < frames; ++i) {
            result(0, i) = audio_data->samples[2 * i];
            result(1, i) = audio_data->samples[2 * i + 1];
        }
    } else {
        throw std::runtime_error("Only mono or stereo sources supported.");
    }
    return result;
}

std::vector<float> resample_channel(const float* input, size_t num_frames, int input_samplerate, int output_samplerate, int quality)
{
    if (input_samplerate == output_samplerate)
    {
        return std::vector(input, input + num_frames);
    }

    const double src_ratio = static_cast<double>(output_samplerate) / static_cast<double>(input_samplerate);
    const size_t output_frames = static_cast<size_t>(num_frames * src_ratio) + 1;

    std::vector<float> output(output_frames);

    SRC_DATA src_data;
    src_data.data_in = input;
    src_data.input_frames = static_cast<long>(num_frames);
    src_data.data_out = output.data();
    src_data.output_frames = static_cast<long>(output_frames);
    src_data.src_ratio = src_ratio;
    src_data.end_of_input = 1;

    if (const int error = src_simple(&src_data, quality, 1))
    {
        throw std::runtime_error("Error during resampling: " + std::string(src_strerror(error)));
    }

    output.resize(src_data.output_frames_gen);
    return output;
}

Eigen::MatrixXf load_stereo(std::string_view path)
{
    using namespace nqr;

    const auto data = std::make_shared<AudioData>();
    NyquistIO io;
    io.Load(data.get(), path.data());

    if (data->samples.empty())
    {
        throw std::runtime_error("Failed to decode audiofile: " + std::string(path));
    }

    const auto stereo = duplicate_or_trim_to_stereo(data);

    if (data->sampleRate == demucscpp::SUPPORTED_SAMPLE_RATE)
    {
        return stereo;
    }

    const auto ch0 = resample_channel(stereo.row(0).data(), static_cast<size_t>(stereo.cols()), data->sampleRate, demucscpp::SUPPORTED_SAMPLE_RATE);
    const auto ch1 = resample_channel(stereo.row(1).data(), static_cast<size_t>(stereo.cols()), data->sampleRate, demucscpp::SUPPORTED_SAMPLE_RATE);

    if (ch0.size() != ch1.size())
    {
        throw std::runtime_error("Resampled channel size mismatch.");
    }

    Eigen::MatrixXf out(2, ch0.size());
    out.row(0) = Eigen::VectorXf::Map(ch0.data(), ch0.size());
    out.row(1) = Eigen::VectorXf::Map(ch1.data(), ch1.size());
    return out;
}

void write_stereo(const Eigen::MatrixXf& wav, std::string_view path)
{
    using namespace nqr;
    const auto audio_data = std::make_shared<AudioData>();
    constexpr auto channel_count = 2;

    audio_data->sampleRate = demucscpp::SUPPORTED_SAMPLE_RATE;
    audio_data->channelCount = channel_count;
    audio_data->samples.resize(wav.cols() * channel_count);

    for (size_t i = 0; i < static_cast<size_t>(wav.cols()); ++i)
    {
        audio_data->samples[2 * i] = wav(0, i);
        audio_data->samples[2 * i + 1] = wav(1, i);
    }

    if (const int status = encode_wav_to_disk({channel_count, PCM_FLT, DITHER_TRIANGLE}, audio_data.get(), path.data()); status != 0)
    {
        throw std::runtime_error("Failed to encode wav file: " + std::to_string(status));
    }
}


}

