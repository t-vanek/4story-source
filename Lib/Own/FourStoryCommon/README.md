# FourStoryCommon — shared infrastructure for the modernized servers

Static library (`fourstory_common`) consolidating the SOCI pool, audit
plumbing, SMTP plumbing, health endpoint, rate limiter, registry
refresher, and the cluster-side `PeerClient` used by every modernized
Asio server (`TLoginSvrAsio`, `TPatchSvrAsio`, `TLogSvrAsio`,
`TMapSvrAsio`). Pulled out so each server doesn't carry its own copy.

The line-based admin shell that used to live here was moved into
`Server/TControlSvrAsio/` — operator access to the cluster is now
centralized on TControl (the per-server localhost shells were
duplicate footguns; the modern one knows about peers, routing,
streaming, and cluster lifecycle).

## Layout

Public headers live under `fourstory/` and mirror the namespace tree.
Consumers write `#include "fourstory/{db,audit,smtp,ops}/...h"`. Source
files live under `src/`.

```
Lib/Own/FourStoryCommon/
├── CMakeLists.txt
├── fourstory/                       # public include root (added to consumer include path)
│   ├── db/
│   │   ├── session_pool.h           # SOCI session pool (TGLOBAL / TGAME)
│   │   └── schema_validator.h       # CheckColumns + SchemaError exception
│   ├── audit/
│   │   ├── audit_logger.h           # IAuditLogger interface + outcome enums
│   │   ├── spdlog_audit_logger.h    # structured spdlog sink
│   │   └── udp_audit_logger.h       # legacy `_UDPPACKET` decorator
│   ├── smtp/
│   │   ├── smtp_client.h            # ISmtpClient interface
│   │   └── spdlog_smtp_client.h     # log-only default impl
│   ├── cluster/
│   │   └── peer_client.h            # outbound CT_PEER_* registry client
│   └── ops/
│       ├── health_endpoint.h        # minimal HTTP /healthz
│       ├── rate_limiter.h           # per-IP token bucket
│       └── registry_refresher.h     # periodic cache reload hook
└── src/
    ├── db/{session_pool,schema_validator}.cpp
    ├── audit/{spdlog_audit_logger,udp_audit_logger}.cpp
    ├── smtp/{spdlog_smtp_client,asio_smtp_client}.cpp
    ├── cluster/peer_client.cpp
    └── ops/{health_endpoint,rate_limiter,registry_refresher}.cpp
```

## Modules

### `fourstory::db`

* **`SessionPool`** — owns N SOCI sessions to one logical DB. Acquire
  via `auto lease = pool.Acquire(); soci::session& sql = *lease;`. The
  lease auto-releases on scope exit. Backends: `Backend::Odbc` for
  Windows ODBC + unixODBC; older `Sqlite3` / `Postgres` enum values are
  preserved for future re-enablement (vcpkg feature set drops them right
  now).
* **`schema_validator::CheckColumns`** — confirms every `(table,
  column)` pair listed exists in `INFORMATION_SCHEMA`. Throws
  `SchemaError` listing missing entries. Table/column names are inlined
  into the SQL (not parameter-bound) because the standard ODBC binding
  path miscompares against `sysname`/NVARCHAR columns; inputs come from
  compile-time constant lists so direct substitution is safe.

Per-server validator entry points (e.g.
`tloginsvr::db::ValidateGlobalSchema`) live in each server's `db/`
directory; this lib carries only the framework.

### `fourstory::audit`

* **`IAuditLogger`** — sink interface for login / char-create /
  char-delete / map-handoff events. Outcome enums
  (`LoginOutcome::Banned`, `CreateCharOutcome::DuplicateName`, …) map
  to the legacy `LR_*` / `CR_*` / `DR_*` wire codes.
* **`SpdlogAuditLogger`** — emits structured log lines through spdlog
  (`event=login outcome=Success uid=1001 ip=… key=…`). The default
  production sink — feeds whatever stack (Seq / Loki / ELK) already
  ingests the server's logs.
