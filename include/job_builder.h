#pragma once

#include <string>
#include <string_view>
#include <atomic>
#include <vector>

namespace stemsmith
{

// New strongly typed job state enum
enum class job_state : uint8_t { queued, running, completed, failed, canceled };

struct job_parameters
{
    std::string input_path;
    std::string output_path;
    std::string model = "htdemucs";
    std::string mode = "fast";
};

struct job_builder {
    job_builder& with_input(std::string_view path) {
        params_.input_path = std::string(path);
        return *this;
    }

    job_builder& with_output(std::string_view path) {
        params_.output_path = std::string(path);
        return *this;
    }

    job_builder& with_model(std::string_view model) {
        params_.model = model;
        return *this;
    }

    job_builder& with_mode(std::string_view mode) {
        params_.mode = mode;
        return *this;
    }

    job_parameters build() const
    {
        if (params_.input_path.empty())
        {
            throw std::invalid_argument("Input path is required");
        }
        if (params_.output_path.empty())
        {
            throw std::invalid_argument("Output path is required");
        }
        return params_;
    }

private:
    job_parameters params_;
};

struct job
{

    std::string id;
    std::string input_path;
    std::string output_path;
    std::string model_name;
    std::string mode; // "hq" or "fast"

    std::atomic<job_state> state{job_state::queued};
    std::atomic<float> progress{0.0f}; // 0.0 to 1.0
    std::string error_message;
    std::vector<std::string> stems;

    [[nodiscard]] std::string status_string() const { return to_string(state.load(std::memory_order_acquire)); }

private:
    static constexpr std::string to_string(job_state s)
    {
        switch (s)
        {
        case job_state::queued: return "queued";
        case job_state::running: return "running";
        case job_state::completed: return "completed";
        case job_state::failed: return "failed";
        case job_state::canceled: return "canceled";
        }
        return "unknown";
    }
};

}