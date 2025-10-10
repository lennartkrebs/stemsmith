#pragma once

#include "model.hpp"

namespace stemsmith
{

struct cached_model_set
{
    bool is_fine_tuned = false;
    int num_sources = 4; // 4 or 6
    std::array<demucscpp::demucs_model, 4> models;
};

class demucs_model_cache
{
public:
    explicit demucs_model_cache(size_t capacity);

    using handle = std::shared_ptr<cached_model_set>;
    using loader = std::function<void(cached_model_set&)>;

    handle get_or_load(std::string_view key, bool is_fine_tuned, loader loader);

private:
    size_t capacity_ = 2;
};


} // namespace stemsmith