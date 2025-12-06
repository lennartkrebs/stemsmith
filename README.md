# stemsmith ⚒️
[![CI](https://github.com/lennartkrebs/stemsmith/actions/workflows/ci.yml/badge.svg)](https://github.com/lennartkrebs/stemsmith/actions/workflows/ci.yml)
[![Pages](https://github.com/lennartkrebs/stemsmith/actions/workflows/deploy-frontend.yml/badge.svg)](https://github.com/lennartkrebs/stemsmith/actions/workflows/deploy-frontend.yml)

Stemsmith is a C++20 library and service facade for Demucs-powered music stem separation. It wraps model caching, audio I/O, and job orchestration behind a small API that can be embedded in apps or invoked from examples. Inference is powered by the bundled demucscpp backend.

## Build
```bash
git submodule update --init --recursive
cmake -S . -B build -DENABLE_OPENMP=ON
cmake --build build --target stemsmith stemsmith_test
ctest --test-dir build --output-on-failure -R stemsmith_test
```

When a job first needs a model, the weights are downloaded into `build/model_cache` (examples trigger this automatically).

## Examples
- `simple_separation_example`: single job with progress printed to stdout.
- `observer_separation_example`: two concurrent jobs demonstrating per-job observers
- `http_server_example`: a simple HTTP server exposing a REST API for submitting separation jobs.

Build targets are created when `STEMSMITH_BUILD_EXAMPLES=ON` (default):

## API
Include the umbrella header:
```cpp
#include "stemsmith/stemsmith.h"
```
- Create a `runtime_config` with cache/output roots and optional `on_job_event`.
- Use `service::create` → `submit(job_request)` to run jobs.
- `job_request::observer` receives per-job `job_event` callbacks.

Quick start:
```cpp
#include "stemsmith/stemsmith.h"
#include <format>
#include <iostream>

int main()
{
    using namespace stemsmith;

    runtime_config cfg;
    cfg.cache.root = "build/model_cache";
    cfg.output_root = "build/output";

    auto svc = service::create(std::move(cfg));
    if (!svc) 
        return 1;

    job_request req;
    req.input_path = "data/test_files/stemsmith_demo_track.wav";
    
    auto handle = (*svc)->submit(req);
    if (!handle) 
        return 1;

    // Wait for completion
    const auto result = handle->result().get();
    if (result.status != job_status::completed) 
        return 1;
    
    std::cout << "Output at: " << result.output_dir << std::endl;
}
```
