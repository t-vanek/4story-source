#pragma once

// PeerAuthToken — server-to-server handshake token.
//
// Wire layout (fixed 56 bytes, big-endian):
//   uint64  timestamp     unix seconds at token creation
//   uint64  nonce         random 64-bit (replay protection)
//   uint8   peer_type     legacy bType (login/log/patch/map/world)
//   uint8   group_id      legacy bGroupID
//   uint8   server_id     legacy bServerID
//   uint8   _reserved[5]  zero, padding
//   uint8   hmac[32]      HMAC-SHA256(secret, ts|nonce|type|group|server|claimed_addr)
//
// The HMAC binds the claimed identity AND the source IP, so a leaked
// token can't be replayed from a different host even within the
// freshness window.
//
// Verifier checks:
//   1. timestamp within ±nonce_window_seconds of "now"  (clock skew)
//   2. (peer_type, group_id, server_id) is known and enabled
//   3. HMAC matches recomputed value
//   4. nonce not seen in last nonce_window_seconds (replay)
//
// Failure paths feed PeerAuthLog with a discrete outcome code.

#include "hmac.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace fourstory::security {

struct PeerAuthToken
{
    static constexpr std::size_t kWireSize = 56;

    std::uint64_t  timestamp = 0;
    std::uint64_t  nonce     = 0;
    std::uint8_t   peer_type = 0;
    std::uint8_t   group_id  = 0;
    std::uint8_t   server_id = 0;
    Hmac::Digest   hmac{};

    // Compose the canonical message buffer used as HMAC input.
    // Includes the claimed_addr so binding a token to a different
    // source IP fails verification.
    static std::string ComposeMessage(std::uint64_t timestamp,
                                      std::uint64_t nonce,
                                      std::uint8_t  peer_type,
                                      std::uint8_t  group_id,
                                      std::uint8_t  server_id,
                                      std::string_view claimed_addr)
    {
        std::string msg;
        msg.reserve(8 + 8 + 3 + claimed_addr.size());
        auto append_u64 = [&](std::uint64_t v) {
            for (int i = 7; i >= 0; --i)
                msg.push_back(static_cast<char>((v >> (i * 8)) & 0xFF));
        };
        append_u64(timestamp);
        append_u64(nonce);
        msg.push_back(static_cast<char>(peer_type));
        msg.push_back(static_cast<char>(group_id));
        msg.push_back(static_cast<char>(server_id));
        msg.append(claimed_addr);
        return msg;
    }

    // Serialize this token to its 56-byte wire form (sender path).
    std::array<std::uint8_t, kWireSize> Encode() const
    {
        std::array<std::uint8_t, kWireSize> out{};
        auto put_u64 = [&out](std::size_t off, std::uint64_t v) {
            for (int i = 0; i < 8; ++i)
                out[off + i] = static_cast<std::uint8_t>((v >> ((7 - i) * 8)) & 0xFF);
        };
        put_u64(0, timestamp);
        put_u64(8, nonce);
        out[16] = peer_type;
        out[17] = group_id;
        out[18] = server_id;
        // 19..23 reserved (already zero)
        std::memcpy(out.data() + 24, hmac.data(), Hmac::kDigestSize);
        return out;
    }

    // Parse a 56-byte buffer into a PeerAuthToken (receiver path).
    static bool Decode(std::span<const std::uint8_t> bytes,
                       PeerAuthToken& out)
    {
        if (bytes.size() < kWireSize) return false;
        auto get_u64 = [&bytes](std::size_t off) {
            std::uint64_t v = 0;
            for (int i = 0; i < 8; ++i)
                v = (v << 8) | bytes[off + i];
            return v;
        };
        out.timestamp = get_u64(0);
        out.nonce     = get_u64(8);
        out.peer_type = bytes[16];
        out.group_id  = bytes[17];
        out.server_id = bytes[18];
        std::memcpy(out.hmac.data(), bytes.data() + 24, Hmac::kDigestSize);
        return true;
    }

