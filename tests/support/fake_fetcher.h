#pragma once
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "stemsmith/weight_fetcher.h"

namespace stemsmith::test
{

class fake_fetcher final : public weight_fetcher
{
public:
    explicit fake_fetcher(std::string_view payload) : payload_(payload) {}

    std::expected<void, std::string> fetch_weights(std::string_view /*url*/,
                                                   const std::filesystem::path& destination,
                                                   progress_callback progress = {}) override
    {
        ++call_count;
        std::ofstream out(destination, std::ios::binary);
        out << payload_;
        out.close();
        if (progress)
        {
            progress(payload_.size(), payload_.size());
        }
        return {};
    }

    size_t call_count{0};

private:
    std::string payload_{};
};

} // namespace stemsmith::test
