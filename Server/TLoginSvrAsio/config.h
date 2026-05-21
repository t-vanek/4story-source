#pragma once

// Application-level configuration loader. Reads a TOML file into a
// typed AppConfig struct that main builds the LoginServer from.
// Replaces the Win32-registry-based legacy config (Server/TLoginSvr
// reads from HKLM\Software\…\Config; not portable, not git-able,
// not version-controllable, not testable).
//
// Schema documented in `tloginsvr.example.toml` at this repo's root.
// All fields are optional — missing keys fall back to defaults that
// match the legacy server's hardcoded behavior so a default-empty
// config file produces a working binary.

#include "login_server.h"

#include <spdlog/common.h>

#include <cstdint>
#include <cstddef>
#include <string>

namespace tloginsvr {

// Database connection config (Phase B). Empty `connection_string`
// keeps the binary in legacy / in-memory mode; setting it routes auth
// (and later, char / session lookup) through SOCI against the named
// backend. Production cutover is a TOML-only change once the schema
// is in place on the target DB.
struct DbConfig
{
    // Backend name: "postgresql" | "sqlite3" | "odbc". Empty = no DB.
    std::string   backend;

    // SOCI-format connection string. Examples:
    //   postgresql: "host=db.prod port=5432 dbname=tloginsvr
    //                user=tloginsvr password=... sslmode=require"
    //   sqlite3:    "db=/var/lib/tloginsvr/dev.sqlite"
    //   odbc:       "DSN=MSSQL_PROD;UID=login;PWD=..."
    std::string   connection_string;

    // Number of pooled sessions. Sized to peer concurrency: ~handler
    // throughput × max ms-per-query. 8 is a sane default for dev; tune
    // to 32–64 in prod with a steady-state CCU >5k.
    std::size_t   pool_size = 8;

    // Maximum time a handler will wait for a free pool session before
    // failing with AcquireTimeout. Bounds the worst-case latency of a
    // single request when the pool is saturated. Zero = wait forever
    // (legacy blocking behavior — only for tests that intentionally
    // want to deadlock on an exhausted pool).
    std::uint32_t acquire_timeout_secs = 30;

    // Worker threads for off-loop SOCI calls. Handlers wrap their
    // SOCI calls in fourstory::db::CoOffloadIf — when this is > 0
    // the work runs on a worker thread, keeping the io_context
    // responsive under DB latency. 0 = legacy in-line behavior
    // (handler coroutine blocks on the SOCI session for its
    // duration). Recommended 2–4 for production; the workers
    // share `pool_size` SOCI sessions so worker_threads <= pool_size
    // is reasonable.
    std::size_t   worker_threads = 4;
};

// Optional SMTP relay config. Empty `host` keeps the binary on the
// SpdlogSmtpClient default (2FA codes go to the log; no mail leaves
// the process). Setting `host` switches the wiring to the real
// Asio-based SMTP client.
struct SmtpConfig
{
    std::string   host;          // empty → log-only mode
    std::uint16_t port = 25;
    std::string   from_address;  // MAIL FROM envelope + From: header
    std::string   from_display;  // optional display name; default = from_address
    std::string   username;      // AUTH LOGIN — empty → no AUTH
    std::string   password;
};

struct AppConfig
{
    LoginServerConfig            server;

    // TGLOBAL — accounts, sessions, server registry, cross-world char index.
    // Configured under [database] (legacy key, kept for back-compat). Empty
    // connection_string keeps the binary in in-memory / dev mode.
    DbConfig                     database;

    // TGAME — per-world character + item + guild data. Configured under
    // [database.world]. Required for real character ops (create / list /
    // delete) and the BR/BOW shard routing in CS_START_REQ. If empty, the
    // char service falls back to in-memory and the map locator skips the
    // shard override check.
    DbConfig                     database_world;

    // Optional UDP audit shim — sends each audit event in legacy
    // _UDPPACKET wire format to the given host:port. Empty host →
    // shim disabled (audit goes through the structured spdlog
    // sink only).
    std::string                  audit_udp_host;
    std::uint16_t                audit_udp_port = 0;

    spdlog::level::level_enum    log_level = spdlog::level::info;

    // Health endpoint — minimal HTTP responder on a separate port,
    // used by k8s liveness/readiness probes and load balancers.
    // 0 disables.
    std::uint16_t                health_port = 8815;

    // Admin TCP shell intentionally NOT exposed here. The single
    // operator entry point for the cluster is TControlSvrAsio's
    // AdminShell — operator commands (status, kick, ban, log-level)
    // reach this server via TControl's peer-forwarder pipeline, not
    // via a per-server localhost shell. See ADR / TControl README.

    // SMTP relay for 2FA mail. Empty host → log-only fallback.
    SmtpConfig                   smtp;
};

// Load + parse the TOML config at `path`. Throws std::runtime_error
// on parse / type errors. Missing file is NOT an error — returns a
// default-constructed AppConfig.
//
// The `legacy_secret_owner` out-param is populated with the bytes of
// the rc4 secret key (if any was specified or defaulted) so the
// LoginServerConfig::rc4_secret_key span has stable backing storage
// for the lifetime of the AppConfig.
AppConfig LoadConfig(const std::string& path);

} // namespace tloginsvr
