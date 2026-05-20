# TControlSvrAsio — modernized control / orchestration server

Wire-compatible replacement for `Server/TControlSvr/` running on the
`FourStoryCommon` infrastructure (SOCI pool, spdlog audit, health
endpoint, admin shell). The legacy daemon ships ~7 285 LOC of
ATL/IOCP/PDH/Win32-SCM code; the rewrite distils the protocol surface
into ~65 CT_\* handlers around a single `boost::asio::io_context`.

The plan, handler-by-handler, lives in
[`_rewrite/docs/CONTROL_SERVER_PORT_PLAN.md`](../../_rewrite/docs/CONTROL_SERVER_PORT_PLAN.md).
This README only covers what F1 ships and how to bring it up.

## Status — F1 → F5 complete + round-2 audit fixes applied

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
| End-to-end legacy `TController.exe` smoke test | | | | | | ⏸ |

## Handler coverage

After round-2 fixes the dispatcher wires **63 / 65** legacy CT_* handlers.
The two intentional skips are documented in
`_rewrite/docs/CONTROL_SERVER_PORT_PLAN.md` §6 (CT_SERVICEUPLOAD* UNC
file-share path) and are not in the legacy dispatch table either
(`CT_INSTALLVERSION_*`, `CT_ACCOUNTINPUT_*`, `CT_SERVICECLOSE_*`,
`CT_DISCONNECT_*`, `CT_LOCALGUILDCHANGE_*`, `CT_LOCALINIT_*` — dead
code in legacy too).

## Known concerns

* **SOCI calls run on the io_context thread.** Every SOCI repo
  (`SociServiceInventory`, `SociOperatorAuthService`,
  `SociUserProtectedService`, `SociEventRepository`,
  `SociPatchMetadataService`, `SociAlerter`) is invoked synchronously
  from handler coroutines. On a slow DB this will stall the single
  io_context thread, blocking every operator and peer for the
  duration of the call. The architectural fix is to wrap each SOCI
  call in `co_await asio::post(thread_pool, ...)`; the modernization
  plan §4.5 notes this as the planned production path. Deferred from
  the F1–F5 scope because the control server's request rate is low
  (~10 operators, ~10 peers, ~1Hz monitoring) and the dev/test
  fakes don't exhibit the problem; production deploys should land
  this before going live.
* **PDH platform counters are not collected.** Per the
  modernization plan §3.3, `CT_PLATFORM_REQ` is wire-preserved but
  the peer-side data is expected to be zero-filled; operators
  observe machine health via `/metrics` instead. The control-server
  handler forwards whatever the peer sent — if the peer ships
  zeros, the GUI's platform tile shows zeros.

The 6-phase plan estimates 23 working days end-to-end.

## What the F1+F2 binary does

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
   (`telnet 127.0.0.1 18186`) for ops introspection.

The remaining ~45 CT_\* handlers (admin operations, castle, event
manager, patch metadata, file upload) log a warning and drop the
packet. F3..F5 fill them in.

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
├── config.{h,cpp}                 — TOML loader
├── control_session.{h,cpp}        — 8-byte CPacket framing
├── control_server.{h,cpp}         — accept loop + per-session dispatch
│                                    + 1Hz peer-keepalive loop
├── peer_dialer.{h,cpp}            — outbound connect with timeout
├── operator_session.h             — CTManager equivalent (login state)
├── peer_session.h                 — CTServer equivalent
├── senders.{h,cpp}                — CT_*_ACK / CT_*_REQ wire builders
├── wire_codec.h                   — POD + length-prefixed-string helpers
├── handlers/
│   ├── handlers.h                 — HandlerContext + Dispatch + RunPeerLoop
│   ├── handlers_auth.cpp          — OPLOGIN / STLOGIN / SERVICEAUTOSTART
│   └── handlers_service.cpp       — SERVICESTAT / SERVICECONTROL /
│                                    NEWCONNECT / RECONNECT /
│                                    SERVICEMONITOR + RunPeerLoop
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
│   ├── operator_registry.{h,cpp}  — by-id + by-seq tracking, dup-kick
│   ├── service_controller.h       — IServiceController interface
│   ├── disabled_service_controller.h — default (NotSupported)
│   └── windows_scm_service_controller.{h,cpp} — Win32 SCM impl
│                                    (Linux build returns NotSupported)
└── tests/
    ├── test_operator_login.cpp    — F1 wire round-trip
    └── test_peer_monitor.cpp      — F2 peer dial + SERVICEMONITOR fan-out
```

## Design decisions captured in F1

- **Single io_context, single thread.** Control server runs dozens
  of operators + ~10 peer daemons, not thousands of players. Strands
  can be added later if any one hot path needs them.
- **DisabledServiceController by default.** Cross-machine daemon
  control needs ops infra (SSH keys, SCM permissions, sudoers). F1
  ships the interface and the no-op implementation; F2 wires the
  Windows SCM impl per the user request.
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
