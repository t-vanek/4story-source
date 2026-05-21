#include "config.h"

#include <toml++/toml.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <stdexcept>

namespace tcontrolsvr {

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

std::uint8_t Byte(std::int64_t v, const char* key)
{
    if (v < 0 || v > 255)
        throw std::runtime_error(std::string(key) + " out of range");
    return static_cast<std::uint8_t>(v);
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
        if (auto a = (*srv)["auto_start"].value<std::int64_t>())
            cfg.auto_start = Byte(*a, "server.auto_start");
    }
    if (auto rl = tbl["login_rate"].as_table())
    {
        if (auto v = (*rl)["burst"].value<std::int64_t>())
        {
            if (*v < 0 || *v > 0xFFFFFFFFLL)
                throw std::runtime_error("login_rate.burst out of range");
            cfg.login_rate_burst = static_cast<std::uint32_t>(*v);
        }
        if (auto v = (*rl)["refill_seconds"].value<std::int64_t>())
        {
            if (*v < 0 || *v > 0xFFFFFFFFLL)
                throw std::runtime_error("login_rate.refill_seconds out of range");
            cfg.login_rate_refill_seconds = static_cast<std::uint32_t>(*v);
        }
    }
    if (auto inv = tbl["inventory"].as_table())
    {
        if (auto v = (*inv)["refresh_seconds"].value<std::int64_t>())
        {
            if (*v < 0 || *v > 0xFFFFFFFFLL)
                throw std::runtime_error("inventory.refresh_seconds out of range");
            cfg.inventory_refresh_seconds = static_cast<std::uint32_t>(*v);
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

    // [[fake.operators]] — only used when [database] is unset.
    if (auto arr = tbl["fake"]["operators"].as_array())
    {
        for (const auto& el : *arr)
        {
            const auto* row = el.as_table();
            if (!row) continue;
            FakeOperatorSeed o{};
            if (auto v = (*row)["id"].value<std::string>())       o.id = *v;
            if (auto v = (*row)["password"].value<std::string>()) o.password = *v;
            if (auto v = (*row)["authority"].value<std::int64_t>())
                o.authority = Byte(*v, "fake.operators[].authority");
            if (!o.id.empty() && o.authority != 0)
                cfg.fake_operators.push_back(std::move(o));
        }
    }
    auto LoadIdNameArray = [&](const char* key, auto& dst)
    {
        if (auto arr = tbl["fake"][key].as_array())
        {
            for (const auto& el : *arr)
            {
                const auto* row = el.as_table();
                if (!row) continue;
                using Item = std::decay_t<decltype(dst[0])>;
                Item it{};
                if (auto v = (*row)["id"].value<std::int64_t>())
                    it.id = Byte(*v, key);
                if (auto v = (*row)["name"].value<std::string>())
                    it.name = *v;
                dst.push_back(std::move(it));
            }
        }
    };
    LoadIdNameArray("groups",   cfg.fake_inventory.groups);
    LoadIdNameArray("machines", cfg.fake_inventory.machines);
    LoadIdNameArray("types",    cfg.fake_inventory.types);

    if (auto rp = tbl["registry"]["persistence"].as_table())
    {
        if (auto v = (*rp)["enabled"].value<bool>())
            cfg.registry_persistence.enabled = *v;
        if (auto v = (*rp)["table_name"].value<std::string>())
            cfg.registry_persistence.table_name = *v;
    }

    if (auto scm = tbl["cluster"]["scm"].as_table())
    {
        if (auto v = (*scm)["backend"].value<std::string>())
            cfg.scm.backend = *v;
        if (auto v = (*scm)["service_name_template"].value<std::string>())
            cfg.scm.service_name_template = *v;
        if (auto v = (*scm)["systemd_user_scope"].value<bool>())
            cfg.scm.systemd_user_scope = *v;
        if (auto v = (*scm)["systemctl_path"].value<std::string>())
            cfg.scm.systemctl_path = *v;
        if (auto v = (*scm)["status_reconcile_interval_secs"].value<std::int64_t>())
        {
            if (*v < 0 || *v > 3600)
                throw std::runtime_error(
                    "cluster.scm.status_reconcile_interval_secs out of range (0..3600)");
            cfg.scm.status_reconcile_interval_secs =
                static_cast<std::uint32_t>(*v);
        }
        if (auto ov = (*scm)["overrides"].as_table())
        {
            for (const auto& [k, v] : *ov)
            {
                if (auto s = v.value<std::string>())
                {
                    // Operators write sids as hex or decimal strings;
                    // the TOML key arrives as a string either way.
                    std::string key{k.str()};
                    std::uint32_t sid = 0;
                    try {
                        sid = (key.size() > 2 && key[0] == '0' &&
                               (key[1] == 'x' || key[1] == 'X'))
                            ? static_cast<std::uint32_t>(
                                std::stoul(key.substr(2), nullptr, 16))
                            : static_cast<std::uint32_t>(
                                std::stoul(key, nullptr, 10));
                    }
                    catch (...) {
                        throw std::runtime_error(
                            "cluster.scm.overrides: bad sid key '" + key + "'");
                    }
                    cfg.scm.overrides[sid] = *s;
                }
            }
        }
    }

    spdlog::info("loaded config from '{}' — port={} db={} fake_ops={} "
                 "fake_groups={} fake_machines={} fake_types={}",
        path, cfg.port,
        cfg.database.connection_string.empty() ? "(none)" : cfg.database.backend,
        cfg.fake_operators.size(),
        cfg.fake_inventory.groups.size(),
        cfg.fake_inventory.machines.size(),
        cfg.fake_inventory.types.size());
    return cfg;
}

} // namespace tcontrolsvr
