# Peer Protocol Plan — TLS, Tokens, gRPC

Date: 2026-05-21
Status: Design — not yet implemented
Scope: server-to-server (peer) communication only; client-server wire
protocol stays untouched for legacy `TClient` compatibility.

## TL;DR

The cluster's peer links today run **plaintext TCP** over the legacy
4Story wire codec, with a one-shot HMAC handshake (`PeerAuthToken`)
that grants the connection unconditional authority for its TCP
lifetime. The audit in
[`claude/design-improvements-Y810Y`](../) surfaced three problems
with this:

1. No transport encryption — MITM exposure once the cluster leaves
   a single trusted VLAN.
2. Coarse authorization — every authenticated peer can call every
   handler; no scope/capability granularity.
3. Long-lived credential — leaked HMAC secret = trvalý průnik until
   the operator manually disables the row in `TPEER_AUTH`.

This document plans a **three-phase migration** to mutual-TLS-protected,
token-authorized, eventually gRPC-based peer communication. Each phase
ships value on its own; the project never enters a half-migrated state
that blocks releases.

| Phase | Scope | Effort | Risk | Lands… |
|---|---|---|---|---|
| **A — TLS wrap** | `asio::ssl::stream<tcp::socket>` under existing `AsioSession`; per-peer X.509 cert; mutual TLS | M | Low | Encryption + peer cert auth, no API change for handlers |
| **B — Token model** | `TPEER_TOKEN` table, access + refresh tokens, scope bitmask, per-packet token validation | L | Med | Short-lived credentials, scoped authorization, audit trail |
| **C — gRPC migration** | `.proto` for peer RPC surface, code-gen, handler-by-handler port | XL | Med-High | Industry-standard transport, observability, streaming |

A and B can land independently; C builds on both. Client-server traffic
is **out of scope** in every phase — `TClient` is shipped binary, the
wire protocol stays bit-exact.

## Current State (audit baseline)

### Transport

- Plain TCP, `boost::asio::ip::tcp::socket` per session.
- Wire codec: 16-byte header + XOR-encrypted body, sequence-numbered
  per direction, checksum-verified. Same codec as client links, only
  the RC4 layer is disabled for peer sessions
  (`AsioSession::EnableInboundRC4` / `EnableOutboundRC4` not called).
- Documented at `Lib/Own/TNetLib/TNetLib/asio_session.h:50-51`:
  > Server-to-server links run plaintext; client-facing links run encrypted

### Authentication

`PeerAuthToken` ([`Lib/Own/FourStoryCommon/fourstory/security/peer_auth_token.h`](../../Lib/Own/FourStoryCommon/fourstory/security/peer_auth_token.h)):

- HMAC-SHA256 over `(timestamp, nonce, peer_type, group_id, server_id, remote_ip)`
- Signed with master key from `[security] master_key_env` (env var)
  or `master_key_hex` (TOML fallback)
- Per-peer secret + IP allowlist in `TPEER_AUTH` table
- Verified once at handshake; session stays open until TCP closes

After verification, `PeerSecurityGate` (post-Phase 2 — `shared_mutex`
hot path, asymmetric ±freshness window) sees no further traffic — the
session is implicitly trusted for everything.

### Authorization

None. A peer that completes the handshake can invoke every handler on
every server. No scope, no capability flags, no per-RPC permission
check.

### Lifecycle

| Event | Today |
|---|---|
| Network blip | Whole TCP drops; peer reconnects from scratch (full handshake) |
| Credential rotation | Operator updates `TPEER_AUTH.secret_hex`, restarts peer |
| Credential leak | Trvalý průnik until manual disable |
| Per-request audit | None — only handshake outcome is logged to `TPEER_AUTH_LOG` |

## Phase A — TLS Wrap

### Goal

Mutual-TLS-protected peer transport with **zero handler refactor**.
The existing `AsioSession` API (`Run`, `RunPackets`, `SendPacket`,
`Close`) stays unchanged; under the hood the socket becomes an
`asio::ssl::stream`.

