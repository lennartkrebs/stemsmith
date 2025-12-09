# stemsmith ⚒️
[![CI](https://github.com/lennartkrebs/stemsmith/actions/workflows/ci.yml/badge.svg)](https://github.com/lennartkrebs/stemsmith/actions/workflows/ci.yml)
[![Pages](https://github.com/lennartkrebs/stemsmith/actions/workflows/deploy-frontend.yml/badge.svg)](https://github.com/lennartkrebs/stemsmith/actions/workflows/deploy-frontend.yml)

Are you also tired of SaaS stem splitters that meter you by the minute or cap tracks per month? Stemsmith is the “runs on your box” answer: a C++20 library and service facade for Demucs-powered music stem separation. It wraps model caching, audio I/O, and job orchestration behind a small API that can be embedded in apps or invoked from examples. Inference is powered by the bundled demucscpp backend. It may not be the fastest on every setup, but the only thing it costs you is time, not surprise invoices.

## Build
```bash
git submodule update --init --recursive
cmake -S . -B build -DENABLE_OPENMP=ON
cmake --build build --target stemsmith stemsmith_test
ctest --test-dir build --output-on-failure -R stemsmith_test
```

When a job first needs a model, the weights are downloaded into `build/model_cache` (examples trigger this automatically).

## Docker (stemsmithd + HTTP API)
Build the container (requires network for apt/CMake fetches):
```bash
DOCKER_BUILDKIT=1 docker build -t stemsmithd .
```

Run the daemon, exposing port 8345 and persisting cache/output to your host:
```bash
docker run --rm -it -p 8345:8345 -v "$HOME/.stemsmith:/root/.stemsmith" -e OMP_NUM_THREADS=4 stemsmithd --workers=4
# override port/paths/threads if needed:
# docker run --rm -it -p 9000:9000 -v "$HOME/.stemsmith:/root/.stemsmith" -e OMP_NUM_THREADS=8 stemsmithd --workers=2 --port 9000 --cache-root /root/.stemsmith/cache --output-root /root/.stemsmith/output
```

Frontend: once `stemsmithd` is running, open the GitHub Pages UI at https://lennartkrebs.github.io/stemsmith (defaults to `http://localhost:8345`, configurable in the UI). Upload a WAV, watch progress, and download stems.

**Note:** running many jobs concurrently will saturate CPU; adjust `--workers` or queue jobs accordingly.

### CPU tuning
Set `OMP_NUM_THREADS` to cap per-job threads and adjust `--workers` so `workers × OMP_NUM_THREADS` roughly matches your performance cores (to avoid oversubscription). The server defaults workers to roughly half the available hardware threads (clamped to at least 1) when `--workers` is omitted or set to 0:

- M1 Pro (8 perf cores): try `OMP_NUM_THREADS=4 --workers 2` or `OMP_NUM_THREADS=8 --workers 1`.
- M4 Max (16 perf cores): try `OMP_NUM_THREADS=4 --workers 4` or `OMP_NUM_THREADS=8 --workers 2`.

If you see slowdowns, lower `--workers` or `OMP_NUM_THREADS` to avoid oversubscribing cores.

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
