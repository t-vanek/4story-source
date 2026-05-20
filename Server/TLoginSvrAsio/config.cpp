#include "config.h"

#include <toml++/toml.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <stdexcept>

namespace tloginsvr {

namespace {

// Default legacy wire-format secret — same bytes as Session.cpp:22's
// `g_strSecretKey`. Repeated here so a default-empty config still
// produces a binary that can accept a real legacy client.
//
// CRITICAL: the length passed into RC4MD5Transform MUST include the
// trailing NUL byte. Legacy Session.cpp:93 calls EncryptBuffer with
// `g_strSecretKey.size() + 1`, feeding the NUL into MD5. Without it
// the modern server hashes 31 bytes, the client hashes 32, the
// derived RC4 keys differ, and every packet decrypts to garbage.
// kDefaultLegacySecretLen therefore uses sizeof() unchanged (i.e.
// includes the array's terminating '\0').
constexpr unsigned char kDefaultLegacySecret[] =
    "A5$$8AFS13A1::-11#!..'\x92" "19716AC&\x94" "/D1;;1#";
constexpr std::size_t   kDefaultLegacySecretLen = sizeof(kDefaultLegacySecret);

Nation ParseNation(std::string_view s)
{
    // Accept the same labels CSPGetNation returns plus the obvious
    // lowercase shorthands. Anything else is a config error — fail
    // loud at startup rather than silently fall back to US (a JP
    // operator who typos "japane" would otherwise reject every
    // bChanneling-carrying login with a malformed-body close).
    if (s == "us"      || s == "US")      return Nation::US;
    if (s == "germany" || s == "de")      return Nation::Germany;
    if (s == "taiwan"  || s == "tw")      return Nation::Taiwan;
    if (s == "japan"   || s == "jp")      return Nation::Japan;
    if (s == "korea"   || s == "kr")      return Nation::Korea;
    if (s == "russia"  || s == "ru")      return Nation::Russia;
    throw std::runtime_error("invalid nation: " + std::string(s)
        + " (expected one of: us|germany|taiwan|japan|korea|russia)");
}

const char* NationName(Nation n)
{
    switch (n)
    {
    case Nation::US:      return "us";
    case Nation::Germany: return "germany";
    case Nation::Taiwan:  return "taiwan";
    case Nation::Japan:   return "japan";
    case Nation::Korea:   return "korea";
    case Nation::Russia:  return "russia";
    }
    return "us";
}

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
        // accepted_versions = [0x2918, 0x2917, …]
        // List of CS_LOGIN_REQ.wVersion values the server will accept.
        // Missing → defaults to the legacy single value 0x2918.
        if (auto t = (*srv)["test_handlers_enabled"].value<bool>())
            cfg.server.test_handlers_enabled = *t;
        if (auto n = (*srv)["nation"].value<std::string>())
            cfg.server.nation = ParseNation(*n);
        if (auto c = (*srv)["control_server_ip"].value<std::string>())
            cfg.server.control_server_ip = *c;
        if (auto g = (*srv)["control_server_gate_open"].value<bool>())
            cfg.server.control_server_gate_open = *g;
        if (auto mc = (*srv)["max_connections"].value<std::int64_t>())
        {
            if (*mc < 0 || *mc > 0xFFFFFFFFLL)
                throw std::runtime_error("server.max_connections out of range: "
                    + std::to_string(*mc));
            cfg.server.max_connections = static_cast<std::uint32_t>(*mc);
        }
        if (auto av = (*srv)["accepted_versions"].as_array())
        {
            std::vector<std::uint16_t> list;
            for (const auto& el : *av)
            {
                if (auto v = el.value<std::int64_t>())
                {
                    if (*v < 0 || *v > 0xFFFF)
                        throw std::runtime_error(
                            "server.accepted_versions: value out of u16 range: "
                            + std::to_string(*v));
                    list.push_back(static_cast<std::uint16_t>(*v));
                }
            }
            if (!list.empty())
                cfg.server.accepted_versions = std::move(list);
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
            // Operator-supplied bytes are passed through verbatim —
            // no implicit NUL append. Operators porting from the
            // legacy default secret MUST include the trailing 00
            // byte in their hex string (see Session.cpp:18 + the
            // kDefaultLegacySecretLen comment in this file).
            cfg.server.rc4_secret_key = ParseHexBytes(*hex);
        }
    }

