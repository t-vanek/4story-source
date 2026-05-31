# TMapSvrAsio — modernized 4Story map / gameplay server

Layered C++20 + Boost.Asio port of `Server/TMapSvr/` (the legacy
112 843-LOC gameplay engine). The transport, dispatcher, schema
validators, and the SOCI service layer are complete; on top of that a
**vertical slice of real gameplay** is now implemented and tested:
the connection lifecycle plus an end-to-end **combat / loot / AI grind
loop.** The large content subsystems (quests, NPC shops, skill effects)
are **mostly not ported** (a kill-count quest slice is the first quest vertical) — see the status table below for the honest line.

> The grind loop is real: **connect → enter map → see monsters → attack
> (real damage) → kill → EXP + gold + item drops → loot the corpse →
> respawn.** Monsters roam, chase, and hit back; you can die and revive.

> Architecture deep-dive: [`ARCHITECTURE.md`](ARCHITECTURE.md)
> Porting recipe (one legacy handler at a time): [`CONSOLIDATION.md`](CONSOLIDATION.md)

## Status — infra complete, gameplay vertical slice

```
Transport (Asio + RC4 + framing)    ████████████████████  100%
Dispatch + rate-limit + metrics     ████████████████████  100%
Schema validators (12 tables)       ████████████████████  100%
SOCI services (player/inv/npc/…)    ████████████████████  100%
Chart loaders (mon/spawn/attr/…)    ██████████████░░░░░░   ~70%
Connection lifecycle (login→enter)  ████████████████████  100%
Combat / loot / AI grind loop       ██████████████░░░░░░   ~70%  (real dmg, death, drops, respawn)
Quests / shops / skill effects      ░░░░░░░░░░░░░░░░░░░░    0%   (next waves)
CS_* wire handlers ported           ██░░░░░░░░░░░░░░░░░░   ~11%  (21 functional+stub / ~297)
```

| Area | State | Notes |
|---|---|---|
| Boost.Asio accept loop + per-session coroutine | ✅ | `map_server.{h,cpp}`; max-connections gate, RC4 optional |
| Pre-auth idle watchdog + per-session rate limiter | ✅ | `services/rate_limiter.{h,cpp}` |
| Handler dispatch (counter + latency + audit) | ✅ | `handlers/dispatch.cpp` |
| Boot-time schema validators (12 tables) | ✅ | `db/schema_validator.cpp` |
| SOCI service layer + chart loaders | ✅ | player / inv / npc / skill / quest / monster / spawn / attr / drop / map-mon |
| In-memory state stores | ✅ | session_registry / channel_presence / monster_registry / char_state / corpse_registry |
| **Connection lifecycle** (login→world→map→enter→charinfo) | ✅ | `handlers_world.cpp` + `session.cpp`; `CS_CHARINFO_ACK` own-char sheet |
| **Static monster spawn + enter-map AOI** | ✅ | `spawn_manager` joins spawn+map-mon+monster charts; `CS_ENTER`/`CS_ADDMON` |
| **Interactive combat** | ✅ | `CS_ACTION` (animation) + `CS_DEFEND` (real `CalcDamage` core) → `CS_HPMP` / death |
| **Monster death + EXP + drop + respawn** | ✅ | `CS_DELMON` + `CS_EXP`; money+items roll onto corpse; detached 15 s respawn |
| **Player death & revival** | ✅ | `CS_DIE_ACK` + `CS_REVIVAL_REQ` → HP refill + reposition |
| **Monster AI** (idle roam → aggro chase → melee attack) | ✅ | `services/monster_ai`; capped per tick; floors player HP at 1 |
| **Corpse loot window** | ✅ | `CS_MONITEMLIST/MONMONEYTAKE/MONITEMTAKE/TAKEALL` → bag + purse (persisted) |
| **Item drop tables** | ✅ | `TMONITEMCHART` roll (`services/loot.h`) + 38-byte item descriptor wire |
| Skill reuse-cooldown gate | 🟡 | `CS_SKILLUSE` drops re-use faster than `TSKILLCHART.dwReuseDelay`; no MP/effects |
| World peer wire (`MW_/DM_`) | 🟡 | Connection lifecycle wired (see Wired handlers); gameplay MW_/DM_/SS_ traffic still TODO |
| **Quest engine** (kill-count slice) | 🟡 | accept → kill-progress → turn-in → reward (gold/EXP); engine per [`QUEST_ENGINE.md`](QUEST_ENGINE.md). Item/skill rewards, accept-conditions, DB-persist of progress pending |
| **NPC dialogue / shops / storage** | ❌ | `OnNpcTalkReq` returns trigger 0; no buy/sell/cabinet |
| **Skill effects + MP cost** | ❌ | heal/buff/debuff (SDT_/MTYPE_), `GetRequiredMP`, `CS_SKILLUSE_ACK` damage |
| Real player AP/WAP/DP from stats + gear | ❌ | combat uses client-sent powers vs monster DP; player defense = 0 placeholder |
| Chat / party / BR-BoW / operator control | ❌ | `OnChatReq` / `OnParty*` / `OnRegisterBow*` / `OnCt*` decode+log only |

