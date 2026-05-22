#include "config.h"

#include <toml++/toml.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <stdexcept>

namespace tworldsvr {

namespace {
spdlog::level::level_enum ParseLogLevel(std::string_view s)
{
    if (s == "trace")    return spdlog::level::trace;
    if (s == "debug")    return spdlog::level::debug;
    if (s == "info")     return spdlog::level::info;
    if (s == "warn")     return spdlog::level::warn;
    if (s == "error")    return spdlog::level::err;
    if (s == "critical") return spdlog::level::critical;
    if (s == "off")      return spdlog::level::off;
    throw std::runtime_error("invalid log level: " + std::string(s));
}

std::uint16_t Port(std::int64_t v, const char* key)
{
    if (v < 0 || v > 65535)
        throw std::runtime_error(std::string(key) + " out of range");
    return static_cast<std::uint16_t>(v);
}
} // namespace

AppConfig LoadConfig(const std::string& path)
{
    AppConfig cfg;
    if (path.empty() || !std::filesystem::exists(path))
    {
        spdlog::info("no config file at '{}'; using defaults", path);
        return cfg;
    }
    toml::table tbl;
    try { tbl = toml::parse_file(path); }
    catch (const toml::parse_error& ex) {
        const auto d = ex.description();
        throw std::runtime_error("TOML parse error in '" + path + "': "
            + std::string(d.data(), d.size()));
    }

    if (auto srv = tbl["server"].as_table())
    {
        if (auto p = (*srv)["port"].value<std::int64_t>())
            cfg.port = Port(*p, "server.port");
        if (auto m = (*srv)["max_connections"].value<std::int64_t>())
        {
            if (*m < 0 || *m > 0xFFFFFFFFLL)
                throw std::runtime_error("server.max_connections out of range");
            cfg.max_connections = static_cast<std::uint32_t>(*m);
        }
        if (auto n = (*srv)["nation"].value<std::int64_t>())
        {
            if (*n < 0 || *n > 255)
                throw std::runtime_error("server.nation out of range (0..255)");
            cfg.nation = static_cast<std::uint8_t>(*n);
        }
    }
    if (auto db = tbl["database"].as_table())
    {
        if (auto v = (*db)["backend"].value<std::string>())
            cfg.database.backend = *v;
        if (auto v = (*db)["connection_string"].value<std::string>())
            cfg.database.connection_string = *v;
        if (auto v = (*db)["pool_size"].value<std::int64_t>())
        {
            if (*v <= 0) throw std::runtime_error("database.pool_size must be > 0");
            cfg.database.pool_size = static_cast<std::size_t>(*v);
        }
        if (auto v = (*db)["worker_threads"].value<std::int64_t>())
        {
            if (*v < 0) throw std::runtime_error("database.worker_threads must be >= 0");
            cfg.database.worker_threads = static_cast<std::size_t>(*v);
        }
    }
    if (auto h = tbl["health"].as_table())
    {
        if (auto p = (*h)["port"].value<std::int64_t>())
            cfg.health_port = Port(*p, "health.port");
    }
    if (auto lg = tbl["log"].as_table())
    {
        if (auto s = (*lg)["level"].value<std::string>())
            cfg.log_level = ParseLogLevel(*s);
    }

    return cfg;
}

} // namespace tworldsvr
