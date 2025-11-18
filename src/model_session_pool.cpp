#include "model_session_pool.h"

#include <mutex>
#include <utility>

namespace stemsmith
{

model_session_pool::model_session_pool(model_cache& cache)
    : model_session_pool(
          [&cache](model_profile_id profile_id) -> std::expected<session_ptr, std::string>
          {
              const auto profile = lookup_profile(profile_id);
              if (!profile)
              {
                  return std::unexpected("Unknown model profile id");
              }
              return std::make_unique<model_session>(*profile, cache);
          })
{
}

model_session_pool::model_session_pool(session_factory factory) : factory_(std::move(factory)) {}

model_session_pool::model_session_pool(model_session_pool&& other) noexcept
    : buckets_(std::move(other.buckets_))
    , factory_(std::move(other.factory_))
{
}

model_session_pool& model_session_pool::operator=(model_session_pool&& other) noexcept
{
    if (this != &other)
    {
        std::scoped_lock lock(mutex_, other.mutex_);
        buckets_ = std::move(other.buckets_);
        factory_ = std::move(other.factory_);
    }
    return *this;
}

model_session_pool::session_handle::~session_handle()
{
    release();
}

model_session_pool::session_handle::session_handle(session_handle&& other) noexcept
    : pool_(other.pool_)
    , profile_(other.profile_)
    , session_(std::move(other.session_))
{
    other.pool_ = nullptr;
}

model_session_pool::session_handle& model_session_pool::session_handle::operator=(session_handle&& other) noexcept
{
    if (this != &other)
    {
        release();
        pool_ = other.pool_;
        profile_ = other.profile_;
        session_ = std::move(other.session_);
        other.pool_ = nullptr;
    }
    return *this;
}

model_session_pool::session_handle::session_handle(model_session_pool* pool,
                                                   model_profile_id profile,
                                                   session_ptr session)
    : pool_(pool)
    , profile_(profile)
    , session_(std::move(session))
{
}

void model_session_pool::session_handle::release()
{
    if (pool_ && session_)
    {
        pool_->recycle(profile_, std::move(session_));
    }
    pool_ = nullptr;
}

model_session& model_session_pool::session_handle::operator*() const noexcept
{
    return *session_;
}

model_session* model_session_pool::session_handle::operator->() const noexcept
{
    return session_.get();
}

model_session* model_session_pool::session_handle::get() const noexcept
{
    return session_.get();
}

std::expected<model_session_pool::session_handle, std::string> model_session_pool::acquire(model_profile_id profile)
{
    if (!factory_)
    {
        return std::unexpected("Session pool is not configured with a factory");
    }

    session_ptr session = nullptr;
    {
        std::lock_guard lock(mutex_);
        if (auto& [idle_sessions] = buckets_[profile]; !idle_sessions.empty())
        {
            session = std::move(idle_sessions.back());
            idle_sessions.pop_back();
        }
    }

    if (!session)
    {
        auto constructed = factory_(profile);
        if (!constructed)
        {
            return std::unexpected(constructed.error());
        }
        session = std::move(constructed.value());
    }

    return session_handle(this, profile, std::move(session));
}

void model_session_pool::recycle(model_profile_id profile, session_ptr session)
{
    std::lock_guard lock(mutex_);
    buckets_[profile].idle_sessions.push_back(std::move(session));
}

} // namespace stemsmith
