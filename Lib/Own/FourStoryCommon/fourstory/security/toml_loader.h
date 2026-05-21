#pragma once

// Header-only TOML parser for the shared [security] block.
//
// Avoids duplicating the same 30 lines of toml++ glue across every
// server's config.cpp. Call from each server's LoadConfig() after
// parsing the toml::table:
//
//   if (auto sec = tbl["security"].as_table())
//       cfg.security = fourstory::security::LoadFromToml(*sec);
//
// All keys are optional — missing block means "defaults" which is a
// permissive posture (ip_allowlist empty + enforce=false + peer auth
// not required).

#include "security_config.h"

#include <toml++/toml.hpp>

namespace fourstory::security {

inline SecurityConfig LoadFromToml(const toml::table& tbl)
{
    SecurityConfig cfg;

    if (auto arr = tbl["ip_allowlist"].as_array())
    {
        for (auto& el : *arr)
            if (auto s = el.value<std::string>())
                cfg.ip_allowlist.push_back(*s);
    }
    if (auto b = tbl["ip_allowlist_enforce"].value<bool>())
        cfg.ip_allowlist_enforce = *b;
    if (auto b = tbl["peer_auth_required"].value<bool>())
        cfg.peer_auth_required = *b;
    if (auto s = tbl["master_key_env"].value<std::string>())
        cfg.master_key_env = *s;
    if (auto s = tbl["master_key_hex"].value<std::string>())
        cfg.master_key_hex = *s;
    if (auto n = tbl["nonce_window_seconds"].value<std::int64_t>())
        cfg.nonce_window = std::chrono::seconds(*n);
    if (auto n = tbl["future_window_seconds"].value<std::int64_t>())
        cfg.future_window = std::chrono::seconds(*n);
    if (auto n = tbl["handshake_timeout_seconds"].value<std::int64_t>())
        cfg.handshake_timeout = std::chrono::seconds(*n);
    if (auto b = tbl["db_trust_store"].value<bool>())
        cfg.db_trust_store = *b;
    if (auto b = tbl["audit_failed_attempts"].value<bool>())
        cfg.audit_failed_attempts = *b;

    return cfg;
}

} // namespace fourstory::security
