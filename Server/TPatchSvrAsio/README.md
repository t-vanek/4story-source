# TPatchSvrAsio — modernized 4Story patch metadata server

Production port of `Server/TPatchSvr/`. Serves patch metadata
(`TVERSION`, `TPREVERSION`, `TINTERFACECHART`) to the legacy client and
to `TControlSvr`. Server-server protocol only — the client downloads
the actual patch payloads over FTP using the URL we hand back in
`CT_PATCH_ACK`.

## Status — all 9 `CT_*` handlers ported

Wire-format byte-for-byte compatible with the legacy server. Codec is
plain (no RC4): legacy `PACKETHEADER` is `WORD wSize; WORD wID; DWORD
dwChkSum;` — we replicate the legacy `CPacket::Encrypt` byte-sum
checksum so the client's framing check passes.

> **Round 2 audit (2026-05-20):** all six TPATCH audit items closed
> (P-1…P-4 in round 1, P-5 + P-6 in round 2). Boot-time schema
> validator (SQL_AUDIT F5) wired in `main.cpp`; periodic
> stale-client sweep on a 60-second cap; pre-version promotion
> implemented inline via MERGE+DELETE in a transaction. See
> [`_rewrite/docs/TPATCH_AUDIT.md`](../../_rewrite/docs/TPATCH_AUDIT.md).

| Handler | Wire | Backed by |
|---|---|---|
| `CT_SERVICEMONITOR_ACK` | `INT64 pad, DWORD tick` → `DWORD tick, DWORD session, DWORD user, DWORD active` | live counter |
| `CT_SERVICEDATACLEAR_ACK` | no payload | no-op (no derived cache) |
| `CT_PATCH_REQ` | `DWORD dwVersion` | `PatchRepository::ListPatchesSince` |
| `CT_NEWPATCH_REQ` | `DWORD dwVersion` | `PatchRepository::ListPatchesSince` (newpatch view) |
| `CT_CHANGEIF_REQ` | `BYTE bOption` | `PatchRepository::ListInterfaceFiles` |
| `CT_PREPATCH_REQ` | `DWORD dwBetaVer` | `PatchRepository::ListPrePatchesSince` + `MinBetaVersion` |
| `CT_PATCHSTART_REQ` | empty | close session (legacy `EC_SESSION_EXIT`) |
| `CT_CTRLSVR_REQ` | empty | heartbeat |
| `CT_PREPATCHCOMPLETE_REQ` | `DWORD dwBetaVer` | `PatchRepository::MarkPreVersionComplete` |

The `ftp_url`, `pre_ftp_url`, and login-server `host:port` come from the
TOML and are advertised back to the client inside the patch acks (just
as legacy `TPatchSvrModule` does from its registry config).

## Cluster self-registration

When the operator wires `[cluster]` in the TOML the binary dials
TControlSvrAsio on startup and announces itself via the modern
`CT_PEER_REGISTER_REQ` flow (`fourstory::cluster::PeerClient`).
Service type byte is fixed at 3 (`kPatchSvr`); `control_host`
empty disables registration (standalone, legacy behavior).

## Database

Single connection pool against **TGLOBAL** — patch metadata lives there:

* `TVERSION` — released patch files; one row per (version, file)
* `TPREVERSION` — pre-release / beta files
* `TUSER_INTERFACE` — UI / interface files keyed by option byte
  (optional; the repository returns empty if the table isn't
  deployed and the schema validator only emits a warning)
* `TMinBetaVer` SP — min-beta version constraint (returns 0 when missing)
* `TPreCompleteAdd` SP — pre-version promotion (optional; modern
  inlines the equivalent MERGE+DELETE so deploys without the SP
  work fine)

Reads are independent (each handler takes its own pool lease);
`MarkPreVersionComplete` runs `MERGE INTO TVERSION … DELETE FROM
TPREVERSION` inside a single `soci::transaction`.

