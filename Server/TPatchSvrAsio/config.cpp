#include "config.h"

#include <toml++/toml.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <stdexcept>

namespace tpatchsvr {

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
        {
            if (*p < 0 || *p > 65535)
                throw std::runtime_error("server.port out of range");
            cfg.port = static_cast<std::uint16_t>(*p);
        }
    }
    if (auto p = tbl["patch"].as_table())
    {
        if (auto s = (*p)["ftp_url"].value<std::string>())     cfg.ftp_url = *s;
        if (auto s = (*p)["pre_ftp_url"].value<std::string>()) cfg.pre_ftp_url = *s;
    }
    if (auto l = tbl["login"].as_table())
    {
        if (auto h = (*l)["host"].value<std::string>())        cfg.login_host = *h;
        if (auto p = (*l)["port"].value<std::int64_t>())
        {
            if (*p < 0 || *p > 65535)
                throw std::runtime_error("login.port out of range");
            cfg.login_port = static_cast<std::uint16_t>(*p);
        }
    }
    if (auto db = tbl["database"].as_table())
    {
        if (auto b = (*db)["backend"].value<std::string>())          cfg.database.backend = *b;
        if (auto c = (*db)["connection_string"].value<std::string>()) cfg.database.connection_string = *c;
        if (auto s = (*db)["pool_size"].value<std::int64_t>())
        {
            if (*s < 1 || *s > 256)
                throw std::runtime_error("database.pool_size out of range");
            cfg.database.pool_size = static_cast<std::size_t>(*s);
        }
    }
    if (auto h = tbl["health"].as_table())
    {
        if (auto p = (*h)["port"].value<std::int64_t>())
        {
            if (*p < 0 || *p > 65535) throw std::runtime_error("health.port out of range");
            cfg.health_port = static_cast<std::uint16_t>(*p);
        }
    }
    if (auto log = tbl["log"].as_table())
    {
        if (auto lvl = (*log)["level"].value<std::string>())
            cfg.log_level = ParseLogLevel(*lvl);
    }
    spdlog::info("loaded config from '{}' — port={} ftp='{}' login={}:{} db={}",
        path, cfg.port, cfg.ftp_url, cfg.login_host, cfg.login_port,
        cfg.database.connection_string.empty() ? "(none)" : cfg.database.backend);
    return cfg;
}

} // namespace tpatchsvr
