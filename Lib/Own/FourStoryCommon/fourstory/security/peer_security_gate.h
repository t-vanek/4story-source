#pragma once

// PeerSecurityGate — the single decision point for every inbound
// server-to-server connection.
//
// Composition:
//   * IpAllowlist             — fast IP filter (always evaluated)
//   * SecurityConfig          — operator-tunable policy
//   * PeerAuthRepository      — DB-backed trust store
//   * NonceCache              — replay protection
//
// Lifecycle:
//   1. Construct once in main.cpp after config + DB pool are ready.
//   2. Call LoadTrustStore() before accepting peers (boot warm-up).
//   3. On every inbound peer connection:
//        a. CheckIp(remote_ip)            — pre-handshake reject
//        b. CheckToken(token, remote_ip)  — post-handshake decide
//   4. Audit all denied outcomes via the repository's TPEER_AUTH_LOG
//      table; allowed connections only count successes in dwOkCount.
//
// Thread safety: the trust map is rebuilt under a mutex on
// LoadTrustStore(); CheckToken takes a shared read lock for lookup.

#include "ip_allowlist.h"
#include "peer_auth_repository.h"
#include "peer_auth_token.h"
#include "security_config.h"

#include "fourstory/db/session_pool.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fourstory::security {

struct PeerCheckResult
{
    PeerAuthOutcome outcome     = PeerAuthOutcome::Allow;
    std::string     peer_name;          // resolved from DB row when known
    std::string     reason;             // human-readable detail
    bool            allowed() const { return outcome == PeerAuthOutcome::Allow; }
};

class PeerSecurityGate
{
public:
    PeerSecurityGate(SecurityConfig                     cfg,
                     fourstory::db::SessionPool*        pool = nullptr)
        : m_cfg(std::move(cfg))
        , m_ip_allowlist(!m_cfg.ip_allowlist_enforce)
        , m_repo(pool ? std::make_unique<PeerAuthRepository>(*pool) : nullptr)
        , m_nonce_cache(m_cfg.nonce_window)
        , m_master_key(m_cfg.ResolveMasterKey())
    {
        m_ip_allowlist.AddAll(m_cfg.ip_allowlist);
    }

    // Reload TPEER_AUTH into the in-memory trust map. Safe to call
    // multiple times; replaces the map atomically.
    std::size_t LoadTrustStore()
    {
        if (!m_repo || !m_cfg.db_trust_store) return 0;
        try
        {
            const auto rows = m_repo->LoadAll();
            std::unordered_map<std::uint64_t, TrustEntry> next;
            for (const auto& r : rows)
            {
                if (!r.enabled) continue;
                TrustEntry te;
                te.row = r;
                // Per-peer IP allowlist (CIDR list, comma-separated).
                IpAllowlist al(true);  // empty per-peer list → fall through to global
                for (auto& s : SplitCsv(r.ip_allowlist))
                    al.Add(s);
                te.peer_ip = std::move(al);
                // Secret bytes decoded once at load.
                te.secret = Hmac::HexToBytes(r.secret_hex);
                next.emplace(Key(r.group_id, r.server_id), std::move(te));
            }
            std::lock_guard<std::mutex> lk(m_mtx);
            m_trust = std::move(next);
            spdlog::info("peer_security_gate: trust store loaded — "
                         "{} enabled peer(s)", m_trust.size());
            return m_trust.size();
        }
        catch (const std::exception& ex)
        {
            spdlog::error("peer_security_gate: LoadTrustStore failed: {}",
                ex.what());
            return 0;
        }
    }

    // Pre-handshake IP filter. Always cheap; runs on the accept loop's
    // thread immediately after accept().
    PeerCheckResult CheckIp(std::string_view remote_ip) const
    {
        PeerCheckResult r;
        if (!m_cfg.ip_allowlist_enforce && m_ip_allowlist.Empty())
            return r;  // legacy "allow all" path
        if (m_ip_allowlist.Allows(remote_ip))
            return r;
        r.outcome = PeerAuthOutcome::IpDenied;
        r.reason  = std::string("ip not in [security].ip_allowlist: ") +
                    std::string(remote_ip);
        return r;
    }

    // Full handshake decision. Verifies token against the DB-backed
    // trust store + master key + nonce cache. The remote_ip is bound
    // into the HMAC so a leaked token can't be reused elsewhere.
    PeerCheckResult CheckToken(const PeerAuthToken&   tok,
                                std::string_view       remote_ip,
                                std::uint64_t          now_unix)
    {
        PeerCheckResult res;

        if (!m_cfg.peer_auth_required)
        {
            res.peer_name = "<auth disabled>";
            return res;
        }

        // Freshness window check (timestamp ± nonce_window seconds).
        const auto window = static_cast<std::uint64_t>(m_cfg.nonce_window.count());
        if (tok.timestamp + window < now_unix ||
            tok.timestamp > now_unix + window)
        {
            res.outcome = PeerAuthOutcome::Expired;
            res.reason  = "timestamp outside ±" + std::to_string(window) + "s";
            AuditDeny(tok, remote_ip, res);
            return res;
        }

        // Identity lookup. Composite key (group, server).
        std::optional<TrustEntry> entry;
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            const auto it = m_trust.find(Key(tok.group_id, tok.server_id));
            if (it != m_trust.end()) entry = it->second;
        }
        if (!entry)
        {
            res.outcome = PeerAuthOutcome::UnknownPeer;
            res.reason  = "no enabled TPEER_AUTH row for (group=" +
                          std::to_string(tok.group_id) + ", server=" +
                          std::to_string(tok.server_id) + ")";
            AuditDeny(tok, remote_ip, res);
            return res;
        }

