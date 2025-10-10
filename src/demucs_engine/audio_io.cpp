#include "audio_io.h"
#include <libnyquist/Common.h>
#include <libnyquist/Decoders.h>
#include <libnyquist/Encoders.h>

namespace stemsmith::audio_io
{

std::vector<float> resample_channel(const float* input, size_t num_frames, int input_samplerate, int output_samplerate, int quality)
{
    if (input_samplerate == output_samplerate)
    {
        return std::vector<float>(input, input + num_frames);
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

    if (int error = src_simple(&src_data, quality, 1))
    {
        throw std::runtime_error("Error during resampling: " + std::string(src_strerror(error)));
    }

    output.resize(src_data.output_frames_gen);
    return output;
}

}