#pragma once

#include <cstddef>
#include <thread>

namespace stemsmith
{

class worker_pool
{
public:
    explicit worker_pool(std::size_t max_concurrent_workers = std::thread::hardware_concurrency());

};


} // namespace stemsmith