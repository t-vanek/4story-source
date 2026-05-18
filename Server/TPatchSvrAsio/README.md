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

## Database

Single connection pool against **TGLOBAL** — patch metadata lives there:

* `TVERSION` — released patch files; one row per (version, file)
* `TPREVERSION` — pre-release / beta files
* `TINTERFACECHART` — UI / interface files keyed by option byte (optional;
  the repository returns empty if the table isn't deployed)
* `TMinBetaVer` SP — min-beta version constraint (returns 0 when missing)

Read-only, except `MarkPreVersionComplete` which calls the
`CSPPreComplete` SP equivalent.

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
│   └── PatchSession (per peer)
│       └── dispatch → handlers::On* coroutines
├── HealthEndpoint (port 8915) — /healthz
└── signal_set (SIGINT/SIGTERM) → io.stop()

Services:
└── fourstory::db::SessionPool (TGLOBAL)
    └── PatchRepository — TVERSION / TPREVERSION / TINTERFACECHART
```
