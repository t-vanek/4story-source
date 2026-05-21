#pragma once

// HMAC-SHA256 wrapper over OpenSSL EVP.
//
// Used by peer authentication to sign + verify handshake tokens. The
// hash output is always 32 bytes; we expose both binary and hex forms.
// Constant-time comparison is provided via Verify() — never compare
// HMAC outputs with memcmp / std::equal in user code.

#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fourstory::security {

class Hmac
{
public:
    static constexpr std::size_t kDigestSize = 32;
    using Digest = std::array<std::uint8_t, kDigestSize>;

    // Compute HMAC-SHA256(key, message) → 32 bytes.
    // Returns an all-zero digest on OpenSSL error (logged once at warn).
    static Digest Sign(std::span<const std::uint8_t> key,
                       std::span<const std::uint8_t> message);

    // Convenience overload for std::string-encoded message.
    static Digest Sign(std::span<const std::uint8_t> key,
                       std::string_view message);

    // Constant-time digest compare. Same length implied (both 32 bytes);
    // returns true when every byte matches.
    static bool Verify(const Digest& a, const Digest& b);

    // Hex encoding helpers (lowercase, no separators).
    static std::string ToHex(const Digest& d);
    static std::string ToHex(std::span<const std::uint8_t> bytes);

    // Parse 64-char hex string → 32-byte digest. Returns false on
    // malformed input (wrong length / non-hex char).
    static bool FromHex(std::string_view hex, Digest& out);

    // Parse arbitrary-length hex string → byte vector. Used for the
    // shared secret which can be any length (≥32 bytes recommended).
    static std::vector<std::uint8_t> HexToBytes(std::string_view hex);
};

} // namespace fourstory::security
