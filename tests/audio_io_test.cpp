#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <libnyquist/Common.h>
#include <libnyquist/Decoders.h>
#include <libnyquist/Encoders.h>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "audio_io.h"

namespace
{
struct temp_dir
{
    temp_dir()
    {
        const auto base = std::filesystem::temp_directory_path();
        path = base / std::filesystem::path("stemsmith-audio-io-test");
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
    }

    ~temp_dir()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    std::filesystem::path path;
};

std::filesystem::path write_fixture_wav(const temp_dir& dir, int sample_rate, int channels, std::size_t frames)
{
    const auto data = std::make_shared<nqr::AudioData>();
    data->sampleRate = sample_rate;
    data->channelCount = channels;
    data->samples.resize(frames * channels);
    std::iota(data->samples.begin(), data->samples.end(), 0.0f);

    const auto file = dir.path / "input.wav";
    const auto result = encode_wav_to_disk({channels, nqr::PCM_FLT, nqr::DITHER_TRIANGLE}, data.get(), file.string());
    EXPECT_EQ(result, 0);
    return file;
}

void expect_wav_file(const std::filesystem::path& path, int expected_rate, int expected_channels)
{
    nqr::NyquistIO loader;
    const auto data = std::make_shared<nqr::AudioData>();
    loader.Load(data.get(), path.string());
    EXPECT_EQ(data->sampleRate, expected_rate);
    EXPECT_EQ(data->channelCount, expected_channels);
}
} // namespace

namespace stemsmith
{
TEST(audio_io_test, loads_and_resamples_wav_to_target_rate)
{
    const temp_dir dir;
    const auto wav_path = write_fixture_wav(dir, 48000, 1, 480);

    const auto buffer = load_audio_file(wav_path);
    ASSERT_TRUE(buffer.has_value());
    EXPECT_EQ(buffer->sample_rate, 44100);
    EXPECT_EQ(buffer->channels, 2U);
    EXPECT_FALSE(buffer->samples.empty());
    EXPECT_EQ(buffer->samples.size() % buffer->channels, 0U);
    EXPECT_NEAR(static_cast<double>(buffer->frame_count()), static_cast<double>(480) * (44100.0 / 48000.0), 2.0);
    EXPECT_FLOAT_EQ(buffer->samples[0], buffer->samples[1]);
}

TEST(audio_io_test, writes_stereo_wav_files)
{
    const temp_dir dir;
    audio_buffer buffer;
    buffer.sample_rate = 44100;
    buffer.channels = 2;
    buffer.samples.reserve(64);
    for (int i = 0; i < 32; ++i)
    {
        const float sample = static_cast<float>(i) / 32.0f;
        buffer.samples.push_back(sample);
        buffer.samples.push_back(sample);
    }

    const auto path = dir.path / "output" / "stem.wav";
    const auto status = write_audio_file(path, buffer);
    ASSERT_TRUE(status.has_value());

    expect_wav_file(path, 44100, 2);
}

TEST(audio_io_test, write_audio_file_fails_with_tiny_buffer)
{
    const temp_dir dir;
    audio_buffer buffer;
    buffer.sample_rate = 44100;
    buffer.channels = 2;
    buffer.samples = {0.0f, 0.0f};

    const auto status = write_audio_file(dir.path / "tiny.wav", buffer);
    ASSERT_FALSE(status.has_value());
    EXPECT_NE(status.error().find("Failed to write audio"), std::string::npos);
}
} // namespace stemsmith
