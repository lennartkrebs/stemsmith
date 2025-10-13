#pragma once

#include <string>
#include <Eigen/Core>
#include <samplerate.h>

namespace stemsmith::audio_io {
std::vector<float> resample_channel(const float* input, size_t num_frames, int input_samplerate, int output_samplerate, int quality = SRC_SINC_FASTEST);
Eigen::MatrixXf load_stereo(std::string_view path);
void write_stereo(const Eigen::MatrixXf& wav, std::string_view path);
} // namespace stemsmith::audio_io
