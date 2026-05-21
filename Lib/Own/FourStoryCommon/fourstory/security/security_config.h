#pragma once

// SecurityConfig — shared shape of the [security] TOML block.
//
// Servers parse this in their config.cpp using toml++ and pass it to
// PeerSecurityGate. The shape stays uniform across all servers so an
// operator config file template works everywhere.
//
// Example tloginsvr.local.toml:
//
//   [security]
//   ip_allowlist = ["10.0.0.0/8", "127.0.0.1/32"]
//   ip_allowlist_enforce = true
//   peer_auth_required = true
//   master_key_env = "FOURSTORY_PEER_KEY"      # env var holding hex secret
//   master_key_hex = ""                         # inline fallback (dev only)
//   nonce_window_seconds = 30
//   future_window_seconds = 3
//   handshake_timeout_seconds = 5
//   audit_failed_attempts = true
//   peer_tls_enabled = false
//   peer_tls_ca_cert = "/etc/4story/tls/ca.crt"
//   peer_tls_peer_cert = "/etc/4story/tls/peer-tloginsvr-eu-1.crt"
//   peer_tls_peer_key = "/etc/4story/tls/peer-tloginsvr-eu-1.key"
//   peer_tls_min_version = "1.3"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace fourstory::security {

struct SecurityConfig
{
    // CIDR / exact-IP entries. Empty list + enforce=true → reject all.
    // Empty list + enforce=false → allow all (legacy / dev behavior).
    std::vector<std::string>  ip_allowlist;
    bool                      ip_allowlist_enforce = false;

    // Mutual peer authentication (HMAC handshake). When true, every
    // inbound server-to-server connection must present a valid
    // PeerAuthToken before any wire traffic is dispatched.
    bool                      peer_auth_required   = false;

    // Master HMAC key. Two sources, in priority order:
    //   1. master_key_env  → process env var name (production)
    //   2. master_key_hex  → inline hex string (dev only — leaks to git)
    // Both empty + peer_auth_required=true → fatal at boot.
    std::string               master_key_env       = "FOURSTORY_PEER_KEY";
    std::string               master_key_hex;

    // Token freshness window — PAST direction. A token whose timestamp
    // is more than `nonce_window` seconds behind our clock is rejected
    // as expired. Default 30 s matches the heartbeat cadence and the
    // nonce-cache retention.
    std::chrono::seconds      nonce_window         = std::chrono::seconds(30);

    // Token freshness window — FUTURE direction. A token whose
    // timestamp is more than `future_window` seconds AHEAD of our
    // clock is rejected as expired. Asymmetric on purpose: a token
    // in the future stays "fresh" once our clock catches up to it,
    // which extends an attacker's replay window beyond nonce_window.
    // Default 3 s assumes ntpd-synced peers; bump only if you have a
    // deliberate clock-skew arrangement that you can't fix at the OS
    // level (rarely the right answer).
    std::chrono::seconds      future_window        = std::chrono::seconds(3);

    // Maximum time the handshake itself may take before the socket is
    // closed. Defense against slowloris-style attacks where a client
    // dribbles bytes to keep a fd open.
    std::chrono::seconds      handshake_timeout    = std::chrono::seconds(5);

    // Per-peer credentials canonical source. true → load TPEER_AUTH at
    // boot + refresh on demand. false → static TOML map only.
    bool                      db_trust_store       = true;

    // Audit failed attempts to TPEER_AUTH_LOG. Enabled by default.
    bool                      audit_failed_attempts = true;

    // ── Peer transport (PEER_PROTOCOL_PLAN.md, Phase A) ─────────────
    // When true, peer-to-peer links wrap their TCP socket in
    // boost::asio::ssl::stream and require mutual TLS authentication.
    // Default false during Phase A rollout; flips to mandatory once
    // every peer in the cluster is on TLS.
    bool                      peer_tls_enabled     = false;

    // Trust root for verifying peer certificates. PEM file containing
    // one or more CA certs. Empty disables peer verification (NEVER
    // for production — fails Validate() when peer_tls_enabled=true).
    std::string               peer_tls_ca_cert;

    // This process's own certificate + private key, presented to the
    // remote peer during the TLS handshake. Both required when
    // peer_tls_enabled=true.
    std::string               peer_tls_peer_cert;
    std::string               peer_tls_peer_key;

    // Minimum negotiated TLS version. "1.2" | "1.3". Default 1.3
    // because every modern OpenSSL supports it and there's no legacy
    // peer constraint (peers are project-controlled binaries).
    std::string               peer_tls_min_version = "1.3";

    // ── Helpers ──────────────────────────────────────────────────────

    // Resolve master key bytes: env var first, hex fallback. Returns
    // empty when neither source is set.
    std::vector<std::uint8_t> ResolveMasterKey() const;

    // Validate the config and return a human-readable error string on
    // problem (e.g. malformed CIDR, empty key with peer_auth_required).
    // Empty return means "OK".
    std::string Validate() const;
};

} // namespace fourstory::security