### Design

```
existing:          AsioSession (m_socket = tcp::socket)
                                    │
                                    └── plain TCP

phase A:           AsioSession<Transport>
                                    │
                   ┌────────────────┼────────────────┐
                   │                │                │
              tcp::socket  ssl::stream<tcp::socket>  (future: gRPC)
              ↑ client-server     ↑ peer
```

Either:

1. **Template `AsioSession` on transport** — `AsioSession<tcp::socket>`
   for clients, `AsioSession<asio::ssl::stream<tcp::socket>>` for
   peers. Code-gen blowup is acceptable (two instantiations only); all
   read/write paths already use `boost::asio::async_*` which accepts
   any AsyncStream.
2. **Two derived classes** — `PlainSession`, `TlsSession`, sharing a
   common interface. More boilerplate but easier to read.

**Recommendation: option 1.** The existing codec doesn't depend on
socket-specific APIs (no `remote_endpoint` in the read loop), so the
template specialization is mechanical. One caveat: `RemoteIPv4()`
needs to query the underlying `tcp::socket` via
`m_socket.next_layer().remote_endpoint()` in the TLS case — adapter
function in TNetLib.

### Cert management

Per-peer X.509 certificate signed by a project-internal CA. Cert
bundle lives under `/etc/4story/tls/`:

```
/etc/4story/tls/
├── ca.crt              # CA cert (shipped, identical on every peer)
├── peer-${name}.crt    # per-peer cert (CN = peer name, e.g. "tloginsvr-eu-1")
├── peer-${name}.key    # per-peer private key (mode 0600)
└── revoked.crl         # optional CRL, polled every N minutes
```

CA bootstrap: `tools/gen_peer_ca.sh` generates the CA + N peer certs
in one shot. Operators can also bring their own PKI; the CA cert path
is configurable in `[security]`.

### Verification

Mutual TLS:

- Server side: `verify_peer | verify_fail_if_no_peer_cert`
- Verification callback checks: CN matches `TPEER_AUTH.peer_name`,
  not on CRL, not expired
- `PeerSecurityGate::CheckIp` and `CheckToken` still run **on top**
  of TLS — defense in depth, and they remain authoritative for
  business-level peer identity (group_id/server_id mapping)

### Config surface

Additive in `[security]`:

```toml
[security]
# ── transport (new) ─────────────────────────────────
peer_tls_enabled       = true
peer_tls_ca_cert       = "/etc/4story/tls/ca.crt"
peer_tls_peer_cert     = "/etc/4story/tls/peer-tloginsvr-eu-1.crt"
peer_tls_peer_key      = "/etc/4story/tls/peer-tloginsvr-eu-1.key"
peer_tls_crl           = ""           # optional
peer_tls_min_version   = "1.3"        # 1.2 | 1.3
peer_tls_ciphers       = ""           # empty = OpenSSL default
```

When `peer_tls_enabled = false` the existing plaintext code path runs
(rollout switch). Once every peer is on TLS, the flag becomes
mandatory and `false` is rejected by `Validate()`.

### Migration strategy

1. Land template `AsioSession` change. Plain TCP still works
   everywhere; nothing observable changes.
2. Ship `tools/gen_peer_ca.sh` + docs.
3. Per-peer rollout: enable `peer_tls_enabled = true` on one peer at
   a time, point a peer to it, verify handshake. Keep plain fallback
   until every peer is converted.
4. Flip default to `true` in `SecurityConfig::Validate()`; plain
   fallback removed in a follow-up PR.

### Test strategy

- New `tnetlib_asio_tls` test target: server with self-signed cert,
  client with cert signed by same CA, full packet round-trip
  (mirror of current `TestPacketRoundtripWithRC4`).
- Negative tests: client with no cert → rejected; client with cert
  signed by different CA → rejected; expired cert → rejected.
