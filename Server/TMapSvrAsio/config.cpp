#include "config.h"

#include <toml++/toml.hpp>
#include <spdlog/spdlog.h>

#include <cstddef>
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

// Decode an even-length hex string into raw bytes. Matches the helper
// in TLoginSvrAsio/config.cpp — operator-supplied bytes are passed
// through verbatim with no implicit NUL append.
std::vector<std::byte> ParseHexBytes(std::string_view hex)
{
    if (hex.size() % 2 != 0)
        throw std::runtime_error("hex string must have even length");
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        throw std::runtime_error(std::string("invalid hex char: ") + c);
    };
    std::vector<std::byte> out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2)
        out.push_back(static_cast<std::byte>((nibble(hex[i]) << 4) | nibble(hex[i + 1])));
    return out;
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
            cfg.server.port = RequirePort(*p, "server.port");
        if (auto m = (*srv)["max_connections"].value<std::int64_t>())
        {
            if (*m < 1 || *m > 100000)
                throw std::runtime_error("server.max_connections out of range");
            cfg.server.max_connections = static_cast<std::uint32_t>(*m);
        }
    }
    if (auto crypto = tbl["crypto"].as_table())
    {
        if (auto hex = (*crypto)["rc4_secret_hex"].value<std::string>())
            cfg.server.rc4_secret_key = ParseHexBytes(*hex);
        if (auto disable = (*crypto)["disable"].value<bool>())
        {
            if (*disable) cfg.server.rc4_secret_key.clear();
        }
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
    if (auto a = tbl["audit"].as_table())
    {
        if (auto h = (*a)["host"].value<std::string>())  cfg.audit.host = *h;
        if (auto p = (*a)["port"].value<std::int64_t>()) cfg.audit.port = RequirePort(*p, "audit.port");
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
    spdlog::info("loaded config from '{}' — port={} mode={} crypto={} world={}:{} db={}",
        path, cfg.server.port, ModeName(cfg.mode),
        cfg.server.rc4_secret_key.empty() ? "off" : "on",
        cfg.world.host, cfg.world.port,
        cfg.database.connection_string.empty() ? "(none)" : cfg.database.backend);
    return cfg;
}

} // namespace tmapsvr
