#include "fourstory/security/hmac.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <spdlog/spdlog.h>

#include <cstdio>

namespace fourstory::security {

Hmac::Digest Hmac::Sign(std::span<const std::uint8_t> key,
                        std::span<const std::uint8_t> message)
{
    Digest out{};
    unsigned int olen = 0;

    unsigned char* p = HMAC(
        EVP_sha256(),
        key.data(), static_cast<int>(key.size()),
        message.data(), message.size(),
        out.data(), &olen);

    if (p == nullptr || olen != kDigestSize)
    {
        spdlog::warn("security::Hmac::Sign: OpenSSL HMAC failed "
                     "(key_size={} msg_size={} olen={})",
            key.size(), message.size(), olen);
        Digest zero{};
        return zero;
    }
    return out;
}

Hmac::Digest Hmac::Sign(std::span<const std::uint8_t> key,
                        std::string_view message)
{
    return Sign(key,
        std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(message.data()),
            message.size()));
}

bool Hmac::Verify(const Digest& a, const Digest& b)
{
    // Constant-time: accumulate XOR diffs, never short-circuit on
    // first mismatch. Prevents timing-attack inference of which byte
    // diverged.
    std::uint8_t diff = 0;
    for (std::size_t i = 0; i < kDigestSize; ++i)
        diff |= static_cast<std::uint8_t>(a[i] ^ b[i]);
    return diff == 0;
}

std::string Hmac::ToHex(const Digest& d)
{
    return ToHex(std::span<const std::uint8_t>(d.data(), d.size()));
}

std::string Hmac::ToHex(std::span<const std::uint8_t> bytes)
{
    static const char k_hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (auto b : bytes)
    {
        out.push_back(k_hex[(b >> 4) & 0xF]);
        out.push_back(k_hex[b & 0xF]);
    }
    return out;
}

static int HexNibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

bool Hmac::FromHex(std::string_view hex, Digest& out)
{
    if (hex.size() != kDigestSize * 2) return false;
    for (std::size_t i = 0; i < kDigestSize; ++i)
    {
        const int hi = HexNibble(hex[i * 2]);
        const int lo = HexNibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    return true;
}

std::vector<std::uint8_t> Hmac::HexToBytes(std::string_view hex)
{
    if (hex.size() % 2 != 0) return {};
    std::vector<std::uint8_t> out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2)
    {
        const int hi = HexNibble(hex[i]);
        const int lo = HexNibble(hex[i + 1]);
        if (hi < 0 || lo < 0) return {};
        out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }
    return out;
}

} // namespace fourstory::security
