#pragma once

#include <string>
#include <Eigen/Core>
#include <samplerate.h>

namespace stemsmith::audio_io {
std::vector<float> resample_channel(const float* input, size_t num_frames, int input_samplerate, int output_samplerate, int quality = SRC_SINC_FASTEST);
Eigen::MatrixXf load_stereo(const std::string& path, int expected_sr);
void write_stereo(const Eigen::MatrixXf& wav, const std::string& path, int sr);
} // namespace stemsmith::audio_io