### Boot-time schema validator

`tpatchsvr::db::ValidateGlobalSchema` (in `db/schema_validator.cpp`)
runs once between pool construction and the listener opening. It
fails fast on missing columns of TVERSION / TPREVERSION, and emits a
warning if TUSER_INTERFACE is absent — paralleling the validator
TLoginSvrAsio already shipped (SQL_AUDIT F5).

### Schema fixtures

`schema/patch-tables.sql` ships idempotent MSSQL DDL for the three
tables plus the two stored procedures. Apply on a fresh dev DB with:

```powershell
sqlcmd -S localhost -E -d tpatchsvr_dev -i Server\TPatchSvrAsio\schema\patch-tables.sql
```

### Stale-client sweep

`PatchServer` maintains a weak_ptr registry of live sessions. On
every `CT_SERVICEMONITOR_ACK` heartbeat (legacy semantics) and as a
60-second periodic safety net, the server closes non-server sessions
whose `connected_at` is older than 60 seconds — matching legacy
`OnCT_SERVICEMONITOR_ACK`'s `m_dwTick` purge.

## Tests

Two test binaries under `tests/`:

| Test | Backend needed | Notes |
|---|---|---|
| `test_tpatchsvr_asio_stale_sweep` | none | Pure-unit sweep semantics over loopback sockets. |
| `test_tpatchsvr_asio_soci_repository` | PG and/or MSSQL | Schema validator + P-1/P-2/P-4 regressions + P-5 promote-pre-version (insert + upsert paths). Skips silently when neither `TPATCHSVR_TEST_PG_CONN` nor `TPATCHSVR_TEST_MSSQL_CONN` is set. |

Apply `schema/patch-tables.sql` to the test DB before the repository
test or it will fail at the validator step.

## Configuration

```toml
[server]
port = 3715                          # legacy DEF_PATCHPORT (TSERVER bType=5)

[patch]
ftp_url     = "ftp://127.0.0.1/patches"
pre_ftp_url = "ftp://127.0.0.1/patches/pre"

[login]
host = "127.0.0.1"                   # advertised to client after patching
port = 4816

[health]
port = 8915                          # /healthz HTTP; 0 disables

[log]
level = "info"

[database]                           # TGLOBAL — patch metadata
backend = "odbc"
connection_string = "DRIVER={ODBC Driver 17 for SQL Server};SERVER=localhost;DATABASE=TGLOBAL_RAGEZONE;Trusted_Connection=yes;TrustServerCertificate=yes"
pool_size = 4
```

## Build

Part of the root CMake project. From the repo root:

```powershell
cmake --build build --config Release --target tpatchsvr_asio
build\bin\Release\tpatchsvr_asio.exe --config Server\TPatchSvrAsio\tpatchsvr.toml
```

`fourstory_common` (the shared SOCI pool + health endpoint + audit
plumbing) is picked up automatically.

## What's intentionally not ported

| Legacy piece | Why skipped |
|---|---|
| `RegCrypt` registry config | Replaced by TOML |
| `Win32 IOCP` reactor | Replaced by Boost.Asio coroutines |
| Direct FTP server | Out of scope — operators run a real FTP daemon and we hand the URL back to the client |

## Architecture

```
Boost.Asio io_context
├── PatchServer (port 3715)
│   ├── PatchSession registry (weak_ptr, mutex-guarded)
│   ├── StaleClientSweepLoop (60s tick)
│   └── PatchSession (per peer)
│       └── dispatch → handlers::On* coroutines
├── HealthEndpoint (port 8915) — /healthz
└── signal_set (SIGINT/SIGTERM) → io.stop()

Services:
└── fourstory::db::SessionPool (TGLOBAL)
    ├── tpatchsvr::db::ValidateGlobalSchema (boot fail-fast)
    └── PatchRepository — TVERSION / TPREVERSION / TUSER_INTERFACE
```