- CI integration: pre-generated test certs checked into
  `Lib/Own/TNetLib/tests/fixtures/` (NOT real keys — generated
  fresh on first CI run).

### Open questions

- **OCSP stapling**: nice to have, not in v1. CRL polling is simpler.
- **Cert rotation without restart**: needs `SSL_CTX` reload on SIGHUP.
  Doable, lands as a Phase A.1 follow-up if needed.
- **Boost version**: Boost.Asio TLS needs OpenSSL ≥ 1.1; already
  required by `tnetlib_crypto.cpp`. No new dependency.

## Phase B — Token Model

### Goal

Replace "session = TCP connection = unconditional authority" with:

- **Access token** — short-lived (default 1h), bound to peer identity,
  carries scope bitmask, presented per-packet
- **Refresh token** — longer-lived (default 24h), single-use, used to
  obtain a fresh access+refresh pair
- **Token rotation** — peer requests fresh access ~10 min before
  expiry; no TCP drop, no full re-handshake

### Wire change

Packet header gains an 8-byte `token_id` field + 4-byte truncated MAC
(`HMAC-SHA256(token_secret, dwNumber || wId || body)[:4]`). New
header layout for peer sessions (client sessions unchanged):

```
existing 16-byte PacketHeader:
+---------+---------+----------+-----------+
| wSize   | wId     | dwNumber | llChecksum|
| 2B      | 2B      | 4B       | 8B        |
+---------+---------+----------+-----------+

peer 28-byte PacketHeader (only when peer_token_required = true):
+---------+---------+----------+-----------+-----------+--------+
| wSize   | wId     | dwNumber | llChecksum| token_id  | tok_mac|
| 2B      | 2B      | 4B       | 8B        | 8B        | 4B     |
+---------+---------+----------+-----------+-----------+--------+
```

`PacketHeader` becomes role-aware. Implementation: separate
`PeerPacketHeader` type, `AsioSession` switches at construction based
on `PeerType` (already exists for the RC4 split).

### Handshake → token issuance

```
peer_initiator                              peer_acceptor
      │                                            │
      │── TCP + TLS handshake (Phase A)            │
      │                                            │
      │── PeerAuthToken (existing handshake) ────► │
      │                                  validates │
      │                       generates token pair │
      │ ◄─── TokenGrant { access, refresh } ──────│
      │                                            │
      │── wire packet w/ access.token_id ────────► │
      │                          validates token,  │
      │                          checks scope,     │
      │                          dispatches handler│
```

### Storage

`TPEER_TOKEN` (new table):

```sql
CREATE TABLE TPEER_TOKEN (
    token_id       BINARY(8)     PRIMARY KEY,    -- random, server-issued
    token_secret   BINARY(32)    NOT NULL,       -- for MAC validation
    kind           TINYINT       NOT NULL,       -- 1=access, 2=refresh
    peer_group     TINYINT       NOT NULL,       -- foreign key shape to TPEER_AUTH
    peer_server    TINYINT       NOT NULL,
    peer_type      TINYINT       NOT NULL,
    scope          BIGINT        NOT NULL,       -- bitmask, see below
    issued_at      DATETIME2     NOT NULL,
    expires_at     DATETIME2     NOT NULL,
    refresh_of     BINARY(8)     NULL,           -- access tokens: ptr to refresh
    consumed_at    DATETIME2     NULL,           -- refresh tokens: when used (single-use)
    issued_to_ip   VARCHAR(45)   NOT NULL,
    revoked_at     DATETIME2     NULL,
    revoked_reason NVARCHAR(128) NULL
);
CREATE INDEX IX_TPEER_TOKEN_peer ON TPEER_TOKEN(peer_group, peer_server, kind);
CREATE INDEX IX_TPEER_TOKEN_expires ON TPEER_TOKEN(expires_at);
```

Hot lookup: `token_id → entry`. Caches:

- L1: per-session pointer (no lookup at all after first packet)
- L2: process-wide `unordered_map<token_id, TokenEntry>`, lazy-loaded
  from DB, evicted on revoke / expiry sweep
- L3: DB (truth)

L2 sweep cadence: every 60s, drop expired and consumed-refresh rows.
Token issuance writes through to DB synchronously (token must survive
process crash to honor refresh).

### Scope bitmask

Each handler annotated with required scope at registration. Example:

```cpp
// MapServer
RegisterPeerHandler(MSG_PEER_MAP_PUSH,    PeerScope::PushMap,    OnPeerMapPush);
RegisterPeerHandler(MSG_PEER_MAP_READ,    PeerScope::ReadMap,    OnPeerMapRead);
RegisterPeerHandler(MSG_PEER_ADMIN_KICK,  PeerScope::ClusterAdmin, OnAdminKick);
```

```cpp
enum class PeerScope : std::uint64_t {
    None             = 0,
    ReadAudit        = 1ULL << 0,
    WriteAudit       = 1ULL << 1,
    ReadMap          = 1ULL << 2,
    PushMap          = 1ULL << 3,
    ReadCharRoster   = 1ULL << 4,
    ClusterAdmin     = 1ULL << 8,
    // … reserved up to bit 63
};
```

Per-peer scope is set in `TPEER_AUTH` (new column `default_scope`),
issued tokens inherit it. Operator can issue ad-hoc reduced-scope
tokens via admin shell (e.g. for one-shot maintenance task).

### Refresh flow

Peer-initiated, before expiry:

```
peer_initiator                            peer_acceptor
      │                                          │
      │── packet w/ token_id (still valid) ────► │
      │── … normal traffic …                     │
      │                                          │
      │── TokenRefreshReq { refresh_token } ───► │
      │            validates, marks consumed,    │
      │            issues new (access, refresh)  │
      │ ◄── TokenGrant { access', refresh' } ───│
      │── packet w/ token_id' ─────────────────► │
```

If refresh token has expired or been consumed, peer falls back to
full re-handshake (`PeerAuthToken`).

### Failure modes

| Scenario | Behavior |
|---|---|
| Token expired mid-session | Drop packet, send `TokenExpiredErr`, peer must refresh |
| Token revoked | Drop packet, close session |
| Wrong scope for handler | Drop packet, audit log, do NOT close (handler-level error) |
| MAC mismatch | Drop packet, close session (tampering / wrong key) |
| Process crash | Issued tokens survive in DB; peers reconnect via TLS + present existing access token |

### Config surface

```toml
[security]
peer_token_required            = false   # rollout flag, becomes mandatory once stable
peer_token_access_ttl_seconds  = 3600    # 1 h
peer_token_refresh_ttl_seconds = 86400   # 24 h
peer_token_refresh_at_ratio    = 0.83    # refresh when access has 17% lifetime left (10 min @ 1h)
peer_token_sweep_interval_seconds = 60
peer_token_storage             = "db"    # "db" | "memory" (memory = volatile, simpler dev)
```

### Test strategy

- `test_token_lifecycle`: issue → use → refresh → use new → old access rejected
- `test_token_scope`: handler with `PushMap` scope, token without → rejected
- `test_token_revocation`: revoke → in-flight requests OK, next request rejected
- `test_token_persistence`: issue token, restart process, reconnect, use existing token
- `test_token_concurrent_refresh`: two coroutines refresh simultaneously, only one succeeds (single-use enforcement)

### Open questions

- **Refresh on multi-process peers**: if peer is HA with multiple
  processes, who refreshes? Sticky session (always same process) vs
  shared token state. Sticky is simpler; cluster doesn't yet have
  HA peers, so defer.
- **Token revocation propagation**: revoke in DB → other processes
  see it on next L2 cache miss (60s window). Acceptable for v1; if
  faster propagation needed, add pub/sub on revoke (NATS / DB CDC).
