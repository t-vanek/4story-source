# TMapSvrAsio — modernized 4Story map / gameplay server

Layered C++20 + Boost.Asio scaffolding for the eventual port of
`Server/TMapSvr/` (the legacy 112 843-LOC gameplay engine). The
transport, dispatcher, schema validators, and the production SOCI
services are in place; **the actual game-rules layer — damage
formulas, AI ticks, the quest VM, drop tables — is intentionally
not yet implemented.**

> Architecture deep-dive: [`ARCHITECTURE.md`](ARCHITECTURE.md)
> Porting recipe (one legacy handler at a time): [`CONSOLIDATION.md`](CONSOLIDATION.md)

## Status — scaffold complete, gameplay TODO

```
Transport (Asio + RC4 + framing)    ████████████████████  100%
Dispatch + rate-limit + metrics     ████████████████████  100%
Schema validators (8 tables)        ████████████████████  100%
SOCI services (player/inv/npc/…)    ████████████████████  100%
CS_* wire handlers ported           █░░░░░░░░░░░░░░░░░░░    5%   (14 / ~297)
DM_/MW_/SS_ handlers ported         ░░░░░░░░░░░░░░░░░░░░    <1%  (1 / ~300)
SendCS_* ack senders ported         ░░░░░░░░░░░░░░░░░░░░    <1%  (3 / ~358)
Chart loaders                       ████████░░░░░░░░░░░░   40%   (~10 / ~24 charts)
Game logic (damage / AI / quest)    ░░░░░░░░░░░░░░░░░░░░    0%
```

| Area | State | Notes |
|---|---|---|
| Boost.Asio accept loop + per-session coroutine | ✅ | `map_server.{h,cpp}`; max-connections gate, RC4 optional |
| Pre-auth idle watchdog | ✅ | T5 — drops sockets that don't `CS_CONNECT_REQ` |
| Per-session rate limiter | ✅ | T5 — `services/rate_limiter.{h,cpp}` |
| Handler dispatch (counter + latency + audit) | ✅ | `handlers/dispatch.cpp` |
| Boot-time schema validators | ✅ (8 tables) | `db/schema_validator.cpp` — TCURRENTUSER / TCHARTABLE / TINVENTABLE / TNPCCHART / TSKILLTABLE / TQUESTTABLE / TQUESTTERMTABLE / TMONSTERCHART / TMONSPAWNCHART / TCOMPANIONTABLE |
| SOCI service layer | ✅ | `soci_player_service` / `soci_inventory_service` / `soci_npc_service` / `soci_skill_service` / `soci_quest_service` / `soci_monster_chart` / `soci_spawn_chart` / `soci_companion_service` / `soci_session_validator` |
| In-memory state stores | ✅ | session_registry / channel_presence / monster_registry |
| World peer wire (`MW_/DM_`) | 🟡 | Connection lifecycle wired: inbound `DM_LOADCHAR_REQ`, `MW_ENTERSVR/ENTERCHAR/ADDCONNECT/CHECKMAIN/CONRESULT/CLOSECHAR/ROUTELIST_REQ`; outbound `MW_ADDCHAR/ENTERSVR/ENTERCHAR/CHECKMAIN/ROUTE_ACK` + client relays. Gameplay MW_/DM_/SS_ traffic still TODO |
| Log peer (UDP `_UDPPACKET`) | ✅ | `services/log_peer.{h,cpp}` |
| Audit log + spdlog sink | ✅ | `audit/audit_log.{h,cpp}` + typed `audit/event.h` |
| Metrics endpoint (Prometheus) | ✅ | `ops/metrics_endpoint.{h,cpp}` |
| Admin TCP shell | ✅ | `ops/admin_shell.{h,cpp}` — status / kick / broadcast |
| Health endpoint | ✅ | reuses `fourstory::ops::HealthEndpoint` |
| **Game rules (damage / AI / quest VM)** | ❌ | Design decision pending — see Roadmap |
| **Combat handlers** | ❌ | `CS_ATTACK_REQ` family not wired |
| **Drop tables / loot** | ❌ | `TDROPCHART` loader missing |

## Wired handlers (27 total)

