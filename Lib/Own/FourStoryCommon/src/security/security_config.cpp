#include "fourstory/security/security_config.h"
#include "fourstory/security/hmac.h"
#include "fourstory/security/ip_allowlist.h"

#include <cstdlib>
#include <string>

namespace fourstory::security {

std::vector<std::uint8_t> SecurityConfig::ResolveMasterKey() const
{
    // Env var wins so production deploys can keep secrets out of TOML.
    if (!master_key_env.empty())
    {
        if (const char* v = std::getenv(master_key_env.c_str()))
        {
            std::string hex(v);
            if (!hex.empty())
                return Hmac::HexToBytes(hex);
        }
    }
    if (!master_key_hex.empty())
        return Hmac::HexToBytes(master_key_hex);
    return {};
}

std::string SecurityConfig::Validate() const
{
    // CIDR sanity — first malformed entry wins.
    if (auto bad = IpAllowlist::FirstInvalid(ip_allowlist))
        return "[security] ip_allowlist entry invalid: '" + *bad + "'";

    // Master key requirement.
    if (peer_auth_required)
    {
        const auto key = ResolveMasterKey();
        if (key.empty())
            return "[security] peer_auth_required=true but no master key "
                   "resolved (set $" + master_key_env +
                   " or [security] master_key_hex)";
        if (key.size() < 16)
            return "[security] master key too short (< 16 bytes) — use "
                   "at least 32 random bytes for production";
    }
    if (nonce_window.count() <= 0)
        return "[security] nonce_window_seconds must be > 0";
    if (handshake_timeout.count() <= 0)
        return "[security] handshake_timeout_seconds must be > 0";
    return {};
}

} // namespace fourstory::security
