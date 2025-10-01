#include <server.h>
#include <job_queue.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <fstream>
#include <filesystem>

// Simple HTTP client using system curl
std::string http_request(const std::string& method, const std::string& url,
                         const std::string& data = "",
                         const std::string& content_type = "application/json") {
    std::stringstream cmd;
    cmd << "curl -s -X " << method << " ";

    if (!data.empty()) {
        cmd << "-H 'Content-Type: " << content_type << "' ";
        cmd << "-d '" << data << "' ";
    }

    cmd << url << " 2>/dev/null";

    char buffer[128];
    std::string result;
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (pipe) {
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
    }
    return result;
}

void test_health_check(const std::string& base_url) {
    std::cout << "\n[TEST] Health Check Endpoint\n";
    std::cout << "==============================\n";

    auto response = http_request("GET", base_url + "/health");
    std::cout << "Response: " << response << "\n";
}

void test_job_submission(const std::string& base_url, std::string& job_id) {
    std::cout << "\n[TEST] Job Submission\n";
    std::cout << "======================\n";

    // Create test audio file
    std::filesystem::create_directories("/tmp/test_audio");
    std::ofstream test_file("/tmp/test_audio/sample.wav");
    test_file << "Fake audio data for testing";
    test_file.close();

    // Test valid job submission
    std::string valid_job = R"({
        "input_path": "/tmp/test_audio/sample.wav",
        "output_path": "/tmp/test_stems/",
        "model_name": "htdemucs",
        "mode": "fast"
    })";

    auto response = http_request("POST", base_url + "/api/jobs", valid_job);
    std::cout << "Valid job response: " << response << "\n";

    // Extract job ID from response
    try {
        auto json = nlohmann::json::parse(response);
        if (json.contains("job_id")) {
            job_id = json["job_id"];
            std::cout << "Created job ID: " << job_id << "\n";
        }
    } catch (...) {
        std::cout << "Failed to parse job response\n";
    }

    // Test invalid job (missing required fields)
    std::cout << "\nTesting invalid job submission...\n";
    std::string invalid_job = R"({
        "input_path": "/tmp/test_audio/sample.wav"
    })";

    response = http_request("POST", base_url + "/api/jobs", invalid_job);
    std::cout << "Invalid job response: " << response << "\n";
}

void test_job_status(const std::string& base_url, const std::string& job_id) {
    std::cout << "\n[TEST] Job Status Endpoint\n";
    std::cout << "===========================\n";

    if (!job_id.empty()) {
        auto response = http_request("GET", base_url + "/api/jobs/" + job_id);
        std::cout << "Job " << job_id << " status: " << response << "\n";
    }

    // Test non-existent job
    std::cout << "\nTesting non-existent job...\n";
    auto response = http_request("GET", base_url + "/api/jobs/non_existent_id");
    std::cout << "Non-existent job response: " << response << "\n";
}

void test_all_jobs_listing(const std::string& base_url) {
    std::cout << "\n[TEST] List All Jobs\n";
    std::cout << "=====================\n";

    auto response = http_request("GET", base_url + "/api/jobs");
    std::cout << "All jobs: " << response << "\n";
}

void test_file_upload(const std::string& base_url) {
    std::cout << "\n[TEST] File Upload Endpoint\n";
    std::cout << "============================\n";

    // Create a test file
    std::filesystem::create_directories("/tmp/upload_test");
    std::ofstream test_file("/tmp/upload_test/test.wav");
    test_file << "Test audio content";
    test_file.close();

    // Use curl with form data for file upload
    std::stringstream cmd;
    cmd << "curl -s -X POST ";
    cmd << "-F 'file=@/tmp/upload_test/test.wav' ";
    cmd << "-F 'model_name=htdemucs' ";
    cmd << "-F 'mode=fast' ";
    cmd << base_url << "/api/upload 2>/dev/null";

    char buffer[128];
    std::string result;
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (pipe) {
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
    }

    std::cout << "File upload response: " << result << "\n";
}

