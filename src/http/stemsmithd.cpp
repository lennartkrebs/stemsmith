#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "server.h"

namespace
{
struct options
{
    std::string bind_address{"0.0.0.0"};
    std::uint16_t port{8345};
    std::filesystem::path cache_root{};
    std::filesystem::path output_root{};
    std::size_t workers{std::thread::hardware_concurrency()};
    bool help{false};
};

std::filesystem::path default_root()
{
    if (const auto env = std::getenv("STEMSMITH_HOME"))
    {
        return std::filesystem::path{env};
    }
    if (const auto home = std::getenv("HOME"))
    {
        return std::filesystem::path{home} / ".stemsmith";
    }
    return std::filesystem::current_path() / ".stemsmith";
}

void print_usage(const char* argv0)
{
    std::cout << "Usage: " << argv0 << " [--bind-address ADDR] [--port PORT] [--cache-root PATH] [--output-root PATH]\n"
              << "             [--workers N]\n\n"
              << "Defaults: bind 0.0.0.0, port 8345, paths under $HOME/.stemsmith (or $STEMSMITH_HOME), workers = HW "
                 "threads.\n";
}

std::optional<options> parse_args(int argc, char* argv[])
{
    options opts{};
    const auto root = default_root();
    opts.cache_root = root / "cache";
    opts.output_root = root / "output";

    auto parse_value = [](std::string_view arg, std::string_view name) -> std::optional<std::string_view>
    {
        if (arg == name)
        {
            return std::nullopt; // value should follow
        }
        std::string prefix{name};
        prefix.push_back('=');
        if (arg.rfind(prefix, 0) == 0)
        {
            return arg.substr(prefix.size());
        }
        return {};
    };

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg{argv[i]};
        if (arg == "--help" || arg == "-h")
        {
            opts.help = true;
            return opts;
        }

        if (auto v = parse_value(arg, "--bind-address"))
        {
            if (v->empty())
            {
                if (i + 1 >= argc)
                {
                    std::cerr << "Missing value for --bind-address\n";
                    return std::nullopt;
                }
                opts.bind_address = argv[++i];
            }
            else
            {
                opts.bind_address = *v;
            }
            continue;
        }

        if (auto v = parse_value(arg, "--port"))
        {
            std::string value;
            if (v->empty())
            {
                if (i + 1 >= argc)
                {
                    std::cerr << "Missing value for --port\n";
                    return std::nullopt;
                }
                value = argv[++i];
            }
            else
            {
                value = std::string{*v};
            }
            try
            {
                const auto parsed = std::stoi(value);
                if (parsed <= 0 || parsed > 65535)
                {
                    std::cerr << "Port must be between 1 and 65535\n";
                    return std::nullopt;
                }
                opts.port = static_cast<std::uint16_t>(parsed);
            }
            catch (const std::exception& ex)
            {
                std::cerr << "Invalid port: " << ex.what() << "\n";
                return std::nullopt;
            }
            continue;
        }

        if (auto v = parse_value(arg, "--cache-root"))
        {
            if (v->empty())
            {
                if (i + 1 >= argc)
                {
                    std::cerr << "Missing value for --cache-root\n";
                    return std::nullopt;
                }
                opts.cache_root = argv[++i];
            }
            else
            {
                opts.cache_root = std::filesystem::path{*v};
            }
            continue;
        }

        if (auto v = parse_value(arg, "--output-root"))
        {
            if (v->empty())
            {
                if (i + 1 >= argc)
                {
                    std::cerr << "Missing value for --output-root\n";
                    return std::nullopt;
                }
                opts.output_root = argv[++i];
            }
            else
            {
                opts.output_root = std::filesystem::path{*v};
            }
            continue;
        }

        if (auto v = parse_value(arg, "--workers"))
        {
            std::string value;
            if (v->empty())
            {
                if (i + 1 >= argc)
                {
                    std::cerr << "Missing value for --workers\n";
                    return std::nullopt;
                }
                value = argv[++i];
            }
            else
            {
                value = std::string{*v};
            }
            try
            {
                const auto parsed = std::stoul(value);
                opts.workers = parsed == 0 ? opts.workers : parsed;
            }
            catch (const std::exception& ex)
            {
                std::cerr << "Invalid workers value: " << ex.what() << "\n";
                return std::nullopt;
            }
            continue;
        }

        std::cerr << "Unknown argument: " << arg << "\n";
        return std::nullopt;
    }

    return opts;
}

namespace
{
volatile std::sig_atomic_t g_signal_status;
}

void signal_handler(int signal)
{
    g_signal_status = signal;
}

} // namespace

int main(int argc, char* argv[])
{
    const auto parsed = parse_args(argc, argv);
    if (!parsed)
    {
        print_usage(argv[0]);
        return 1;
    }
    if (parsed->help)
    {
        print_usage(argv[0]);
        return 0;
    }

    const auto ensure_dir = [](const std::filesystem::path& path, const char* label)
    {
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        if (ec)
        {
            std::cerr << "Failed to create " << label << " directory at " << path << ": " << ec.message() << "\n";
            std::exit(1);
        }
    };

    ensure_dir(parsed->cache_root, "cache");
    ensure_dir(parsed->output_root, "output");

    stemsmith::http::config cfg;
    cfg.bind_address = parsed->bind_address;
    cfg.port = parsed->port;
    cfg.cache_root = parsed->cache_root;
    cfg.output_root = parsed->output_root;
    cfg.worker_count = parsed->workers;

    stemsmith::http::server srv(cfg);
    srv.start();

    std::cout << "stemsmithd listening on " << cfg.bind_address << ":" << cfg.port << "\n";
    std::cout << "cache_root=" << cfg.cache_root << "\n";
    std::cout << "output_root=" << cfg.output_root << "\n";
    std::cout << "workers=" << cfg.worker_count << "\n";
    std::cout << "Press Ctrl+C to stop\n";

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    while (!g_signal_status)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    srv.stop();
    return 0;
}
