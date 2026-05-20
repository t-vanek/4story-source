#include "config.h"

#include <toml++/toml.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <stdexcept>

namespace tmapsvr {

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

// Legacy default RC4 secret — identical to TLoginSvrAsio. The literal
// includes the two CP1252 curly quotes (0x92, 0x94) plus the trailing
// NUL byte; legacy Session.cpp hashes secret_len+1 bytes so the NUL
// participates in the MD5 → RC4 key derivation, and any consumer
// missing the NUL desyncs the keystream.
const std::vector<std::byte>& DefaultLegacySecret()
{
    static const std::vector<std::byte> bytes = []
    {
        constexpr unsigned char kRaw[] =
            "A5$$8AFS13A1::-11#!..'\x92" "19716AC&\x94" "/D1;;1#";
        std::vector<std::byte> out;
        out.reserve(sizeof(kRaw));   // includes trailing NUL
        for (std::size_t i = 0; i < sizeof(kRaw); ++i)
            out.push_back(static_cast<std::byte>(kRaw[i]));
        return out;
    }();
    return bytes;
}

} // namespace

AppConfig LoadConfig(const std::string& path)
{
    AppConfig cfg;
    cfg.rc4_secret_key = DefaultLegacySecret();

    if (path.empty() || !std::filesystem::exists(path))
    {
        spdlog::info("no config file at '{}'; using defaults", path);
        return cfg;
    }
    toml::table tbl;
    try { tbl = toml::parse_file(path); }
    catch (const toml::parse_error& ex)
    {
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
        if (auto t = (*srv)["pre_auth_timeout_seconds"].value<std::int64_t>())
        {
            if (*t < 0 || *t > 0xFFFFFFFFLL)
                throw std::runtime_error("server.pre_auth_timeout_seconds out of range");
            cfg.pre_auth_timeout_seconds = static_cast<std::uint32_t>(*t);
        }
        if (auto versions = (*srv)["accepted_versions"].as_array())
        {
            cfg.accepted_versions.clear();
            for (const auto& el : *versions)
            {
                const auto v = el.value<std::int64_t>();
                if (!v) continue;
                if (*v < 0 || *v > 0xFFFF)
                    throw std::runtime_error("server.accepted_versions entry out of range");
                cfg.accepted_versions.push_back(static_cast<std::uint16_t>(*v));
            }
        }
    }
    if (auto rl = tbl["connect_rate"].as_table())
    {
        if (auto v = (*rl)["burst"].value<std::int64_t>())
        {
            if (*v < 0 || *v > 0xFFFFFFFFLL)
                throw std::runtime_error("connect_rate.burst out of range");
            cfg.connect_rate_burst = static_cast<std::uint32_t>(*v);
        }
        if (auto v = (*rl)["refill_seconds"].value<std::int64_t>())
        {
            if (*v < 0 || *v > 0xFFFFFFFFLL)
                throw std::runtime_error("connect_rate.refill_seconds out of range");
            cfg.connect_rate_refill_seconds = static_cast<std::uint32_t>(*v);
        }
    }
    if (auto db = tbl["database"].as_table())
    {
        if (auto b = (*db)["backend"].value<std::string>())
            cfg.database.backend = *b;
        if (auto c = (*db)["connection_string"].value<std::string>())
            cfg.database.connection_string = *c;
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
    if (auto h = tbl["health"].as_table())
    {
        if (auto p = (*h)["port"].value<std::int64_t>())
            cfg.health_port = Port(*p, "health.port");
    }
    if (auto a = tbl["admin"].as_table())
    {
        if (auto p = (*a)["port"].value<std::int64_t>())
            cfg.admin_port = Port(*p, "admin.port");
        if (auto b = (*a)["bind"].value<std::string>())
            cfg.admin_bind = *b;
    }
    if (auto log = tbl["log"].as_table())
    {
        if (auto lvl = (*log)["level"].value<std::string>())
            cfg.log_level = ParseLogLevel(*lvl);
    }

    spdlog::info("loaded config from '{}' — port={} db={} max_conn={}",
        path, cfg.port,
        cfg.database.connection_string.empty() ? "(none)" : cfg.database.backend,
        cfg.max_connections);
    return cfg;
}

} // namespace tmapsvr