## Wired handlers

Client (`CS_*`) — **functional** (real game logic):

```
CS_CONNECT_REQ        session.cpp   validate TCURRENTUSER, bind registry, optimistic CS_CONNECT_ACK
CS_CONREADY_REQ       session.cpp   → CS_CHARINFO_ACK + CS_ENTER (players) + CS_ADDMON (monsters)
CS_MOVE_REQ           movement.cpp  position update + channel broadcast
CS_ACTION_REQ         combat.cpp    attack animation broadcast (CS_ACTION_ACK)
CS_DEFEND_REQ         combat.cpp    real damage (CalcDamage) → CS_HPMP / death → CS_DELMON + CS_EXP + loot + respawn
CS_REVIVAL_REQ        combat.cpp    clear death, refill HP, reposition → CS_REVIVAL_ACK
CS_MONITEMLIST_REQ    loot.cpp      open corpse loot window (money + items)
CS_MONMONEYTAKE_REQ   loot.cpp      take corpse money → purse → CS_MONEY_ACK
CS_MONITEMTAKE_REQ    loot.cpp      take one corpse item → bag (persisted) → CS_GETITEM_ACK
CS_MONITEMTAKEALL_REQ loot.cpp      take all that fits (partial on full bag)
CS_SKILLUSE_REQ       skill.cpp     reuse-cooldown gate (TSKILLCHART); no damage/MP/effect yet
CS_QUESTEXEC_REQ      quest.cpp     accept quest / turn-in (QT_COMPLETE) → reward gold+EXP
CS_QUESTDROP_REQ      quest.cpp     abandon active quest → CS_QUESTCOMPLETE_ACK(QR_DROP)
```

Client (`CS_*`) — **wired but stubbed** (decode + log, awaiting their subsystem):

```
CS_NPCTALK_REQ        npc.cpp       returns quest-trigger 0 (needs NPC dialogue + shop)
CS_CHAT_REQ           social.cpp    no routing (channel / whisper / world relay)
CS_PARTYADD/JOIN/DEL  social.cpp    party invite/join/leave — forwarded to World (TODO)
CS_REGISTERBOW_REQ    bow.cpp       BR/BoW queue — logged only
CS_CANCELBOWQUEUE_REQ bow.cpp
CS_CASHBOWRESPAWN_REQ bow.cpp
```

Control (`CT_*`, from TControlSvr) — stubs: `CT_ANNOUNCEMENT` / `CT_USERKICKOUT`
/ `CT_SERVICEMONITOR` / `CT_SERVICEDATACLEAR` / `CT_CTRLSVR` (`control.cpp`).

World peer (`DM_/MW_`, inbound from TWorldSvr) — **functional** connection lifecycle
(`handlers_world.cpp`):

```
DM_LOADCHAR_REQ    load char snapshot → DM_LOADCHAR_ACK (char + inv/skill/quest/companion)
MW_ENTERSVR_REQ    resolve identity → MW_ENTERSVR_ACK
MW_ENTERCHAR_REQ   per-con ready handshake → MW_ENTERCHAR_ACK
MW_ADDCONNECT_REQ  peer-server list → client CS_ADDCONNECT_ACK
MW_CHECKMAIN_REQ   main-cell check → MW_CHECKMAIN_ACK
MW_CONRESULT_REQ   settled verdict → client CS_CONNECT_ACK (close on reject)
MW_CLOSECHAR_REQ   close order → client CS_SHUTDOWN_ACK + teardown
MW_ROUTELIST_REQ   resolve server ids → MW_ROUTE_ACK
```

> **Connect-ack note:** `session.cpp::OnConnectReq` still emits an
> optimistic `CS_CONNECT_ACK` at connect time so connect works with no
> `[world]` peer configured. Once the full World connect loop is proven
> end-to-end, that ack moves into `OnMWConResultReq` (the authoritative
> path) to avoid a double ack.

The remaining ~280 `CS_*` and the gameplay `DM_/MW_/SS_` handlers are
catalogued in `CONSOLIDATION.md`. The porting recipe (locate → verify id
→ pick file → declare → implement → dispatch → verify) is in that doc.

## What's still ahead

The legacy `TMapSvr/` carries ~113 kLOC where the bulk of the cluster's
*gameplay content* lives — `Quest*.cpp` × ~20 files (quest engine),
`TAICmd*.cpp` × ~15 files (mob AI), `CSHandler.cpp` (21 kLOC, 297 CS_
handlers), `SSHandler.cpp` (20 kLOC). The grind-loop slice is in; the
content layers come next. The two architecture questions that gated the
gameplay phase are both resolved:

1. **Quest VM vs data-driven engine.** ✅ [`QUEST_ENGINE.md`](QUEST_ENGINE.md)
   — legacy quests are data-parameterised instances of ~20 fixed action
   types (QT_*) over canonical DB tables, so a register-based, DB-sourced
   engine, *not* Lua-via-sol2 and *not* re-authored YAML.
2. **Dispatcher shape.** ✅ [`DISPATCH.md`](DISPATCH.md) — keep the thin
   wire-dispatch `switch` (fixed id set, type-safe); use register tables
   only for internal logic (quest terms / AI). Schema-versioned codec
   rejected — the wire is byte-for-byte client-fixed.

**Do not deploy this binary against a live player base** — it plays the
grind loop but most of the game (quests, shops, trade, guilds, PvP,
battlegrounds) is not here yet.

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

In-process tests (18 suites) cover the wire codec, dispatch path, combat /
damage / loot / money / inventory / corpse / skill-cooldown logic, the
world-peer handshake, and the byte layout of every ack encoder. The SOCI
integration suites skip when `TMAPSVR_TEST_MSSQL_CONN` is unset.

## Files

```
Server/TMapSvrAsio/
├── README.md                       (this file)
├── ARCHITECTURE.md                 — layered design
├── CONSOLIDATION.md                — legacy-port recipe
├── DISPATCH.md                     — why dispatch stays a thin switch
├── QUEST_ENGINE.md                 — data-driven quest VM design
├── CMakeLists.txt
├── tmapsvr.example.toml
├── main.cpp                        — boot, signal handling
├── config.{h,cpp}                  — TOML loader
├── map_server.{h,cpp}              — accept loop + per-conn coroutine
├── handlers.h                      — HandlerContext + decls
├── handlers/                       — one CS_/CT_ family per file
│   ├── dispatch.cpp                — switch on MessageId
│   ├── session.cpp                 — connect / ready
│   ├── movement.cpp                — move
│   ├── combat.cpp                  — action / defend / revival
│   ├── loot.cpp                    — corpse loot window
│   ├── npc.cpp                     — NPC interaction (stub)
│   ├── skill.cpp                   — skill cast (cooldown gate)
│   ├── quest.cpp                   — quest exec / drop (stub)
│   ├── social.cpp                  — chat / party (stub)
│   ├── bow.cpp                     — BR / BoW queue (stub)
│   └── control.cpp                 — CT_* operator handlers (stub)
├── handlers_world.{h,cpp}          — inbound DM_/MW_ from World peer
├── wire_codec.h                    — POD reader / writer
├── domain/                         — pure POD types
├── services/                       — interfaces + SOCI + Fake impls
├── db/                             — query catalog + schema validators
├── audit/                          — typed events + emitter
├── ops/                            — metrics + admin shell + endpoint
├── legacy_src/                     — verbatim Server/TMapSvr/ (reference)
└── tests/                          — 18 unit + integration suites
```

## Roadmap

| Phase | Scope | Status |
|---|---|---|
| **T1** | Per-family handler split (one file per CS_/CT_ family) | ✅ |
| **T2** | SOCI service layer + schema validators | ✅ |
| **T3** | World-peer strand serialization | ✅ |
| **T4** | Audit + metrics data plane | ✅ |
| **T5** | Pre-auth watchdog + rate limit | ✅ |
| **T6** | Metrics + admin shell endpoints | ✅ |
| **T7** | Quest VM + dispatcher design decisions | ✅ ([`QUEST_ENGINE.md`](QUEST_ENGINE.md) / [`DISPATCH.md`](DISPATCH.md)) |
| **T8** | Connection lifecycle (login→world→map→enter) | ✅ |
| **T9** | Combat: spawn + `CS_ACTION`/`CS_DEFEND` real damage + death/EXP | ✅ |
| **T10** | Mob AI tick (roam → chase → melee) + timed respawn | ✅ |
| **T11** | Player death & revival | ✅ |
| **T12** | Monster drops → corpse loot window + inventory persist | ✅ |
| **T13** | Skill reuse-cooldown gate | 🟡 (MP cost + effects pending) |
| **T14** | Quest engine — kill-count slice (accept→kill→turn-in→reward) | 🟡 |
| **T15** | NPC shops (buy/sell) + storage | ⏸ |
| **T16** | Real player combat stats (AP/WAP/DP from base + gear) | ⏸ |
| **T17** | Bulk handler port (CONSOLIDATION recipe × ~280) | ⏸ |

See `_rewrite/docs/MODERNIZATION_PLAN.md` for the cluster-wide position of
this work, and the project commit history for the wave-by-wave detail.