void test_websocket_connection(const std::string& ws_url) {
    std::cout << "\n[TEST] WebSocket Connection (Manual)\n";
    std::cout << "=====================================\n";
    std::cout << "To test WebSocket, run in another terminal:\n";
    std::cout << "wscat -c " << ws_url << "/ws\n";
    std::cout << "Then send: {\"action\":\"subscribe\",\"job_id\":\"<job_id>\"}\n";
}

void run_automated_tests(const std::string& base_url, const std::string& ws_url) {
    std::cout << "\n========================================\n";
    std::cout << "  AUTOMATED TEST SUITE FOR STEMSMITH\n";
    std::cout << "========================================\n";

    std::string job_id;

    // Run tests in sequence
    test_health_check(base_url);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    test_job_submission(base_url, job_id);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    test_job_status(base_url, job_id);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    test_all_jobs_listing(base_url);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    test_file_upload(base_url);

    test_websocket_connection(ws_url);

    std::cout << "\n========================================\n";
    std::cout << "  TEST SUITE COMPLETED\n";
    std::cout << "========================================\n";
}

int main() {
    using namespace stemsmith;

    // Create job queue with 2 worker threads
    const auto queue = std::make_shared<job_queue>(2);

    // Configure and start server
    server_config config;
    config.bind_address = "127.0.0.1";
    config.port = 8080;
    config.http_thread_count = 4;

    server srv(config, queue);

    // Start server in background thread
    std::thread server_thread([&srv]() {
        srv.run();
    });

    // Wait for server to start
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::string base_url = "http://" + config.bind_address + ":" +
                          std::to_string(config.port);
    std::string ws_url = "ws://" + config.bind_address + ":" +
                        std::to_string(config.port);

    std::cout << "\n╔════════════════════════════════════════╗\n";
    std::cout << "║     STEMSMITH SERVER TEST SUITE        ║\n";
    std::cout << "╚════════════════════════════════════════╝\n";
    std::cout << "\nServer running on " << base_url << "\n";
    std::cout << "WebSocket endpoint: " << ws_url << "/ws\n";

    std::cout << "\nOptions:\n";
    std::cout << "  1. Run automated tests\n";
    std::cout << "  2. Submit custom job\n";
    std::cout << "  3. Monitor server (manual testing)\n";
    std::cout << "  q. Quit\n";
    std::cout << "\nChoice: ";

    std::string choice;
    while (std::getline(std::cin, choice)) {
        if (choice == "1") {
            run_automated_tests(base_url, ws_url);
        }
        else if (choice == "2") {
            std::cout << "\nEnter input file path: ";
            std::string input_path;
            std::getline(std::cin, input_path);

            auto custom_job = std::make_shared<job>();
            custom_job->id = "custom_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
            custom_job->input_path = input_path;
            custom_job->output_path = "/tmp/custom_stems/";
            custom_job->model_name = "htdemucs";
            custom_job->mode = "fast";

            std::cout << "Submitting job " << custom_job->id << "...\n";
            queue->push(custom_job);
        }
        else if (choice == "3") {
            std::cout << "\nServer is running. You can test manually using:\n";
            std::cout << "  curl " << base_url << "/health\n";
            std::cout << "  curl -X POST " << base_url << "/api/jobs -H 'Content-Type: application/json' -d '{...}'\n";
            std::cout << "\nPress Enter to return to menu...\n";
            std::cin.get();
        }
        else if (choice == "q" || choice == "Q") {
            break;
        }

        std::cout << "\nOptions: 1=Tests, 2=Custom Job, 3=Monitor, q=Quit\n";
        std::cout << "Choice: ";
    }

    std::cout << "\nShutting down server...\n";
    srv.stop();

    if (server_thread.joinable()) {
        server_thread.join();
    }

    std::cout << "Server stopped. Goodbye!\n";

    return 0;
}