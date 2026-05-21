#include "config.h"

#include "fourstory/security/toml_loader.h"

#include <toml++/toml.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <stdexcept>

namespace tlogsvr {

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
    catch (const toml::parse_error& ex)
    {
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
        if (auto b = (*srv)["bind"].value<std::string>())
            cfg.bind_address = *b;
    }
    if (auto t = tbl["target_table"].value<std::string>())
        cfg.target_table = *t;
    if (auto db = tbl["database"].as_table())
    {
        if (auto b = (*db)["backend"].value<std::string>())          cfg.database.backend = *b;
        if (auto c = (*db)["connection_string"].value<std::string>()) cfg.database.connection_string = *c;
        if (auto s = (*db)["pool_size"].value<std::int64_t>())
        {
            if (*s < 1 || *s > 256) throw std::runtime_error("database.pool_size out of range");
            cfg.database.pool_size = static_cast<std::size_t>(*s);
        }
        if (auto s = (*db)["worker_threads"].value<std::int64_t>())
        {
            if (*s < 0 || *s > 256)
                throw std::runtime_error("database.worker_threads out of range");
            cfg.database.worker_threads = static_cast<std::size_t>(*s);
        }
    }
    if (auto log = tbl["log"].as_table())
    {
        if (auto l = (*log)["level"].value<std::string>())
            cfg.log_level = ParseLogLevel(*l);
    }
    if (auto health = tbl["health"].as_table())
    {
        if (auto p = (*health)["port"].value<std::int64_t>())
        {
            if (*p < 0 || *p > 65535)
                throw std::runtime_error("health.port out of range");
            cfg.health_port = static_cast<std::uint16_t>(*p);
        }
    }
    if (auto bp = tbl["backpressure"].as_table())
    {
        if (auto s = (*bp)["sample_interval_secs"].value<std::int64_t>())
        {
            if (*s < 0 || *s > 3600)
                throw std::runtime_error("backpressure.sample_interval_secs out of range (0..3600)");
            cfg.backpressure.sample_interval = std::chrono::seconds(*s);
        }
        if (auto a = (*bp)["always_log"].value<bool>())
            cfg.backpressure.always_log = *a;
    }
    if (auto retry = tbl["retry"].as_table())
    {
        if (auto m = (*retry)["max_queue"].value<std::int64_t>())
        {
            if (*m < 0 || *m > 1'000'000)
                throw std::runtime_error("retry.max_queue out of range (0..1000000)");
            cfg.retry.max_queue = static_cast<std::size_t>(*m);
        }
        if (auto s = (*retry)["drain_interval_secs"].value<std::int64_t>())
        {
            if (*s < 1 || *s > 3600)
                throw std::runtime_error("retry.drain_interval_secs out of range (1..3600)");
            cfg.retry.drain_interval = std::chrono::seconds(*s);
        }
        if (auto b = (*retry)["drain_batch_size"].value<std::int64_t>())
        {
            if (*b < 1 || *b > 10'000)
                throw std::runtime_error("retry.drain_batch_size out of range (1..10000)");
            cfg.retry.drain_batch_size = static_cast<std::size_t>(*b);
        }
    }
    if (auto c = tbl["cluster"].as_table())
    {
        if (auto h = (*c)["control_host"].value<std::string>())
            cfg.cluster.control_host = *h;
        if (auto p = (*c)["control_port"].value<std::int64_t>())
        {
            if (*p < 0 || *p > 65535)
                throw std::runtime_error("cluster.control_port out of range");
            cfg.cluster.control_port = static_cast<std::uint16_t>(*p);
        }
        if (auto g = (*c)["group_id"].value<std::int64_t>())
        {
            if (*g < 0 || *g > 255)
                throw std::runtime_error("cluster.group_id out of range (0..255)");
            cfg.cluster.group_id = static_cast<std::uint8_t>(*g);
        }
        if (auto s = (*c)["server_id"].value<std::int64_t>())
        {
            if (*s < 0 || *s > 255)
                throw std::runtime_error("cluster.server_id out of range (0..255)");
            cfg.cluster.server_id = static_cast<std::uint8_t>(*s);
        }
        if (auto a = (*c)["reported_addr"].value<std::string>())
            cfg.cluster.reported_addr = *a;
    }
    if (auto sec = tbl["security"].as_table())
        cfg.security = fourstory::security::LoadFromToml(*sec);
    if (auto err = cfg.security.Validate(); !err.empty())
        throw std::runtime_error(err);
    spdlog::info("loaded config from '{}' — udp_port={} table='{}' db={} "
                 "health={} retry_queue={}/{}s",
        path, cfg.port, cfg.target_table,
        cfg.database.connection_string.empty() ? "(none)" : cfg.database.backend,
        cfg.health_port, cfg.retry.max_queue,
        static_cast<long long>(cfg.retry.drain_interval.count()));
    return cfg;
}

} // namespace tlogsvr
