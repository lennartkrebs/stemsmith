#pragma once

#include <cstddef>
#include <vector>

namespace stemsmith
{
struct audio_buffer
{
    int sample_rate{};
    std::size_t channels{};
    std::vector<float> samples; // interleaved PCM data in [-1, 1]

    [[nodiscard]] std::size_t frame_count() const noexcept
    {
        return channels == 0 ? 0 : samples.size() / channels;
    }

    [[nodiscard]] bool empty() const noexcept { return samples.empty(); }
};
} // namespace stemsmith