- **MAC over body**: includes encrypted body? Plaintext body? **Recommend
  encrypted** so MAC binds to wire bytes, not pre-encrypt state.
  Cheaper to validate (no second decrypt).

## Phase C — gRPC Migration

### Goal

Replace the legacy 4Story wire codec on peer links with **gRPC over
HTTP/2 over mTLS**. Code-generated stubs, typed RPC, built-in
streaming, observability stack works out of the box.

Client links **never migrate** — `TClient` binary is shipped and
speaks the legacy codec; that's the project's preservation mandate.

### Design

Per peer RPC, a `.proto` definition:

```protobuf
syntax = "proto3";
package fourstory.peer.v1;

import "google/protobuf/timestamp.proto";

service MapPeer {
    // Push a new map shard to a target MapSvr.
    rpc PushMap(PushMapReq) returns (PushMapResp);

    // Stream live mob updates to the LogSvr for audit.
    rpc StreamMobAudit(stream MobAuditEvent) returns (StreamSummary);
}

service AdminPeer {
    rpc KickPlayer(KickPlayerReq) returns (KickPlayerResp);
    rpc BroadcastMessage(BroadcastReq) returns (BroadcastResp);
}

message PushMapReq {
    bytes  access_token_id = 1;       // 8B, validated server-side
    bytes  access_token_mac = 2;      // 4B over body
    uint32 map_id = 10;
    bytes  payload = 11;
    google.protobuf.Timestamp issued = 12;
}
```

Tokens flow in **gRPC metadata**, not RPC body — they apply to every
call uniformly. Server-side interceptor validates token + scope +
maps `peer_group/peer_server/peer_type` into a `PeerIdentity` passed
to the handler. Handlers stay business-logic only.

### Migration strategy — handler by handler

1. Land `.proto` definitions for one peer surface (e.g.
   `LogSvr` audit ingestion — small, well-bounded, no client impact).
2. Generate stubs into `Lib/Own/TProtocol/grpc/`; check generated
   code in (dev machines may not all have protoc).
3. Stand up gRPC server alongside existing legacy listener on a new
   port. Peer initiator picks transport based on
   `[peer] use_grpc = true|false`.
4. Migrate handlers from legacy `AsioSession::RunPackets` dispatch
   to gRPC server interceptor. Each handler is one PR.
5. Once every handler on a service is gRPC, retire its legacy
   listener. Peer-link `AsioSession` stays for backward compat with
   non-migrated services during the transition.

### Coexistence

During the migration the cluster has **two peer transports running in
parallel**. Acceptable because:

- Peer topology is static; each peer pair negotiates which transport
  to use via config
- gRPC port is separate (e.g. 4501 next to legacy 4500)
- Both share the same TLS cert + token store (Phase A + B
  infrastructure)

### Observability

gRPC gives us free:

- Per-call timing, status code, error details
- Distributed tracing (OpenTelemetry interceptor)
- Health probe via `grpc.health.v1.Health`
- Reflection for `grpcurl`-style ad-hoc debugging

These are the missing operability bits that the legacy wire codec
will never have.

### Config surface

```toml
[peer]
listen_grpc_port   = 4501
grpc_max_msg_size  = 16777216    # 16 MB
use_grpc           = false       # initiator side: speak gRPC iff peer supports it
```

### Test strategy

- gRPC integration tests run in-process via `grpc::InProcessChannel`
- Token interceptor reuses Phase B test fixtures
- Migration parity: for each migrated handler, side-by-side test with
  legacy `AsioSession` dispatch to verify identical observable
  behavior

### Open questions

- **Generated code in repo vs build-time**: build-time is cleaner but
  every dev needs protoc + grpc-cpp installed. **Recommend: check in
  generated code**, regenerate via Makefile target.
- **Proto evolution**: `proto3` is forward-compatible by design;
  schema changes are safe as long as field numbers aren't reused.
- **gRPC dependency size**: significant (~30 MB of headers + libs)
  but already pulled in by some vcpkg deps transitively. Worth the
  size for the observability + tooling story.

