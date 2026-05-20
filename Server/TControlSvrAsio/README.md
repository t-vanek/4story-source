# TControlSvrAsio — modernized control / orchestration server

Wire-compatible replacement for `Server/TControlSvr/` running on the
`FourStoryCommon` infrastructure (SOCI pool, spdlog audit, health
endpoint, admin shell). The legacy daemon ships ~7 285 LOC of
ATL/IOCP/PDH/Win32-SCM code; the rewrite distils the protocol surface
into ~65 CT_\* handlers around a single `boost::asio::io_context`.

The plan, handler-by-handler, lives in
[`_rewrite/docs/CONTROL_SERVER_PORT_PLAN.md`](../../_rewrite/docs/CONTROL_SERVER_PORT_PLAN.md).
This README only covers what F1 ships and how to bring it up.

## Status — F1 scaffold

| Area | F1 | F2 | F3 | F4 | F5 | F6 |
|------|----|----|----|----|----|----|
| Accept loop + ControlSession framing | ✅ | | | | | |
| OperatorSession state machine | ✅ | | | | | |
| PeerSession skeleton | ✅ (type only) | impl | | | | |
| TOML config | ✅ | + DB | | | | |
| Health endpoint + admin shell | ✅ | | | | | |
| `IOperatorAuthService` interface + fake | ✅ | + SOCI | | | | |
| `IServiceInventory` interface + fake | ✅ | + SOCI | | | | |
| `IServiceController` interface (Windows SCM impl) | ✅ (disabled default) | + WindowsScm | | | | |
| `CT_OPLOGIN_REQ` / `CT_STLOGIN_REQ` | ✅ | | | | | |
| Post-login ack chain (GROUP/MACHINE/SVRTYPE/AUTOSTART) | ✅ | | | | | |
| `CT_SERVICESTAT_REQ`, peer dial, monitoring | | ✅ | | | | |
| Admin operations + audit | | | ✅ | | | |
| Event scheduler + manager | | | | ✅ | | |
| Patch metadata + castle + ops polish | | | | | ✅ | |
| Schema validator + smoke test | | | | | | ✅ |

The 6-phase plan estimates 23 working days end-to-end.

## What the F1 binary does

1. Loads TOML config (default `tcontrolsvr.toml` next to the binary).
2. Seeds an in-memory `FakeOperatorAuthService` from
   `[[fake.operators]]` and a `FakeServiceInventory` from
   `[[fake.groups]]` / `[[fake.machines]]` / `[[fake.types]]`.
3. Binds the configured TCP port and runs the accept loop. Each
   accepted socket spawns one `ControlSession` coroutine that
   demuxes 8-byte-header CPacket frames, verifies the running-XOR
   checksum, and hands the body off to the handler dispatch.
4. Handles three CT_\* messages:
   - `CT_OPLOGIN_REQ` — authenticates, registers in
     `OperatorRegistry` (duplicate-kick mirrors the legacy gate),
     replies with `CT_OPLOGIN_ACK` + the GROUP/MACHINE/SVRTYPE list
     acks + `CT_SERVICEAUTOSTART_ACK`. Authority 1 (MANAGER_ALL) is
     restricted to `127.0.0.1` connections (legacy console gate).
   - `CT_STLOGIN_REQ` — read-only stat tool variant; no duplicate
     kick, no follow-up acks.
   - `CT_SERVICEAUTOSTART_REQ` — flips the cluster-wide auto-restart
     flag and broadcasts the new value to every logged-in operator.
5. Exposes `/healthz` on a separate port and a localhost admin shell
   (`telnet 127.0.0.1 18186`) for ops introspection.

The rest of the 65 CT_\* handlers log a warning and drop the packet.
F2..F5 fill them in.

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
├── operator_session.h             — CTManager equivalent (login state)
├── peer_session.h                 — CTServer equivalent (F2)
├── senders.{h,cpp}                — CT_*_ACK wire builders
├── wire_codec.h                   — POD + length-prefixed-string helpers
├── handlers/
│   ├── handlers.h                 — HandlerContext + Dispatch
│   └── handlers_auth.cpp          — OPLOGIN / STLOGIN / SERVICEAUTOSTART
├── services/
│   ├── operator_auth_service.h    — IOperatorAuthService
│   ├── fake_operator_auth_service.{h,cpp}
│   ├── service_inventory.h        — IServiceInventory + POD shapes
│   ├── fake_service_inventory.h
│   ├── operator_registry.{h,cpp}  — by-id + by-seq tracking, dup-kick
│   ├── service_controller.h       — IServiceController interface
│   └── disabled_service_controller.h — F1 default (NotSupported)
└── tests/
    └── test_operator_login.cpp    — wire round-trip
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
