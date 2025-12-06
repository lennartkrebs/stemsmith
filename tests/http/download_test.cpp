#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

#include "http/server.h"

namespace stemsmith::http
{
class server_test_hook
{
public:
    static crow::response download(server& srv, const std::string& id)
    {
        return srv.handle_download(id);
    }

    static job_registry& registry(server& srv)
    {
        return srv.registry_;
    }
};
} // namespace stemsmith::http

TEST(http_download_test, returns_not_found_for_unknown_job)
{
    using namespace stemsmith::http;
    config cfg;
    server srv(cfg);

    const auto resp = server_test_hook::download(srv, "missing");
    EXPECT_EQ(resp.code, crow::status::NOT_FOUND);
}

TEST(http_download_test, returns_conflict_if_not_completed)
{
    using namespace stemsmith::http;
    config cfg;
    server srv(cfg);

    auto& reg = server_test_hook::registry(srv);
    const auto id = reg.next_id();
    reg.add(id, stemsmith::job_handle{}, std::filesystem::path{});

    stemsmith::job_descriptor desc;
    stemsmith::job_event ev;
    ev.status = stemsmith::job_status::running;
    reg.update(id, desc, ev);

    const auto resp = server_test_hook::download(srv, id);
    EXPECT_EQ(resp.code, crow::status::CONFLICT);
}

TEST(http_download_test, returns_zip_when_completed)
{
    using namespace stemsmith::http;
    config cfg;
    server srv(cfg);

    auto& reg = server_test_hook::registry(srv);
    const auto id = reg.next_id();
    reg.add(id, stemsmith::job_handle{}, std::filesystem::path{});

    const auto tmp_dir = std::filesystem::temp_directory_path() / "stemsmith-http-zip";
    std::filesystem::create_directories(tmp_dir);
    {
        const auto file_path = tmp_dir / "test.txt";
        std::ofstream out(file_path);
        out << "hello";
    }

    stemsmith::job_descriptor desc;
    desc.output_dir = tmp_dir;

    stemsmith::job_event ev;
    ev.status = stemsmith::job_status::completed;
    reg.update(id, desc, ev);

    auto resp = server_test_hook::download(srv, id);
    EXPECT_EQ(resp.code, crow::status::OK);
    EXPECT_NE(resp.body.size(), 0u);
    EXPECT_NE(resp.get_header_value("Content-Type").find("application/zip"), std::string::npos);
}