    // Build a signed token (sender helper).
    static PeerAuthToken Build(std::span<const std::uint8_t> secret,
                               std::uint8_t  peer_type,
                               std::uint8_t  group_id,
                               std::uint8_t  server_id,
                               std::string_view source_addr,
                               std::uint64_t now_unix,
                               std::uint64_t random_nonce)
    {
        PeerAuthToken t{};
        t.timestamp = now_unix;
        t.nonce     = random_nonce;
        t.peer_type = peer_type;
        t.group_id  = group_id;
        t.server_id = server_id;
        const auto msg = ComposeMessage(t.timestamp, t.nonce,
            t.peer_type, t.group_id, t.server_id, source_addr);
        t.hmac = Hmac::Sign(secret, msg);
        return t;
    }
};

// ── Auth outcome codes ──────────────────────────────────────────────
// Mirrors the bOutcome column in TPEER_AUTH_LOG.
enum class PeerAuthOutcome : std::uint8_t
{
    Allow         = 0,
    IpDenied      = 1,
    UnknownPeer   = 2,
    BadHmac       = 3,
    Replay        = 4,
    Expired       = 5,
    Disabled      = 6,
    Malformed     = 7,
};

inline const char* OutcomeName(PeerAuthOutcome o)
{
    switch (o) {
        case PeerAuthOutcome::Allow:       return "allow";
        case PeerAuthOutcome::IpDenied:    return "ip_denied";
        case PeerAuthOutcome::UnknownPeer: return "unknown_peer";
        case PeerAuthOutcome::BadHmac:     return "bad_hmac";
        case PeerAuthOutcome::Replay:      return "replay";
        case PeerAuthOutcome::Expired:     return "expired";
        case PeerAuthOutcome::Disabled:    return "disabled";
        case PeerAuthOutcome::Malformed:   return "malformed";
    }
    return "?";
}

// ── NonceCache ───────────────────────────────────────────────────────
// Sliding-window set of recently-seen (timestamp, nonce) pairs.
// Replay protection: a token whose nonce is already in the cache is
// rejected even if its HMAC is valid. Entries expire automatically
// once their timestamp falls outside the freshness window.

class NonceCache
{
public:
    explicit NonceCache(std::chrono::seconds window = std::chrono::seconds(30))
        : m_window(window)
    {}

    // Try to record `nonce` issued at `timestamp` for `peer_type`.
    // Returns true if accepted (first time seen), false on replay.
    bool TryRecord(std::uint8_t peer_type,
                   std::uint64_t timestamp,
                   std::uint64_t nonce,
                   std::uint64_t now_unix)
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        Expire(now_unix);
        const auto key = MakeKey(peer_type, timestamp, nonce);
        return m_seen.insert(key).second;
    }

    std::size_t Size() const
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        return m_seen.size();
    }

private:
    struct Entry { std::uint64_t key; std::uint64_t expires_at; };

    static std::uint64_t MakeKey(std::uint8_t pt,
                                 std::uint64_t ts,
                                 std::uint64_t nonce)
    {
        // 56 bits of nonce + 8 bits of peer_type — collision-resistant
        // within the freshness window. Timestamp is checked separately.
        (void)ts;
        return (static_cast<std::uint64_t>(pt) << 56) | (nonce & 0x00FFFFFFFFFFFFFFull);
    }

    void Expire(std::uint64_t now_unix)
    {
        const auto cutoff = now_unix - static_cast<std::uint64_t>(m_window.count());
        for (auto it = m_expiry.begin(); it != m_expiry.end();)
        {
            if (it->expires_at <= cutoff)
            {
                m_seen.erase(it->key);
                it = m_expiry.erase(it);
            }
            else { ++it; }
        }
    }

    mutable std::mutex            m_mtx;
    std::unordered_set<std::uint64_t> m_seen;
    std::vector<Entry>            m_expiry;
    std::chrono::seconds          m_window;
};

} // namespace fourstory::security