        // Per-peer IP allowlist (in addition to the global one).
        if (!entry->peer_ip.Empty() && !entry->peer_ip.Allows(remote_ip))
        {
            res.outcome = PeerAuthOutcome::IpDenied;
            res.reason  = std::string("peer-specific ip allowlist denied ") +
                          std::string(remote_ip);
            res.peer_name = entry->row.peer_name;
            AuditDeny(tok, remote_ip, res);
            return res;
        }

        // Type sanity — token must agree with the DB-recorded peer type.
        if (entry->row.type_id != tok.peer_type)
        {
            res.outcome = PeerAuthOutcome::UnknownPeer;
            res.reason  = "type mismatch: token=" +
                          std::to_string(tok.peer_type) + " db=" +
                          std::to_string(entry->row.type_id);
            res.peer_name = entry->row.peer_name;
            AuditDeny(tok, remote_ip, res);
            return res;
        }

        // HMAC verification — secret comes from the DB row; message is
        // recomputed from the token fields + remote_ip.
        const auto msg = PeerAuthToken::ComposeMessage(
            tok.timestamp, tok.nonce, tok.peer_type,
            tok.group_id, tok.server_id, remote_ip);
        const auto expected = Hmac::Sign(entry->secret, msg);
        if (!Hmac::Verify(tok.hmac, expected))
        {
            res.outcome = PeerAuthOutcome::BadHmac;
            res.reason  = "HMAC verification failed";
            res.peer_name = entry->row.peer_name;
            AuditDeny(tok, remote_ip, res);
            return res;
        }

        // Replay check — record only AFTER HMAC verification so an
        // attacker can't flood the nonce cache with bogus tokens.
        if (!m_nonce_cache.TryRecord(tok.peer_type, tok.timestamp,
                                      tok.nonce, now_unix))
        {
            res.outcome = PeerAuthOutcome::Replay;
            res.reason  = "nonce already seen within window";
            res.peer_name = entry->row.peer_name;
            AuditDeny(tok, remote_ip, res);
            return res;
        }

        // ✓ allow
        res.peer_name = entry->row.peer_name;
        if (m_repo && m_cfg.audit_failed_attempts)
        {
            m_repo->LogOutcome(tok.group_id, tok.server_id, tok.peer_type,
                std::string(remote_ip), PeerAuthOutcome::Allow);
        }
        return res;
    }

    const SecurityConfig& Config() const { return m_cfg; }
    bool                  Enforcing() const { return m_cfg.peer_auth_required; }
    std::size_t           TrustCount() const
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        return m_trust.size();
    }

private:
    struct TrustEntry
    {
        PeerAuthRow                row;
        IpAllowlist                peer_ip{true};   // empty = no per-peer filter
        std::vector<std::uint8_t>  secret;
    };

    static std::uint64_t Key(std::uint8_t g, std::uint8_t s)
    {
        return (static_cast<std::uint64_t>(g) << 8) | static_cast<std::uint64_t>(s);
    }

    static std::vector<std::string> SplitCsv(std::string_view s)
    {
        std::vector<std::string> out;
        std::size_t i = 0;
        while (i < s.size())
        {
            auto j = s.find(',', i);
            if (j == std::string_view::npos) j = s.size();
            // Trim whitespace
            std::size_t a = i;
            while (a < j && (s[a] == ' ' || s[a] == '\t')) ++a;
            std::size_t b = j;
            while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) --b;
            if (b > a) out.emplace_back(s.substr(a, b - a));
            i = j + 1;
        }
        return out;
    }

    void AuditDeny(const PeerAuthToken&  tok,
                   std::string_view      remote_ip,
                   const PeerCheckResult& r)
    {
        spdlog::warn("peer_security_gate: DENY ip={} outcome={} reason='{}' "
                     "claimed=(g={},s={},t={})",
            remote_ip, OutcomeName(r.outcome), r.reason,
            tok.group_id, tok.server_id, tok.peer_type);
        if (m_repo && m_cfg.audit_failed_attempts)
        {
            m_repo->LogOutcome(tok.group_id, tok.server_id, tok.peer_type,
                std::string(remote_ip), r.outcome, r.reason);
        }
    }

    SecurityConfig                                          m_cfg;
    IpAllowlist                                             m_ip_allowlist;
    std::unique_ptr<PeerAuthRepository>                     m_repo;
    NonceCache                                              m_nonce_cache;
    std::vector<std::uint8_t>                               m_master_key;
    mutable std::mutex                                      m_mtx;
    std::unordered_map<std::uint64_t, TrustEntry>           m_trust;
};

} // namespace fourstory::security
