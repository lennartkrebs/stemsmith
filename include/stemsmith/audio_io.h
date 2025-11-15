#pragma once

#include "stemsmith/audio_buffer.h"

#include <expected>
#include <filesystem>
#include <string>

namespace stemsmith
{

enum class audio_format
{
    wav
};

[[nodiscard]] std::expected<audio_buffer, std::string> load_audio_file(const std::filesystem::path& path);
[[nodiscard]] std::expected<void, std::string> write_audio_file(const std::filesystem::path& path, const audio_buffer& buffer, audio_format format = audio_format::wav);
} // namespace stemsmith
