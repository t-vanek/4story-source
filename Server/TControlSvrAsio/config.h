#pragma once

// TControlSvrAsio TOML configuration. F1 ships the structural shape
// (server port, DB config, ops endpoints, log level) plus a seed
// table for the in-memory FakeOperatorAuth / FakeServiceInventory.
// SOCI-backed loads land in F2.

#include <spdlog/common.h>

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace tcontrolsvr {

struct DbConfig
{
    std::string  backend;
    std::string  connection_string;
    std::size_t  pool_size = 4;
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

    // F1 seeds — populated only when [fake] tables are present.
    std::vector<FakeOperatorSeed>  fake_operators;
    FakeInventorySeed              fake_inventory;

    spdlog::level::level_enum log_level = spdlog::level::info;
};

AppConfig LoadConfig(const std::string& path);

} // namespace tcontrolsvr
