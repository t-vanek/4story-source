#pragma once

// TControlSvrAsio TOML configuration. F1 ships the structural shape
// (server port, DB config, ops endpoints, log level) plus a seed
// table for the in-memory FakeOperatorAuth / FakeServiceInventory.
// SOCI-backed loads land in F2.

#include <spdlog/common.h>

#include <cstdint>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace tcontrolsvr {

struct DbConfig
{
    std::string  backend;
    std::string  connection_string;
    std::size_t  pool_size = 4;
    // Worker threads for synchronous SOCI calls. Each worker
    // serves one in-flight DB query at a time, so this can be
    // smaller than pool_size if your bottleneck is DB latency, or
    // equal if it's CPU. Defaults to 2 — adequate for the control
    // server's low DB rate. 0 disables the worker pool (handlers
    // run SOCI in-line on the io_context, legacy F1–F5 behavior).
    std::size_t  worker_threads = 2;
};

// One row of [[fake.operators]] in TOML.
struct FakeOperatorSeed
{
    std::string  id;
    std::string  password;
    std::uint8_t authority = 0;
};

// Optional in-config bootstrap inventory for F1 — useful when running
// against fake services. F2 swaps this for SOCI loads from TMACHINE /
// TGROUP / TSVRTYPE / TSERVER / TIPADDR.
struct FakeInventorySeed
{
    struct Group   { std::uint8_t id; std::string name; };
    struct Machine { std::uint8_t id; std::string name; };
    struct Type    { std::uint8_t id; std::string name; };

    std::vector<Group>    groups;
    std::vector<Machine>  machines;
    std::vector<Type>     types;
};

struct AppConfig
{
    // TCP listener for operators (TController.exe + stat tool) and
    // peer daemons. Default 12000 is arbitrary but free in the
    // legacy installer's default port table.
    std::uint16_t  port = 12000;

    DbConfig       database;

    // Health endpoint and admin shell — same shape as Login/Patch.
    std::uint16_t  health_port = 18086;
    std::uint16_t  admin_port  = 18186;
    std::string    admin_bind  = "127.0.0.1";

    // Default auto-start flag for the cluster scheduler. Operators
    // can flip this at runtime via CT_SERVICEAUTOSTART_REQ.
    std::uint8_t   auto_start = 0;

    // Per-IP rate limit on CT_OPLOGIN_REQ. Token bucket — `burst`
    // attempts allowed in quick succession, then one refill every
    // `refill_seconds`. Hardening against brute-force; the legacy
    // server had no such limit. Set burst=0 to disable.
    std::uint32_t  login_rate_burst          = 5;
    std::uint32_t  login_rate_refill_seconds = 10;

    // Live inventory refresh period. Re-reads TMACHINE / TGROUP /
    // TSVRTYPE / TSERVER / TIPADDR every N seconds so the operator
    // GUI sees topology changes without a daemon restart. 0
    // disables the refresher (legacy behavior — load once at boot).
    // Only meaningful when [database] is configured.
    std::uint32_t  inventory_refresh_seconds = 0;

    // F1 seeds — populated only when [fake] tables are present.
    std::vector<FakeOperatorSeed>  fake_operators;
    FakeInventorySeed              fake_inventory;

    // [cluster.scm] — picks the IServiceController backend used by
    // `cluster start/stop/restart` admin shell commands. See
    // services/service_controller_factory.h for the selection
    // rules. Default "auto" picks the platform-native backend
    // (windows on _WIN32, systemd on __linux__, otherwise disabled).
    struct ScmConfig
    {
        std::string  backend              = "auto";
        std::string  service_name_template= "{type_name}-{group}-{server}";
        bool         systemd_user_scope   = false;
        std::string  systemctl_path       = "systemctl";
        std::unordered_map<std::uint32_t, std::string> overrides;
    };
    ScmConfig scm;

    spdlog::level::level_enum log_level = spdlog::level::info;
};

AppConfig LoadConfig(const std::string& path);

} // namespace tcontrolsvr
