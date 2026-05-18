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
};

struct AppConfig
{
    LoginServerConfig            server;
    DbConfig                     database;
    spdlog::level::level_enum    log_level = spdlog::level::info;

    // Health endpoint — minimal HTTP responder on a separate port,
    // used by k8s liveness/readiness probes and load balancers.
    // 0 disables.
    std::uint16_t                health_port = 8815;
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
