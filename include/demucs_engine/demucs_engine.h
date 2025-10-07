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

struct demucs_engine
{
    using progress_callback_t = std::function<void(const separation_progress&)>;
    virtual ~demucs_engine() = default;
    virtual void separate(job& job, progress_callback_t progress_callback) = 0;
};

struct demucs_engine_config
{
    std::string weights_path;
    bool preload_model = true;
};

}