#include "http/server.h"

#include <chrono>
#include <curl/curl.h>
#include <filesystem>
#include <iostream>
#include <thread>

using stemsmith::http::config;
using stemsmith::http::server;

std::optional<std::string> post_job(const std::string& url, const std::filesystem::path& wav)
{
    CURL* curl = curl_easy_init();
    if (!curl)
        return std::nullopt;

    struct curl_httppost* form = nullptr;
    struct curl_httppost* last = nullptr;
    curl_formadd(&form,
                 &last,
                 CURLFORM_COPYNAME,
                 "file",
                 CURLFORM_FILE,
                 wav.string().c_str(),
                 CURLFORM_CONTENTTYPE,
                 "audio/wav",
                 CURLFORM_END);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, form);

    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
        const auto total = size * nmemb;
        auto* out = static_cast<std::string*>(userdata);
        out->append(ptr, total);
        return total;
    });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    if (const auto rc = curl_easy_perform(curl); rc != CURLE_OK)
    {
        std::cerr << "POST failed: " << curl_easy_strerror(rc) << "\n";
        curl_formfree(form);
        curl_easy_cleanup(curl);
        return std::nullopt;
    }

    curl_formfree(form);
    curl_easy_cleanup(curl);
    return response;
}

namespace
{
constexpr auto kExampleTrack = "data/test_files/stemsmith_demo_track.wav";
#ifdef STEMSMITH_SOURCE_DIR
const std::filesystem::path kExampleTrackPath = std::filesystem::path{STEMSMITH_SOURCE_DIR} / kExampleTrack;
#endif
}

int main()
{
    if (!std::filesystem::exists(kExampleTrackPath))
    {
        std::cerr << "Missing demo wav at " << kExampleTrackPath << "\n";
        return 1;
    }

    config cfg;
    cfg.bind_address = "127.0.0.1";
    cfg.port = 8350;
    cfg.cache_root = "build/model_cache";
    cfg.output_root = "build/output_http_example";

    server srv(cfg);
    srv.start();

    // Give the server a moment to start up
    std::this_thread::sleep_for(std::chrono::seconds(1));

    const auto post_response = post_job("http://127.0.0.1:8350/jobs", kExampleTrackPath);
    if (!post_response)
    {
        std::cerr << "Failed to submit job\n";
        srv.stop();
        return 1;
    }

    std::cout << "POST /jobs response: " << *post_response << "\n";
    std::cout << "Server running at http://127.0.0.1:8350\n";
    std::cout << "Try GET /jobs/{id} and /jobs/{id}/download from another terminal.\n";
    std::cout << "Press Enter to stop the server once you're done.\n";

    std::cin.get();
    srv.stop();
    return 0;
}
