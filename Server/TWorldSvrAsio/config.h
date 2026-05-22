#pragma once

// TWorldSvrAsio TOML configuration. W1 ships the structural shape
// (server port, log level, ops endpoints). W2 adds [database] for
// the global + world SOCI pools. W3+ adds per-feature toggles.

#include <spdlog/common.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace tworldsvr {

struct DbConfig
{
    std::string  backend;
    std::string  connection_string;
    std::size_t  pool_size      = 4;
    // Worker threads for synchronous SOCI calls. Each worker serves
    // one in-flight DB query at a time. 0 = run SOCI in-line on the
    // io_context (legacy fallback; only OK for tests). Wired in W2.
    std::size_t  worker_threads = 2;
};

struct AppConfig
{
    // TCP listener for inbound SS connections from TMap/TLogin peers.
    // DEF_WORLDPORT (3815) matches the legacy default in TNetDef.h
    // so existing peer configurations point at this listener without
    // change.
    std::uint16_t port = 3815;

    // Hard cap on concurrent peer sockets. Defense against pre-auth
    // socket flood (parallel to TLoginSvrAsio's max_connections gate).
    // 0 = unlimited (not recommended in production).
    std::uint32_t max_connections = 256;

    DbConfig      database;

    // Health endpoint shape mirrors the four shipped Asio daemons.
    std::uint16_t health_port = 18087;

    spdlog::level::level_enum log_level = spdlog::level::info;
};

AppConfig LoadConfig(const std::string& path);

} // namespace tworldsvr
