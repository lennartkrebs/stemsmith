#pragma once

#include "stemsmith/weight_fetcher.h"

#include <chrono>

namespace stemsmith
{
class http_weight_fetcher final : public weight_fetcher
{
  public:
    explicit http_weight_fetcher(std::chrono::seconds timeout = std::chrono::seconds{30});
    ~http_weight_fetcher() override;

    std::expected<void, std::string> fetch_weights(std::string_view url,
                                                   const std::filesystem::path& destination,
                                                   progress_callback progress = {}) override;

  private:
    std::chrono::seconds timeout_;
};
} // namespace stemsmith