    // [audit] — optional UDP shim to TLogSvr collector.
    if (auto audit = tbl["audit"].as_table())
    {
        if (auto udp = (*audit)["udp"].as_table())
        {
            if (auto h = (*udp)["host"].value<std::string>())
                cfg.audit_udp_host = *h;
            if (auto p = (*udp)["port"].value<std::int64_t>())
            {
                if (*p < 0 || *p > 65535)
                    throw std::runtime_error("audit.udp.port out of range: "
                        + std::to_string(*p));
                cfg.audit_udp_port = static_cast<std::uint16_t>(*p);
            }
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

    // [admin] — opt-in TCP shell.
    if (auto admin = tbl["admin"].as_table())
    {
        if (auto h = (*admin)["bind"].value<std::string>())
            cfg.admin_bind = *h;
        if (auto p = (*admin)["port"].value<std::int64_t>())
        {
            if (*p < 0 || *p > 65535)
                throw std::runtime_error("admin.port out of range: "
                    + std::to_string(*p));
            cfg.admin_port = static_cast<std::uint16_t>(*p);
        }
    }

    // [smtp] — outbound mail relay for 2FA security codes.
    if (auto smtp = tbl["smtp"].as_table())
    {
        if (auto h = (*smtp)["host"].value<std::string>())
            cfg.smtp.host = *h;
        if (auto p = (*smtp)["port"].value<std::int64_t>())
        {
            if (*p < 0 || *p > 65535)
                throw std::runtime_error("smtp.port out of range: " + std::to_string(*p));
            cfg.smtp.port = static_cast<std::uint16_t>(*p);
        }
        if (auto f = (*smtp)["from_address"].value<std::string>())
            cfg.smtp.from_address = *f;
        if (auto d = (*smtp)["from_display"].value<std::string>())
            cfg.smtp.from_display = *d;
        if (auto u = (*smtp)["username"].value<std::string>())
            cfg.smtp.username = *u;
        if (auto p = (*smtp)["password"].value<std::string>())
            cfg.smtp.password = *p;
    }
    if (!cfg.smtp.host.empty() && cfg.smtp.from_address.empty())
        throw std::runtime_error("smtp.host is set but smtp.from_address is empty");

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

    // [database] — TGLOBAL (accounts, sessions, server registry).
    // [database.world] — TGAME (per-world chars, items, guilds).
    // Absence / empty `connection_string` on the global section keeps the
    // binary in legacy / in-memory mode. Missing [database.world] degrades
    // gracefully: char service falls back to in-memory, map locator skips
    // BR/BOW shard checks.
    auto parse_db = [](const toml::table& t, DbConfig& out, const char* section)
    {
        if (auto b = t["backend"].value<std::string>())
            out.backend = *b;
        if (auto c = t["connection_string"].value<std::string>())
            out.connection_string = *c;
        if (auto p = t["pool_size"].value<std::int64_t>())
        {
            if (*p < 1 || *p > 1024)
                throw std::runtime_error(std::string(section) + ".pool_size out of range: "
                    + std::to_string(*p));
            out.pool_size = static_cast<std::size_t>(*p);
        }
        if (auto a = t["acquire_timeout_secs"].value<std::int64_t>())
        {
            if (*a < 0 || *a > 3600)
                throw std::runtime_error(std::string(section)
                    + ".acquire_timeout_secs out of range (0..3600): "
                    + std::to_string(*a));
            out.acquire_timeout_secs = static_cast<std::uint32_t>(*a);
        }
    };
    if (auto db = tbl["database"].as_table())
    {
        parse_db(*db, cfg.database, "database");
        if (auto world = (*db)["world"].as_table())
        {
            parse_db(*world, cfg.database_world, "database.world");
        }
    }

    spdlog::info("loaded config from '{}' — server.port={} nation={} health.port={} rc4={} "
                 "db_global={} db_world={} log_level={}",
        path,
        cfg.server.port,
        NationName(cfg.server.nation),
        cfg.health_port,
        cfg.server.rc4_secret_key.empty() ? "disabled" : "enabled",
        cfg.database.connection_string.empty()
            ? "in-memory"
            : (cfg.database.backend + " (pool=" + std::to_string(cfg.database.pool_size) + ")"),
        cfg.database_world.connection_string.empty()
            ? "in-memory"
            : (cfg.database_world.backend + " (pool=" + std::to_string(cfg.database_world.pool_size) + ")"),
        spdlog::level::to_string_view(cfg.log_level));

    return cfg;
}

} // namespace tloginsvr
