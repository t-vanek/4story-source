# TControlSvrAsio — modernized control / orchestration server

Wire-compatible replacement for `Server/TControlSvr/` running on the
`FourStoryCommon` infrastructure (SOCI pool, spdlog audit, health
endpoint, admin shell). The legacy daemon ships ~7 285 LOC of
ATL/IOCP/PDH/Win32-SCM code; the rewrite distils the protocol surface
into ~65 CT_\* handlers around a single `boost::asio::io_context`.

> Cluster context: [main README](../../README.md#overall-progress) ·
> patch catalog vs legacy Araz sources:
> [`_rewrite/docs/PATCH_README.md` §4](../../_rewrite/docs/PATCH_README.md#4-tcontrolsvr--tcontrolsvrasio)

The plan, handler-by-handler, lives in
[`_rewrite/docs/CONTROL_SERVER_PORT_PLAN.md`](../../_rewrite/docs/CONTROL_SERVER_PORT_PLAN.md).
This README only covers what F1 ships and how to bring it up.

## Status — F1 → F6 + cluster control plane shipped

F1-F5 ported every legacy CT_\* handler (round-2 audit closed all
wire-parity gaps). F6 added the universal `IServiceController`
backends (Win32 SCM + systemd via `systemctl`), persistent peer
registry (`TPEER_REGISTRY`), and a periodic SCM status
reconciliation loop. On top of the legacy protocol the server now
also runs a modern **cluster control plane** (registry + routing +
streaming events + orchestration) — see the section after the
F1–F6 table.

Round-2 audit (2026-05-20) caught real wire-parity gaps and missing
handlers that F1–F5 had overlooked. All findings are now closed
except for the architectural SOCI-on-io_context concern (see
"Known concerns" below); the wire matches legacy byte-for-byte and
every previously-missing handler is wired in dispatch.

### Round-2 fixes

| Severity | Issue | Fix | Test |
|---|---|---|---|
| 🔴 Wire breaker | CHATBANLIST / EVENTLIST / CASHITEMLIST / PREVERSIONTABLE count was DWORD; legacy uses WORD | `senders.cpp` writes uint16 | `test_wire_parity` |
| 🔴 Wire breaker | CHATBANLIST_ACK row order wrong | Reordered to legacy: id, target, created, minutes, reason, op | `test_wire_parity` |
| 🔴 Truncated | CT_EVENTUPDATE_REQ shipped only `kind+value` | Appends full EventInfo via `event_codec::Write` | `test_wire_parity` |
| 🟠 Missing | CT_ITEMFIND_REQ / CT_ITEMSTATE_REQ / CT_MONACTION_REQ / CT_SERVICEDATACLEAR_REQ / CT_PLATFORM_REQ | Wired in `handlers_extra.cpp` | `test_wire_parity` |
| 🟠 Missing | CT_SERVICECHANGE_REQ (peer → control) | Wired in `RunPeerLoop` | smoke |
| 🟠 Missing | 9 peer→operator ACK route-backs (ITEMFIND/STATE/MONSPAWNFIND/EVENTQUARTER*/TOURNAMENT/RPSGAME/CMGIFT*) | Wired in `RunPeerLoop` via `OnPeerAckRouteBack` and the two specialized strip-paths | smoke |
| 🟠 Missing | Post-dial event push (`SendEventToNewConnect`) | Restored in `OnNewConnectReq` for Login/Map/World peers | smoke |

### Production hardening (post-audit)

| Component | Status | Notes |
|---|---|---|
| `LoginRateLimiter` on CT_OPLOGIN_REQ / CT_STLOGIN_REQ | ✅ | Token-bucket from `fourstory::ops`. Tripped peers receive the same generic reject ack as a wrong password — attackers can't distinguish rate-limit from invalid creds. Tunable via `[login_rate]` TOML (`burst=0` disables). |
| `RegistryRefresher` for SOCI inventory | ✅ | Re-reads TMACHINE / TGROUP / TSVRTYPE / TSERVER / TIPADDR every `[inventory] refresh_seconds`; `PeerRegistry.Rebind` picks up new services + drops removed ones. 0 disables (legacy load-once behavior). |
| `CT_SERVICEUPLOAD*` graceful stub | ✅ | Plan §6: returns `bRet=2` instead of dropping silently, so GUI shows an error tile. |
| SOCI integration suite | ✅ | `test_soci_repositories` exercises all five SOCI repos; skips when no `TCONTROLSVR_TEST_{PG,MSSQL}_CONN` env var is set. |
| `fourstory::db::CoOffload` thread-pool offload helper | ✅ | Header in `Lib/Own/FourStoryCommon/fourstory/db/co_offload.h`. Wraps a sync SOCI call in `co_await asio::post(pool, …)` + resumes on the original executor; exceptions propagate via the canonical `void(exception_ptr, R)` completion signature. Wired into CT_OPLOGIN_REQ / CT_STLOGIN_REQ / CT_USERPROTECTED_REQ as the hot-path proof-of-concept; other call sites opt in by writing `co_await fourstory::db::CoOffload(*ctx.db_pool, [&] { … })`. Worker pool size via `[database] worker_threads` (0 = legacy in-line behavior). |

| Area | F1 | F2 | F3 | F4 | F5 | F6 |
|------|----|----|----|----|----|----|
| Accept loop + ControlSession framing | ✅ | | | | | |
| OperatorSession state machine | ✅ | | | | | |
| PeerSession + PeerRegistry + outbound dial | ✅ (type only) | ✅ | | | | |
| TOML config | ✅ | + DB | | | | |
| Health endpoint + admin shell | ✅ | | | | | |
| `IOperatorAuthService` interface + fake | ✅ | + SOCI | | | | |
| `IServiceInventory` interface + fake | ✅ | + SOCI | | | | |
| `IServiceController` interface | ✅ (disabled default) | + WindowsScm | | | | |
| `CT_OPLOGIN_REQ` / `CT_STLOGIN_REQ` | ✅ | | | | | |
| Post-login ack chain (GROUP/MACHINE/SVRTYPE/AUTOSTART) | ✅ | | | | | |
| `CT_SERVICESTAT_REQ` / `CT_SERVICECONTROL_REQ` | | ✅ | | | | |
| `CT_NEWCONNECT_REQ` / `CT_RECONNECT_REQ` / `CT_CTRLSVR_REQ` | | ✅ | | | | |
| Peer-driven `CT_SERVICEMONITOR_REQ` + `CT_SERVICEDATA_ACK` fan-out | | ✅ | | | | |
| 1Hz peer keep-alive watchdog (`PeerKeepaliveLoop`) | | ✅ | | | | |
| Schema validator (TGROUP / TMACHINE / TIPADDR / TSVRTYPE / TSERVER) | | ✅ | | | | |
| Authority gate enum + `CT_AUTHORITY_ACK` reject path | | | ✅ | | | |
| Admin forwarders — KICK / MOVE / POSITION / CHARMSG / ANNOUNCEMENT | | | ✅ | | | |
| `CT_USERPROTECTED_REQ` via `IUserProtectedService` (SOCI: `TUserProtectedAdd`) | | | ✅ | | | |
| Chat-ban: N-wave aggregator + list + delete | | | ✅ | | | |
| `IAdminAuditLogger` interface + spdlog impl (shared "audit" channel) | | | ✅ | | | |
| `CT_MONSPAWNFIND_REQ` map broadcast | | | ✅ | | | |
| `IEventRepository` + `EventRegistry` + overlap validation | | | | ✅ | | |
| Event CRUD handlers (CHANGE / DEL / LIST / MSG / UPDATE) | | | | ✅ | | |
| Cash-shop handlers (CASHITEMSALE / CASHSHOPSTOP / CASHITEMLIST) | | | | ✅ | | |
| 1Hz `EventSchedulerLoop` — daily / term + alarms + auto-delete | | | | ✅ | | |
| Raw passthrough forwarders (EVENTQUARTER / TOURNAMENT / HELP / RPS / CMGIFT) | | | | ✅ | | |
| `IPatchMetadataService` + SOCI impl (TUpdateVersion / TBetaToVer / …) | | | | | ✅ | |
| `CT_UPDATEPATCH_REQ` / `CT_PREVERSIONTABLE_REQ` / `CT_PREVERSIONUPDATE_REQ` | | | | | ✅ | |
| Castle handlers (INFO / GUILDCHG / ENABLE) + peer-ack routing | | | | | ✅ | |
| `IAlerter` (SOCI: `OPTool_SMSEmergency` / spdlog default) fired on offline peer | | | | | ✅ | |
| Service-upload no-op stubs (`CT_SERVICEUPLOAD*`) | | | | | ⏸ | (intentional: legacy UNC-share anti-pattern) |
| Real `IServiceController` backends (Win32 SCM + systemd) | | | | | | ✅ |
| `cluster start/stop/restart/wait-healthy` admin shell commands | | | | | | ✅ |
| End-to-end legacy `TController.exe` smoke test | | | | | | ⏸ |

## Cluster control plane (post-F6)

A small foundation layered on top of the legacy CT_\* protocol that
gives every peer server (TLogin / TLog / TPatch / TMap) a unified
surface for self-registration, routing, streaming events, and
lifecycle commands. Each block is its own commit on the branch +
its own test executable:

| Block | What it adds | Wire / surface |
|---|---|---|
| **Registry** (F1, server side) | Peers self-register on startup + keep a lease alive with a 30s heartbeat. Lease-expiry sweep reaps anything that misses ~3 windows. | `CT_PEER_REGISTER_REQ` / `_ACK` / `_HEARTBEAT_REQ` / `_ACK` / `_DEREGISTER_REQ` at message-id range `0x9F00–0x9F04` (outside the legacy `0x93xx` range so it's obvious these are not part of the 4Story client wire) |
| **Communication** (PeerClient lib) | Outbound counterpart of the registry handlers. Lives in `Lib/Own/FourStoryCommon/fourstory/cluster/peer_client.{h,cpp}`. Each peer server links it + `co_spawn`s `Run()` from its main. Reconnect loop with exponential backoff; graceful `DEREGISTER` on `Stop()`. | Library + `[cluster]` TOML block on every peer server |
| **Routing** (`MessageRouter`) | Single typed surface for "send this frame to that peer / that type / those groups", replacing inline `for (auto& peer : peers.FindByType(...))` open-codings. `SendToService` / `SendToType` (round-robin) / `BroadcastToGroupType` / `BroadcastToType`. | C++ API only — internal abstraction |
| **Gateway** (admin-shell `route` + `peer <sid>`) | Operator CLI that drives `MessageRouter` from outside: `route service <sid> <wId> [hex-body]`, `route type <group> <type> <wId> [hex-body]`, `route broadcast …`. `peer <sid>` unifies static inventory + runtime status + registry entry in one view. Every routing command emits an `IAdminAuditLogger` record. | Admin shell only — no new wire surface |
| **Stream** (`subscribe registry`) | Long-lived TCP subscription: operator opens the admin-shell connection, sends `subscribe registry`, and gets a key=value line per registry transition until the socket closes. Format `registry.<kind> sid=0x… lease=… name=… …`. Lives over an `in-process RegistryEventBus`. | Admin shell, line-based push |
| **Orchestration** (`cluster …`) | Cluster-wide lifecycle commands: `cluster start <type>` + `cluster stop <type>` broadcast SCM Start/Stop across every matching peer; `cluster restart <sid> [timeout]` Stop → wait-for-deregister → Start; `cluster wait-healthy [timeout]` blocks until every static service has a live registration. | Admin shell, drives `IServiceController` + `PeerRegistry` |

### Universal service controller (`IServiceController`)

| Platform | Backend | Real Start/Stop |
|---|---|---|
| Windows (`_WIN32`) | `WindowsScmServiceController` — `OpenSCManager`/`StartService`/`ControlService`/`QueryServiceStatus` | ✅ |
| Linux (`__linux__`) | `SystemdServiceController` — `systemctl start/stop/is-active` shell-out via popen, captured stdout, CoOffload-wrapped so the blocking call doesn't reach the io_context | ✅ |
| macOS / BSDs / other | `DisabledServiceController` fallback | ❌ no-op |

Factory at `services/service_controller_factory.h`. `[cluster.scm]
backend = "auto"` picks the platform default; explicit `"windows"`
or `"systemd"` on the wrong platform falls back to `disabled` with
a warn line. Unknown backend throws at boot so operator typos don't
surface later as silent no-ops. Per-service name overrides live in
`[cluster.scm.overrides] 0x010101 = "4Story_Login_World1"`.

### Persistent registry (`TPEER_REGISTRY`)

Opt-in via `[registry.persistence] enabled = true`. When enabled,
every `Register`/`Heartbeat`/`Deregister`/`Expire` transition
writes through to the configured TGLOBAL table (`TPEER_REGISTRY` by
default), and TControl boot reloads the snapshot before accepting
peer connections. After a TControl restart the cluster picture is
immediately accurate instead of going through a ~90 s "all peers
missing" window. DDL ships at
[`schema/tcontrol-peer-registry.sql`](schema/tcontrol-peer-registry.sql);
apply once per TGLOBAL database before enabling. This optional snapshot
adapter replaces `SociPeerRepository`; the two adapters expect different
`TPEER_REGISTRY` columns and must never write the same table together.
The default keeps the existing repository, including its status/metrics logs.
Snapshot writes use the worker pool when configured, and run inline otherwise.
Its tests use a fake persistence adapter; a native PostgreSQL Control repository
and production database acceptance remain pending.

### SCM status reconciliation loop

`[cluster.scm] status_reconcile_interval_secs = 30` drives a
coroutine that walks the static inventory every interval and calls
`IServiceController::QueryStatus` on each service. When the live
read differs from the cached `RuntimeStatus.status`, the cache
updates AND a `ScmStatusChanged` event publishes onto the event
bus. The `subscribe registry` stream picks it up as
`registry.scm-status sid=0x… prev=stopped status=running …`, so
operators tailing the stream see status transitions live (without
re-polling `peers`). `interval = 0` disables the loop.

### Security note — peer authentication is **not yet implemented**

Today's `CT_PEER_REGISTER_REQ` handler accepts any caller that can
speak the wire framing. There is no IP allowlist, no PSK, no HMAC,
and no mTLS. An attacker with network access to TControl's CT_\*
port can register as any `service_id` in the inventory, hijack
admin-forwarder broadcasts, and spoof status. This is a regression
from legacy's `control_server_ip` IP-pinning on peer CT_\* traffic.
The deployment assumption today is "operator LAN, no hostile
clients on that segment." Closing this is the next concrete task
on the control-server backlog; the planned design is IP allowlist
(from TIPADDR) + per-service PSK + HMAC-SHA256 trailer on every
peer-side CT_PEER_\* frame.

## Handler coverage

After round-2 fixes the dispatcher wires **63 / 65** legacy CT_* handlers.
The two intentional skips are documented in
`_rewrite/docs/CONTROL_SERVER_PORT_PLAN.md` §6 (CT_SERVICEUPLOAD* UNC
file-share path) and are not in the legacy dispatch table either
(`CT_INSTALLVERSION_*`, `CT_ACCOUNTINPUT_*`, `CT_SERVICECLOSE_*`,
`CT_DISCONNECT_*`, `CT_LOCALGUILDCHANGE_*`, `CT_LOCALINIT_*` — dead
code in legacy too).

## Known concerns

* **SOCI thread-pool offload is opt-in per call site.** The
  `fourstory::db::CoOffload` helper bridges sync SOCI calls onto a
  `boost::asio::thread_pool` worker so the io_context stays
  responsive. CT_OPLOGIN_REQ / CT_STLOGIN_REQ / CT_USERPROTECTED_REQ
  are wired through it (hot operator-facing DB paths). The remaining
  SOCI call sites — `SociServiceInventory::Reload` (boot + 30s
  refresher), `SociEventRepository::*`, `SociPatchMetadataService::*`,
  `SociAlerter::Notify` — still execute in-line on the io_context.
  For the control server's low DB rate (~10 operators, ~10 peers,
  ~1Hz monitoring) that's acceptable, but production deploys with
  high-latency DB links should opt them in by wrapping the call
  sites with `co_await fourstory::db::CoOffload(*ctx.db_pool, …)`.
  The helper is in `Lib/Own/FourStoryCommon/fourstory/db/co_offload.h`
  and is reusable across every Asio server.
* **PDH platform counters are not collected.** Per the
  modernization plan §3.3, `CT_PLATFORM_REQ` is wire-preserved but
  the peer-side data is expected to be zero-filled; operators
  observe machine health via `/metrics` instead. The control-server
  handler forwards whatever the peer sent — if the peer ships
  zeros, the GUI's platform tile shows zeros.

The 6-phase plan estimates 23 working days end-to-end.

## What the F1–F6 binary does

1. Loads TOML config (default `tcontrolsvr.toml` next to the binary).
2. **Auth + inventory**: when `[database]` is configured, opens a
   SOCI pool against `TGLOBAL_RAGEZONE`, runs the schema validator
   (TGROUP / TMACHINE / TIPADDR / TSVRTYPE / TSERVER required;
   TEVENTCHART / TCASHSHOPITEMCHART / TPREVERSION reported as warnings
   when missing), and uses `SociOperatorAuthService` +
   `SociServiceInventory`. Without a database it falls back to the
   in-memory fakes seeded from `[[fake.*]]` TOML tables.
3. Binds the configured TCP port and runs the accept loop. Each
   accepted socket spawns one `ControlSession` coroutine that
   demuxes 8-byte-header CPacket frames, verifies the running-XOR
   checksum, and hands the body off to the handler dispatch.
4. Handles the F1+F2 CT_\* surface:
   - **F1 operator auth** — `CT_OPLOGIN_REQ` (with the 127.0.0.1
     authority-1 gate + duplicate-kick), `CT_STLOGIN_REQ` (read-only
     stat tool), `CT_SERVICEAUTOSTART_REQ` (cluster-wide broadcast).
     After a successful OPLogin the server emits the
     `GROUP/MACHINE/SVRTYPE/AUTOSTART_ACK` chain.
   - **F2 service lifecycle** — `CT_SERVICESTAT_REQ` (snapshot from
     `PeerRegistry`), `CT_SERVICECONTROL_REQ` (Start/Stop via
     `IServiceController`; default-disabled controller returns
     NotSupported, the WorldSvr cascade clears manager_control on
     siblings in the same group).
   - **F2 peer dial** — `CT_NEWCONNECT_REQ` and `CT_RECONNECT_REQ`
     route through `PeerDialer` (async connect with timeout) and
     register the resulting `PeerSession` in `PeerRegistry`. The
     `CT_CTRLSVR_REQ` handshake fires on dial success; the peer's
     read loop spawns on the same io_context.
   - **F2 peer monitoring** — Inbound `CT_SERVICEMONITOR_REQ` echoes
     the tick back via `CT_SERVICEMONITOR_ACK` and broadcasts the
     full counters via `CT_SERVICEDATA_ACK` to every logged-in
     operator. The 1Hz `PeerKeepaliveLoop` walks the registry,
     marks offline peers (>60s since last recv), closes their
     sockets, and emits zero-filled SERVICEDATA so the GUI tile
     transitions to "stopped".
5. Exposes `/healthz` on a separate port and a localhost admin shell
   (`telnet 127.0.0.1 18186`) for ops introspection. The shell is the
   single operator entry point for the cluster — it covers `peers`,
   `registry`, `peer <sid>`, `kick`, `announce`, `route service|type|
   broadcast`, `subscribe registry`, `service status|start|stop`,
   `cluster start|stop|restart|wait-healthy`, and `log-level`. Run
   `help` after connect for the full reference.
6. **Cluster control plane** (post-F6, see the section above):
   - Accepts modern `CT_PEER_REGISTER_REQ` / `_HEARTBEAT_REQ` /
     `_DEREGISTER_REQ` from peer servers + drives the lease-expiry
     sweep every 15 s
   - `ScmStatusReconciliationLoop` polls `IServiceController::
     QueryStatus` every 30 s + publishes `ScmStatusChanged` events
     to the bus
   - When `[registry.persistence] enabled = true` and `[database]`
     is configured, writes through every registry mutation to
     `TPEER_REGISTRY` and reloads the snapshot at boot

### Wire compatibility

The session framing matches the legacy `CPacket` layout — 8-byte
header `WORD wSize | WORD wID | DWORD dwChkSum` followed by body
bytes, no RC4. The legacy GUI client `TController.exe` connects
without any protocol changes.

String fields use the legacy `int32 length` + raw bytes (CP1252)
encoding. POD fields are little-endian, packed (no padding).

## Build

The TControlSvrAsio target is wired into the root CMake. From the
repo root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target tcontrolsvr_asio -j
```

Tests:

```sh
ctest --test-dir build -R tcontrolsvr_asio --output-on-failure
```

The F1 test (`tcontrolsvr_asio_operator_login`) stands the server up
on an ephemeral port, connects a loopback client, and verifies the
full post-login ack chain plus the wrong-password reject path.

## Run

```sh
cp Server/TControlSvrAsio/tcontrolsvr.example.toml ./tcontrolsvr.toml
# edit operators / inventory
./build/bin/tcontrolsvr_asio --config ./tcontrolsvr.toml
```

Then point a legacy `TController.exe` at the configured port and
authenticate against one of the `[[fake.operators]]` rows. After F2
lands, the `[database]` section will load from `TMACHINE` / `TGROUP`
/ `TSVRTYPE` / `TSERVER` / `TIPADDR` instead of `[fake.*]`.

## Files

```
Server/TControlSvrAsio/
├── CMakeLists.txt
├── README.md
├── tcontrolsvr.example.toml
├── main.cpp                       — CLI, signal handling, service wire-up
├── config.{h,cpp}                 — TOML loader (server, db, cluster.scm,
│                                    registry.persistence, …)
├── control_session.{h,cpp}        — 8-byte CPacket framing
├── control_server.{h,cpp}         — accept loop + per-session dispatch
│                                    + PeerKeepaliveLoop (1 Hz)
│                                    + RegistryLeaseExpiryLoop (15 s)
│                                    + ScmStatusReconciliationLoop (30 s)
├── peer_dialer.{h,cpp}            — outbound connect with timeout
├── admin_shell.{h,cpp}            — single operator entry point: peers /
│                                    registry / peer / kick / announce /
│                                    route / subscribe / service /
│                                    cluster / log-level
├── message_router.{h,cpp}         — typed routing primitives on top of
│                                    PeerRegistry (single / round-robin /
│                                    group-broadcast / type-broadcast)
├── operator_session.h             — CTManager equivalent (login state)
├── peer_session.h                 — CTServer equivalent
├── senders.{h,cpp}                — CT_*_ACK / CT_*_REQ wire builders
│                                    (legacy + modern CT_PEER_*)
├── wire_codec.h                   — POD + length-prefixed-string helpers
├── schema/
│   └── tcontrol-peer-registry.sql — TPEER_REGISTRY DDL (modern, opt-in)
├── handlers/
│   ├── handlers.h                 — HandlerContext + Dispatch + RunPeerLoop
│   ├── handlers_auth.cpp          — OPLOGIN / STLOGIN / SERVICEAUTOSTART
│   ├── handlers_service.cpp       — SERVICESTAT / SERVICECONTROL /
│   │                                NEWCONNECT / RECONNECT /
│   │                                SERVICEMONITOR + RunPeerLoop
│   ├── handlers_admin.cpp         — F3 admin forwarders
│   ├── handlers_event.cpp         — F4 event manager
│   ├── handlers_patch.cpp         — F5 patch metadata + castle
│   ├── handlers_extra.cpp         — round-2 ITEMFIND / MONACTION /
│   │                                SERVICEDATACLEAR / PLATFORM /
│   │                                SERVICEUPLOAD*
│   └── handlers_registry.cpp      — modern CT_PEER_REGISTER /
│                                    HEARTBEAT / DEREGISTER
├── db/
│   └── schema_validator.{h,cpp}   — boot-time fail-fast on TGLOBAL_RAGEZONE
├── services/
│   ├── operator_auth_service.h    — IOperatorAuthService
│   ├── fake_operator_auth_service.{h,cpp}
│   ├── soci_operator_auth_service.{h,cpp} — TOPLogin SP impl
│   ├── service_inventory.h        — IServiceInventory + POD shapes
│   ├── fake_service_inventory.h
│   ├── soci_service_inventory.{h,cpp} — TMACHINE/TGROUP/TSVRTYPE/TSERVER/TIPADDR
│   ├── peer_registry.{h,cpp}      — service_id → PeerSession + RuntimeStatus
│   │                                + dynamic RegistryEntry + Hydrate
│   ├── operator_registry.{h,cpp}  — by-id + by-seq tracking, dup-kick
│   ├── service_controller.{h,cpp} — IServiceController interface + enum
│   │                                helpers (ServiceStatusName)
│   ├── disabled_service_controller.h — default (NotSupported)
│   ├── windows_scm_service_controller.{h,cpp} — Win32 SCM impl
│   ├── systemd_service_controller.{h,cpp} — systemctl shell-out impl
│   ├── service_controller_factory.{h,cpp} — auto / windows / systemd /
│   │                                disabled selection
│   ├── scm_name_resolver.{h,cpp}  — shared template + overrides
│   ├── registry_event_bus.{h,cpp} — in-process pub/sub for registry
│   │                                transitions (subscribe registry)
│   ├── registry_persistence.h     — IRegistryPersistence interface +
│   │                                Noop default
│   ├── soci_registry_persistence.{h,cpp} — TPEER_REGISTRY upsert/touch/
│   │                                remove/load via SOCI
│   ├── admin_audit_logger.h       — IAdminAuditLogger interface
│   ├── spdlog_admin_audit_logger.{h,cpp}
│   ├── soci_event_repository.{h,cpp}
│   ├── soci_patch_metadata_service.{h,cpp}
│   ├── soci_user_protected_service.{h,cpp}
│   └── soci_alerter.{h,cpp}       — OPTool_SMSEmergency
└── tests/
    ├── test_operator_login.cpp           — F1 wire round-trip
    ├── test_peer_monitor.cpp             — F2 peer dial + SERVICEMONITOR
    ├── test_admin_forwarders.cpp         — F3 KICK/BAN/CHATBAN
    ├── test_event_scheduler.cpp          — F4 StepScheduler state machine
    ├── test_patch_metadata.cpp           — F5 patch SP wiring
    ├── test_wire_parity.cpp              — round-2 count-width + order fixes
    ├── test_soci_repositories.cpp        — env-gated SOCI integration
    ├── test_co_offload.cpp               — CoOffload helper
    ├── test_registry_refresher.cpp       — RegistryRefresher coroutine hook
    ├── test_peer_registry.cpp            — F1 registry handlers + lease
    ├── test_peer_client.cpp              — PeerClient register/heartbeat/
    │                                       deregister + reconnect
    ├── test_message_router.cpp           — single / round-robin / broadcast
    ├── test_admin_shell.cpp              — admin shell command parsing
    ├── test_admin_shell_route.cpp        — gateway route / peer commands
    ├── test_admin_shell_stream.cpp       — subscribe registry streaming
    ├── test_admin_shell_cluster.cpp      — cluster start/stop/restart/
    │                                       wait-healthy
    ├── test_service_controller.cpp       — factory + systemd runner stub
    ├── test_registry_persistence.cpp     — FakePersistence wiring +
    │                                       Hydrate epoch advance
    └── test_scm_status_reconcile.cpp     — ReconcileScmStatusOnce
                                            transitions + events
```

## Design decisions captured in F1

- **Single io_context, single thread.** Control server runs dozens
  of operators + ~10 peer daemons, not thousands of players. Strands
  can be added later if any one hot path needs them.
- **Platform service controller.** `auto` selects Windows SCM or Linux
  systemd; other platforms fall back to the disabled controller. Container
  smoke configuration explicitly disables service control and status polling,
  because Compose/Podman owns container lifecycle. Real service-manager
  permissions and integration acceptance remain deployment responsibilities.
- **Free-function senders.** Mirrors the TPatchSvrAsio convention.
  Each `CT_*_ACK` is a single `co_await` away from any handler.
- **Operator role as enum.** `OperatorRole::All / Control / User /
  Service / GMLevel1..3` mirrors `MANAGER_CLASS` in
  `TControlType.h`. Authority gating lands in F3 along with the rest
  of the admin handlers.
- **Audit destination: shared `TLOG_AUDIT`.** Decision captured for
  F3 — GM action records will use the same table as the login
  server, with an extended `audit_kind` enum. No audit emission in
  F1 (no admin handlers yet).
- **REST gateway ready.** Service interfaces (`IOperatorAuthService`,
  `IServiceInventory`, `IServiceController`, the planned
  `IEventRepository`, etc.) are wire-protocol-independent, so a REST
  endpoint can drive them in parallel with the CT_\* dispatch. F1
  does not include the REST gateway itself.
