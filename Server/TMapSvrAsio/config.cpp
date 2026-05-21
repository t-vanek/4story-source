#include "config.h"

#include <toml++/toml.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <stdexcept>
#include <string_view>

namespace tmapsvr {

const char* ModeName(Mode m)
{
    switch (m) {
        case Mode::PvE: return "pve";
        case Mode::Bow: return "bow";
        case Mode::Br:  return "br";
    }
    return "?";
}

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

Mode ParseMode(std::string_view s)
{
    if (s == "pve") return Mode::PvE;
    if (s == "bow") return Mode::Bow;
    if (s == "br")  return Mode::Br;
    throw std::runtime_error("invalid mode (expected pve|bow|br): "
                             + std::string(s));
}

std::uint16_t RequirePort(std::int64_t v, const char* key)
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
            cfg.port = RequirePort(*p, "server.port");
    }
    if (auto m = tbl["mode"].as_table())
    {
        if (auto t = (*m)["type"].value<std::string>())
            cfg.mode = ParseMode(*t);
    }
    if (auto db = tbl["database"].as_table())
    {
        if (auto b = (*db)["backend"].value<std::string>())           cfg.database.backend = *b;
        if (auto c = (*db)["connection_string"].value<std::string>()) cfg.database.connection_string = *c;
        if (auto s = (*db)["pool_size"].value<std::int64_t>())
        {
            if (*s < 1 || *s > 256)
                throw std::runtime_error("database.pool_size out of range");
            cfg.database.pool_size = static_cast<std::size_t>(*s);
        }
        if (auto s = (*db)["worker_threads"].value<std::int64_t>())
        {
            if (*s < 0 || *s > 256)
                throw std::runtime_error("database.worker_threads out of range");
            cfg.database.worker_threads = static_cast<std::size_t>(*s);
        }
    }
    if (auto w = tbl["world"].as_table())
    {
        if (auto h = (*w)["host"].value<std::string>())  cfg.world.host = *h;
        if (auto p = (*w)["port"].value<std::int64_t>()) cfg.world.port = RequirePort(*p, "world.port");
    }
    if (auto h = tbl["health"].as_table())
    {
        if (auto p = (*h)["port"].value<std::int64_t>())
            cfg.health_port = RequirePort(*p, "health.port");
    }
    if (auto log = tbl["log"].as_table())
    {
        if (auto lvl = (*log)["level"].value<std::string>())
            cfg.log_level = ParseLogLevel(*lvl);
    }
    spdlog::info("loaded config from '{}' — port={} mode={} world={}:{} db={}",
        path, cfg.port, ModeName(cfg.mode),
        cfg.world.host, cfg.world.port,
        cfg.database.connection_string.empty() ? "(none)" : cfg.database.backend);
    return cfg;
}

} // namespace tmapsvr
