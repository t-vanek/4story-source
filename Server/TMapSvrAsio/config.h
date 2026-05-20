#pragma once

// TMapSvrAsio TOML configuration. F1 ships the structural shape (port,
// DB, ops endpoints, log level). Gameplay handlers + interface wiring
// arrive in later phases — see Server/TMapSvrAsio/README.md for the
// phased plan.

#include <spdlog/common.h>

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace tmapsvr {

struct DbConfig
{
    std::string  backend;            // "odbc" | "postgresql" | …
    std::string  connection_string;  // dialect-specific
    std::size_t  pool_size = 8;
    std::size_t  worker_threads = 4;
};

struct AppConfig
{
    // TCP listener for the legacy game client. Legacy default is 5815
    // (per TSERVER row for bType=TMAP=4). Configurable so a single host
    // can run multiple Map instances on adjacent ports.
    std::uint16_t  port = 5815;

    // RC4 secret key for the inbound client wire (same legacy convention
    // as TLoginSvrAsio: client→server is RC4+XOR, server→client is
    // XOR-only). Empty disables RC4 — useful for tests where the peer
    // is another modernized binary, not a legacy game client.
    std::vector<std::byte> rc4_secret_key;

    // Wire-version gate (CS_CONNECT_REQ.wVersion). Defaults to the
    // single legacy ProtocolBase.h::TVERSION value. Operators can
    // extend the list to support a rolling client rollout.
    std::vector<std::uint16_t> accepted_versions = { 0x2918 };

    DbConfig       database;

    // Ops endpoints — match the TLoginSvrAsio shape so an operator can
    // hit /healthz on the same port everywhere.
    std::uint16_t  health_port = 18095;
    std::uint16_t  admin_port  = 18195;
    std::string    admin_bind  = "127.0.0.1";

    // Per-IP rate limit on CS_CONNECT_REQ. Defends against
    // half-open-session floods in the map handshake (legacy server has
    // no rate limit on this path either). burst=0 disables.
    std::uint32_t  connect_rate_burst          = 10;
    std::uint32_t  connect_rate_refill_seconds = 5;

    // Pre-auth idle timeout — drop connections that haven't sent a
    // valid CS_CONNECT_REQ within this window. 0 disables. 30s
    // is generous for a real client (CS_CONNECT_REQ is the first
    // packet after the TCP handshake completes); hostile to half-
    // open SYN-floods.
    std::uint32_t  pre_auth_timeout_seconds = 30;

    // Soft cap on concurrent live sessions. New accepts past the cap
    // are dropped immediately. 0 = no cap (legacy behavior). MMO map
    // hosts can hold a few thousand chars; the default is generous so
    // a single misconfigured shard doesn't OOM the host.
    std::uint32_t  max_connections = 8000;

    spdlog::level::level_enum log_level = spdlog::level::info;
};

AppConfig LoadConfig(const std::string& path);

} // namespace tmapsvr