## Schema Migration

All three phases are additive to the SQL schema. Migrations:

- **Phase A** — no schema changes (cert files on disk only)
- **Phase B** — `CREATE TABLE TPEER_TOKEN` + `ALTER TABLE TPEER_AUTH
  ADD COLUMN default_scope BIGINT NOT NULL DEFAULT 0`
- **Phase C** — no schema changes (gRPC is transport)

Rollout: standard additive migration story documented in
`MODERNIZATION_PLAN.md`.

## Cross-Cutting Concerns

### Backward compatibility

Every phase ships with a compatibility flag (`peer_tls_enabled`,
`peer_token_required`, `use_grpc`) that defaults to **off** in the
first release where it's available. Operators opt in per peer pair.
Flag flips to mandatory only after the full cluster is migrated,
typically two releases later.

### Configuration migration

`[security]` block keeps growing. Once stable, consider splitting
into `[security.transport]`, `[security.tokens]`, `[security.peer]`
for readability.

### Audit

Every phase emits to `TPEER_AUTH_LOG` (existing) plus:

- Phase A — TLS handshake outcomes (issuer DN, verify result)
- Phase B — token issuance, refresh, revocation, scope-deny events
- Phase C — RPC-level audit via gRPC interceptor

### Operator UX

- Phase A — add `tools/gen_peer_ca.sh` + `docs/PEER_TLS_SETUP.md`
- Phase B — admin shell commands: `token issue`, `token revoke`,
  `token list`, `scope show <peer>`
- Phase C — `grpcurl` recipes in operator guide

## Effort Estimate

Per the existing roadmap convention (small team, no full-time
dedication):

| Phase | Engineering | Testing | Docs | Total |
|---|---|---|---|---|
| A — TLS wrap | 2 weeks | 1 week | 0.5 week | **~4 weeks** |
| B — Token model | 4 weeks | 2 weeks | 1 week | **~7 weeks** |
| C — gRPC migration | 8 weeks (initial) + N×1 week per handler | — | — | **3-6 months** |

A and B are tractable as background work alongside other modernization
streams. C is a major project — only commit once the cluster is on a
stable post-Phase B baseline and there's a clear operability case
(metrics, tracing, multi-region).

## Decision Log

| Date | Decision | Rationale |
|---|---|---|
| 2026-05-21 | Order: A → B → C | TLS is the most independent piece and unblocks B (token distribution needs secure transport). C builds on token model. |
| 2026-05-21 | Token storage = DB-backed | Survives restart; refresh flow remains correct across peer process bounces. |
| 2026-05-21 | gRPC for peer only, not client | `TClient` is shipped binary; legacy wire codec stays for client-server. |
| 2026-05-21 | Mutual TLS, not server-only | Peers are first-class identities. Server-only TLS would still require a separate peer auth layer. Better to identify at TLS layer. |
| 2026-05-21 | Asymmetric token TTLs (1h access / 24h refresh) | Standard OAuth-ish lifetimes; balances UX (no constant re-auth) and blast radius (leak window). |

## References

- Audit findings: see `claude/design-improvements-Y810Y` branch
  (PRs #39, #40) for the security/perf/bug nálezy that motivated
  this plan.
- Existing peer security primitives:
  - [`fourstory/security/peer_security_gate.h`](../../Lib/Own/FourStoryCommon/fourstory/security/peer_security_gate.h)
  - [`fourstory/security/peer_auth_token.h`](../../Lib/Own/FourStoryCommon/fourstory/security/peer_auth_token.h)
  - [`fourstory/security/security_config.h`](../../Lib/Own/FourStoryCommon/fourstory/security/security_config.h)
- TNetLib session: [`TNetLib/asio_session.h`](../../Lib/Own/TNetLib/TNetLib/asio_session.h)
- Cluster overview: [`MODERNIZATION_PLAN.md`](MODERNIZATION_PLAN.md)
