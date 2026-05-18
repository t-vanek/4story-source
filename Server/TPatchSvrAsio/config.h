#pragma once

// TPatchSvrAsio TOML configuration. Mirrors TLoginSvrAsio's config
// shape but with patch-specific knobs (FTP root URL, login peer
// address that gets advertised in the PATCH_ACK responses).

#include <spdlog/common.h>

#include <cstdint>
#include <cstddef>
#include <string>

namespace tpatchsvr {

// SOCI DB connection — TGLOBAL only (patch metadata lives there).
struct DbConfig
{
    std::string  backend;
    std::string  connection_string;
    std::size_t  pool_size = 4;
};

struct AppConfig
{
    // TCP listener.
    std::uint16_t  port = 3715;                   // legacy DEF_PATCHPORT

    // FTP root URL where patch files actually live. The client gets
    // this back via CT_PATCH_ACK and downloads files itself; the
    // patch server only serves metadata.
    std::string    ftp_url = "ftp://patch.local";
    std::string    pre_ftp_url = "ftp://patch.local/pre";

    // Login server address advertised to the client after patching
    // (so the client knows where to log in once it has the files).
    std::string    login_host = "127.0.0.1";
    std::uint16_t  login_port = 4816;

    // TGLOBAL DB (patch metadata tables).
    DbConfig       database;

    // Health endpoint port. 0 disables.
    std::uint16_t  health_port = 8915;

    spdlog::level::level_enum log_level = spdlog::level::info;
};

AppConfig LoadConfig(const std::string& path);

} // namespace tpatchsvr
