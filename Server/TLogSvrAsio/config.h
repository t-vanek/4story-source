#pragma once

#include <spdlog/common.h>

#include <cstdint>
#include <cstddef>
#include <string>

namespace tlogsvr {

struct DbConfig
{
    std::string  backend;
    std::string  connection_string;
    std::size_t  pool_size = 4;
};

struct AppConfig
{
    // UDP listening port. Legacy default was 2000 (UdpSocket peer port
    // in TLoginSvr); keep it for back-compat.
    std::uint16_t  port = 2000;
    std::string    bind_address = "0.0.0.0";

    // Insert target. Default matches schema/tlog-audit.sql. Override
    // to a different table when you partition or move logs to a
    // dedicated audit DB.
    std::string    target_table = "TLOG_AUDIT";

    // TGLOBAL DB. Production deploys typically point this at a
    // dedicated audit database (separate from the live game DB) so
    // log retention policies don't touch player data.
    DbConfig       database;

    spdlog::level::level_enum log_level = spdlog::level::info;
};

AppConfig LoadConfig(const std::string& path);

} // namespace tlogsvr
