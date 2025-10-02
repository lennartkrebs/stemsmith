#include "job_manager.h"

namespace stemsmith {

job_manager::job_manager(size_t worker_threads)
    : queue_(std::make_unique<job_queue>(worker_threads))
{

}


} // namespace stemsmith