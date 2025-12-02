#pragma once

#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "model_cache.h"
#include "model_session.h"
#include "stemsmith/job_config.h"

namespace stemsmith
{

class model_session_pool
{
public:
    using session_ptr = std::unique_ptr<model_session>;
    using session_factory = std::function<std::expected<session_ptr, std::string>(model_profile_id)>;

    explicit model_session_pool(model_cache& cache);
    explicit model_session_pool(session_factory factory);

    model_session_pool(model_session_pool&& other) noexcept;
    model_session_pool& operator=(model_session_pool&& other) noexcept;

    class session_handle
    {
    public:
        session_handle() = default;
        ~session_handle();

        session_handle(session_handle&& other) noexcept;
        session_handle& operator=(session_handle&& other) noexcept;

        session_handle(const session_handle&) = delete;
        session_handle& operator=(const session_handle&) = delete;

        model_session& operator*() const noexcept;
        model_session* operator->() const noexcept;
        [[nodiscard]] model_session* get() const noexcept;

    private:
        friend class model_session_pool;
        session_handle(model_session_pool* pool, model_profile_id profile, session_ptr session);
        void release();

        model_session_pool* pool_{};
        model_profile_id profile_{};
        session_ptr session_;
    };

    [[nodiscard]] std::expected<session_handle, std::string> acquire(model_profile_id profile);

private:
    /**
     * @brief Bucket of idle sessions for a specific model profile.
     */
    struct bucket
    {
        std::vector<session_ptr> idle_sessions;
    };

    void recycle(model_profile_id profile, session_ptr session);

    std::mutex mutex_; // broad mutex for protecting access to buckets
    std::map<model_profile_id, bucket> buckets_;
    session_factory factory_;
};

} // namespace stemsmith