```
CS_CONNECT_REQ            session.cpp     enter map, presence broadcast
CS_CONREADY_REQ           session.cpp     post-load → CS_CHARINFO_ACK (own char; AOI flood deferred)
CS_MOVE_REQ               movement.cpp    position + broadcast
CS_NPCTALK_REQ            npc.cpp         dialogue dispatch
CS_SKILLUSE_REQ           skill.cpp       skill cooldown + broadcast (no damage calc)
CS_QUESTEXEC_REQ          quest.cpp       quest progress update (no objective eval)
CS_QUESTDROP_REQ          quest.cpp       abandon quest
CS_CHAT_REQ               social.cpp      channel chat broadcast
CS_PARTYADD_REQ           social.cpp      party invite (forwarded to World)
CS_PARTYJOIN_REQ          social.cpp
CS_PARTYDEL_REQ           social.cpp
CS_REGISTERBOW_REQ        bow.cpp         BR/BoW queue registration
CS_CANCELBOWQUEUE_REQ     bow.cpp
CS_CASHBOWRESPAWN_REQ     bow.cpp

CT_ANNOUNCEMENT_ACK       control.cpp     broadcast operator message
CT_USERKICKOUT_ACK        control.cpp     kick by char-id
CT_SERVICEMONITOR_ACK     control.cpp     live counts back to TControlSvr
CT_SERVICEDATACLEAR_ACK   control.cpp     no-op (registry is canonical)
CT_CTRLSVR_REQ            control.cpp     heartbeat

DM_LOADCHAR_REQ  (inbound, World→Map)  handlers_world.cpp  load char snapshot → DM_LOADCHAR_ACK
MW_ENTERSVR_REQ  (inbound, World→Map)  handlers_world.cpp  resolve identity → MW_ENTERSVR_ACK
MW_ENTERCHAR_REQ (inbound, World→Map)  handlers_world.cpp  per-con entry ready → MW_ENTERCHAR_ACK
MW_ADDCONNECT_REQ(inbound, World→Map)  handlers_world.cpp  peer-server list → client CS_ADDCONNECT_ACK
MW_CHECKMAIN_REQ (inbound, World→Map)  handlers_world.cpp  main-cell check → MW_CHECKMAIN_ACK
MW_CONRESULT_REQ (inbound, World→Map)  handlers_world.cpp  settled verdict → client CS_CONNECT_ACK
MW_CLOSECHAR_REQ (inbound, World→Map)  handlers_world.cpp  close order → client CS_SHUTDOWN_ACK + teardown
MW_ROUTELIST_REQ (inbound, World→Map)  handlers_world.cpp  resolve server ids → MW_ROUTE_ACK
```

> **Connect-ack note:** `session.cpp::OnConnectReq` still emits an
> optimistic `CS_CONNECT_ACK` at `CS_CONNECT_REQ` time so connect works
> with no `[world]` peer configured. Once the full World connect loop is
> proven end-to-end, that optimistic ack moves into `OnMWConResultReq`
> (the authoritative path) to avoid a double ack.

The remaining ~280 `CS_*` and ~300 `DM_/MW_/SS_` handlers are catalogued
in `CONSOLIDATION.md`. The porting recipe (locate → verify id → pick
file → declare → implement → dispatch → verify) is in that doc.

## Why "scaffold only"

The legacy `TMapSvr/` carries ~113 kLOC where the bulk of the cluster's
*gameplay logic* lives — `Quest*.cpp` × 24 files (quest engine),
`TAICmd*.cpp` × 14 files (mob AI), `CSHandler.cpp` (20 kLOC switch),
`SSHandler.cpp` (15 kLOC switch). Porting that 1:1 reproduces the
maintenance burden the modernization was supposed to escape.

Two design questions need decisions before the gameplay layer lands:

1. **Quest VM vs data-driven engine.** Lua-via-sol2 means scripting
   without recompiles, but introduces a runtime. YAML + interpreter
   keeps the build hermetic, but constrains quest expressiveness.
2. **Dispatcher shape.** The legacy `SSHandler.cpp` `switch` recompiles
   every server when one packet ID changes. A register-based dispatch
   (handler table indexed by `MessageId`) or a schema-versioned codec
   (flatbuffers / protobuf) would decouple them.

Until these are resolved, the scaffold runs handlers that decode the
wire, validate the session, and call into services — but the
business-rule layer between them returns "OK, broadcast" instead of
running combat math. **Do not deploy this binary against players.**

