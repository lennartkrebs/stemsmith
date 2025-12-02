/**
 * @file job_result.h
 * @brief Types representing job lifecycle artefacts such as handles, observers,
 *        and completed results.
 *
 * End users interact with ::stemsmith::job_handle and ::stemsmith::job_result
 * returned from ::stemsmith::service::submit.
 */
#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>

namespace stemsmith
{

enum class job_status
{
    queued,
    running,
    completed,
    failed,
    cancelled
};

struct job_descriptor;
struct job_event;

struct job_result
{
    std::filesystem::path input_path;
    std::filesystem::path output_dir;
    job_status status{job_status::queued};
    std::optional<std::string> error{};
};

struct job_observer
{
    std::function<void(const job_descriptor&, const job_event&)> callback{};
};

class worker_pool;
struct job_handle_state;

class job_handle
{
public:
    job_handle();

    [[nodiscard]] std::size_t id() const noexcept;
    [[nodiscard]] const job_descriptor& descriptor() const;
    [[nodiscard]] std::shared_future<job_result> result() const;
    [[nodiscard]] std::expected<void, std::string> cancel(std::string reason = {}) const;
    void set_observer(job_observer observer) const;

private:
    explicit job_handle(std::shared_ptr<job_handle_state> state);
    std::shared_ptr<job_handle_state> state_;

    friend class job_runner;
};
} // namespace stemsmith