* **`UdpAuditLogger`** — decorator that additionally emits the
  wire-faithful legacy `_UDPPACKET` to `TLogSvr`. Wired when
  `[audit.udp]` is set in the consumer's TOML so existing tooling that
  reads `TLOG_AUDIT` keeps working.

### `fourstory::smtp`

* **`ISmtpClient`** — sink interface for outbound mail (used by 2FA).
* **`SpdlogSmtpClient`** — log-only default. Production deploys plug in
  a real SMTP transport behind the same interface.

### `fourstory::cluster`

* **`PeerClient`** — outbound counterpart of the modern cluster
  control plane (TControlSvrAsio's `CT_PEER_*` handlers). Each peer
  game server constructs one on startup and `co_spawn`s `Run()`. The
  coroutine drives the full connect → register → heartbeat →
  reconnect state machine: exponential backoff on TCP / REGISTER /
  HEARTBEAT failures, graceful DEREGISTER on `Stop()`. Wire framing
  inlined (8-byte CPacket header + XOR-fold checksum) so the library
  stays standalone — peer servers link only `fourstory_common`, not
  TProtocol / TNetLib. `MakePeerClientOptions(cluster_cfg, type_id,
  name_prefix, listener_bind, listener_port, version)` is the
  one-liner every server's main uses to turn its `[cluster]` TOML
  block into a configured options bag.

### `fourstory::ops`

* **`HealthEndpoint`** — minimal HTTP `/healthz` JSON
  (`{"status":"ok","uptime_seconds":N,"version":"5.0"}`) for k8s
  liveness/readiness probes. ~50 LOC of Asio + raw strings;
  Boost.Beast would be overkill for one endpoint.
* **`LoginRateLimiter`** — token-bucket per peer IP. Defaults: burst=5,
  refill=1/10s, GC after 600s idle. Caller asks `Allow(peer_key)`
  before invoking auth. (Used by the login server; patch/log don't
  need it.)
* **`RegistryRefresher`** — periodic timer that fires registered
  callbacks every N seconds. The login server uses it to reload
  `TVETERANCHART` without a restart; mirrors legacy
  `CTLoginSvrModule::UpdateData`.

## Use from a consumer's CMakeLists

```cmake
target_link_libraries(my_server PUBLIC fourstory_common)
# Headers are reached via "fourstory/..." — the include path is set
# transitively as PUBLIC on fourstory_common, so no extra
# target_include_directories needed.
```

Transitive deps `fourstory_common` brings: `Boost::system`,
`spdlog::spdlog`, `SOCI::soci_core` (+ `SOCI::soci_odbc` if available).
Add `tnetlib_portable` yourself if you also need the wire-codec layer.

## Build

Lives under the root CMake project; built as a transitive dependency
of any server target. To build it standalone:

```sh
cmake --build build --target fourstory_common
```

## Design notes

* **Why a static lib, not a header-only template farm?** SOCI session
  ownership + audit-sink wiring needs runtime state. A static lib keeps
  build times reasonable and lets `main.cpp` of each server pick the
  exact sink implementations.
* **Why `fourstory/` prefix?** Cheap collision avoidance — consumer
  projects may have their own `db/` / `audit/` directories, and writing
  `#include "fourstory/db/session_pool.h"` makes the dependency
  explicit at the call site.
* **What stays in the server, not the lib?** Login-domain interfaces
  (`IAuthService`, `ICharService`, `IConnectionRegistry`,
  `IMapServerLocator`, `ISessionTerminator`) and the SOCI services
  implementing them remain in `Server/TLoginSvrAsio/services/`. They're
  not shared across servers — only the plumbing is. Same rule for
  the AdminShell — it lives in `Server/TControlSvrAsio/` because
  TControl is its only consumer (centralized operator entry point
  for the cluster). The opposite direction is OK: the `PeerClient`
  IS shared because four peer servers link it.
