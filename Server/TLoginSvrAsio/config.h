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
#include <string>

namespace tloginsvr {

struct AppConfig
{
    LoginServerConfig            server;
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
