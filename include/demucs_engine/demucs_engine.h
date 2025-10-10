#pragma once

#include <string>
#include <functional>

namespace stemsmith
{

struct job;

struct separation_progress
{
    float progress = 0.0f;
    std::string current_stage;
};

struct demucs_engine_config
{
    std::string weights_path;
    bool preload_model = true;
};

class demucs_engine
{
public:
    using progress_callback_t = std::function<void(const separation_progress&)>;

    explicit demucs_engine(demucs_engine_config config);
    ~demucs_engine();
    void separate(job& job, progress_callback_t progress_callback);

private:
    static constexpr std::array STEM_NAMES = {"vocals", "drums", "bass", "other", "guitar", "piano"};
};

} // namespace stemsmith