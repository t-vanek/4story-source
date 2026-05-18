#include "config.h"

#include <toml++/toml.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <stdexcept>

namespace tloginsvr {

namespace {

// Default legacy wire-format secret — same bytes as Session.cpp:16's
// `g_strSecretKey`. Repeated here so a default-empty config still
// produces a binary that can accept a real legacy client.
constexpr unsigned char kDefaultLegacySecret[] =
    "A5$$8AFS13A1::-11#!..'\x92" "19716AC&\x94" "/D1;;1#";
constexpr std::size_t   kDefaultLegacySecretLen = sizeof(kDefaultLegacySecret) - 1;

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

// Decode a hex string (no spaces, no 0x prefix) to bytes. Throws on
// odd length / non-hex chars.
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

    // Default RC4 secret = legacy bytes (so a no-config build still
    // accepts the shipped legacy client).
    cfg.server.rc4_secret_key.assign(
        reinterpret_cast<const std::byte*>(kDefaultLegacySecret),
        reinterpret_cast<const std::byte*>(kDefaultLegacySecret) + kDefaultLegacySecretLen);

    if (path.empty() || !std::filesystem::exists(path))
    {
        spdlog::info("no config file at '{}'; using defaults", path);
        return cfg;
    }

    toml::table tbl;
    try
    {
        tbl = toml::parse_file(path);
    }
    catch (const toml::parse_error& ex)
    {
        const auto desc = ex.description();
        throw std::runtime_error("TOML parse error in '" + path + "': "
            + std::string(desc.data(), desc.size()));
    }

    // [server]
    if (auto srv = tbl["server"].as_table())
    {
        if (auto p = (*srv)["port"].value<std::int64_t>())
        {
            if (*p < 0 || *p > 65535)
                throw std::runtime_error("server.port out of range: " + std::to_string(*p));
            cfg.server.port = static_cast<std::uint16_t>(*p);
        }
    }

    // [crypto]
    if (auto crypto = tbl["crypto"].as_table())
    {
        if (auto disable = (*crypto)["disable_rc4"].value<bool>())
        {
            if (*disable) cfg.server.rc4_secret_key.clear();
        }
        if (auto hex = (*crypto)["rc4_secret_hex"].value<std::string>())
        {
            cfg.server.rc4_secret_key = ParseHexBytes(*hex);
        }
    }

    // [log]
    if (auto log = tbl["log"].as_table())
    {
        if (auto lvl = (*log)["level"].value<std::string>())
        {
            cfg.log_level = ParseLogLevel(*lvl);
        }
    }

    // [health]
    if (auto health = tbl["health"].as_table())
    {
        if (auto p = (*health)["port"].value<std::int64_t>())
        {
            if (*p < 0 || *p > 65535)
                throw std::runtime_error("health.port out of range: " + std::to_string(*p));
            cfg.health_port = static_cast<std::uint16_t>(*p);
        }
    }

    // [database] — Phase B: SOCI-backed services. Absence / empty
    // `connection_string` keeps the binary in legacy / in-memory mode.
    if (auto db = tbl["database"].as_table())
    {
        if (auto b = (*db)["backend"].value<std::string>())
            cfg.database.backend = *b;
        if (auto c = (*db)["connection_string"].value<std::string>())
            cfg.database.connection_string = *c;
        if (auto p = (*db)["pool_size"].value<std::int64_t>())
        {
            if (*p < 1 || *p > 1024)
                throw std::runtime_error("database.pool_size out of range: "
                    + std::to_string(*p));
            cfg.database.pool_size = static_cast<std::size_t>(*p);
        }
    }

    spdlog::info("loaded config from '{}' — server.port={} health.port={} rc4={} "
                 "db={} log_level={}",
        path,
        cfg.server.port,
        cfg.health_port,
        cfg.server.rc4_secret_key.empty() ? "disabled" : "enabled",
        cfg.database.connection_string.empty()
            ? "in-memory"
            : (cfg.database.backend + " (pool=" + std::to_string(cfg.database.pool_size) + ")"),
        spdlog::level::to_string_view(cfg.log_level));

    return cfg;
}

} // namespace tloginsvr