## Configuration

```toml
[server]
bind = "0.0.0.0"
port = 7016                          # legacy DEF_MAPPORT
# max_connections = 4000
# pre_auth_timeout_secs = 60

[crypto]
disable_rc4 = false

[log]
level = "info"

[health]
port = 8916                          # /healthz
metrics_port = 9091                  # /metrics (Prometheus)

[admin]
bind = "127.0.0.1"
port = 18187                         # localhost-only telnet

[world_peer]                         # outbound dial to TWorldSvr
host = "127.0.0.1"
port = 7015

[log_peer.udp]                       # legacy _UDPPACKET to TLogSvr
host = "127.0.0.1"
port = 2000

[database]                           # TGAME — per-world chars/items
backend = "odbc"
connection_string = "DRIVER={ODBC Driver 17 for SQL Server};SERVER=localhost;DATABASE=TGAME_RAGEZONE;Trusted_Connection=yes;TrustServerCertificate=yes"
pool_size = 8
```

Annotated reference: `tmapsvr.example.toml`. Local dev override:
`tmapsvr.local.toml` (gitignored by convention).

## Build

```sh
cmake --build build --target tmapsvr_asio -j
build/bin/tmapsvr_asio --config Server/TMapSvrAsio/tmapsvr.local.toml
```

Links `fourstory_common` + `tnetlib_portable`.

## Tests

```sh
ctest --test-dir build -R tmapsvr_asio --output-on-failure
```

In-process tests cover the wire codec, dispatch path, and per-service
SOCI suites (skip when `TMAPSVR_TEST_MSSQL_CONN` is unset).

## Files

```
Server/TMapSvrAsio/
├── README.md                       (this file)
├── ARCHITECTURE.md                 — layered design
├── CONSOLIDATION.md                — legacy-port recipe
├── CMakeLists.txt
├── tmapsvr.example.toml
├── main.cpp                        — boot, signal handling
├── config.{h,cpp}                  — TOML loader
├── map_server.{h,cpp}              — accept loop + per-conn coroutine
├── handlers.h                      — HandlerContext + decls
├── handlers/                       — one CS_/CT_ family per file
│   ├── dispatch.cpp                — switch on MessageId
│   ├── session.cpp                 — connect / disconnect
│   ├── movement.cpp                — move / jump
│   ├── npc.cpp                     — NPC interaction
│   ├── skill.cpp                   — skill cast (no damage yet)
│   ├── quest.cpp                   — quest exec / drop
│   ├── social.cpp                  — chat / party
│   ├── bow.cpp                     — BR / BoW queue
│   └── control.cpp                 — CT_* operator handlers
├── handlers_world.{h,cpp}          — inbound DM_/MW_ from World peer
├── wire_codec.h                    — POD reader / writer
├── domain/                         — pure POD types
├── services/                       — interfaces + SOCI + Fake impls
├── db/                             — query catalog + 8 schema validators
├── audit/                          — typed events + emitter
├── ops/                            — metrics + admin shell + endpoint
├── legacy_src/                     — verbatim Server/TMapSvr/ (reference)
└── tests/                          — (TODO) unit + integration
```

## Roadmap

| Phase | Scope | Status |
|---|---|---|
| **T1** | Per-family handler split (one file per CS_/CT_ family) | ✅ |
| **T2** | SOCI service layer (8 services) + schema validators | ✅ |
| **T3** | World-peer strand serialization | ✅ |
| **T4** | Audit + metrics data plane | ✅ |
| **T5** | Pre-auth watchdog + rate limit | ✅ |
| **T6** | Metrics + admin shell endpoints | ✅ |
| **T7** | Quest VM design decision | ⏸ |
| **T8** | Combat / damage formula port | ⏸ |
| **T9** | Mob AI tick + spawn manager | ⏸ |
| **T10** | Drop table / loot generator | ⏸ |
| **T11** | Bulk handler port (CONSOLIDATION recipe × 280) | ⏸ |

See `_rewrite/docs/MODERNIZATION_PLAN.md` Phase 5 for the cluster-wide
position of this work, and `_rewrite/docs/PATCH_README.md` for the
patch catalog of what's already been changed vs the legacy Araz
sources.
