# TMapSvrAsio — Architecture

This is a layered C++20 Boost.Asio server. The layers below describe
**what each directory owns**, **how data flows through them**, and
**what stays out of each one** so a new contributor can find the
right place for new code without having to grep the design out of
commit messages.

## Top-level layout

```
Server/TMapSvrAsio/
├── main.cpp               Boot, signal handling, lifetime owner
├── config.h/cpp           TOML loader → AppConfig
├── map_server.h/cpp       TCP accept loop + per-connection coroutine
├── handlers.h             HandlerContext + handler decls
├── handlers/              One CS_/CT_ family per file (T1 split)
├── handlers_world.h/cpp   Inbound DM_/MW_ from World peer
├── wire_codec.h           POD reader / writer + length-prefixed string
├── domain/                Pure POD data types (no behavior)
├── services/              Interfaces + SOCI / Fake / InMemory impls
├── db/                    Query catalog, schema validators, narrow helpers
├── audit/                 Structured events + emitter
├── ops/                   Observability + admin endpoints
├── legacy_src/            Verbatim Server/TMapSvr/ — reference, not built
└── tests/                 (future) Unit + integration tests
```

## Layered data flow

```
┌──────────────────────────────────────────────────────────────┐
│  CLIENT (legacy 4Story client)                               │
└────────────────────┬─────────────────────────────────────────┘
                     │  TCP + RC4 inbound (optional)
                     ▼
┌──────────────────────────────────────────────────────────────┐
│  L1  Transport                                               │
│      tnetlib::AsioSession (RC4 + XOR + sequence framing)     │
│      MapServer accepts, max_connections gate, pre-auth       │
│      watchdog (T5), per-session rate limiter (T5)            │
└────────────────────┬─────────────────────────────────────────┘
                     │  DecodedPacket → (wId, body)
                     ▼
┌──────────────────────────────────────────────────────────────┐
│  L2  Dispatch                                                │
│      handlers/dispatch.cpp                                    │
│      Rate-limit gate → metrics (counter+latency) → audit     │
│      emit → OnXxx coroutine                                   │
└────────────────────┬─────────────────────────────────────────┘
                     │  body → typed wire decode
                     ▼
┌──────────────────────────────────────────────────────────────┐
│  L3  Handler                                                 │
│      handlers/{session,movement,npc,skill,quest,social,bow,  │
│                control}.cpp                                   │
│      Decode legacy CPacket body, validate, call services,    │
│      encode ack, broadcast via presence.                     │
└────────────────────┬─────────────────────────────────────────┘
                     │  service calls
                     ▼
┌──────────────────────────────────────────────────────────────┐
│  L4  Services (interfaces in services/, impls Soci/Fake)     │
│      IMapSessionValidator   IPlayerService                   │
│      IInventoryService      INpcService                      │
│      ISkillService          IQuestService                    │
│      IMonsterChart          ISpawnChart                      │
│      ICompanionService      IMonsterRegistry (in-memory)     │
│      ISessionRegistry       IChannelPresence  (in-memory)    │
│      IWorldClient           ILogPeer                         │
│      IRateLimiter           IAuditLog                        │
│      IMetrics                                                │
└────────────────────┬─────────────────────────────────────────┘
                     │  SOCI bind/fetch
                     ▼
┌──────────────────────────────────────────────────────────────┐
│  L5  Persistence                                             │
│      db/queries.h           Named SQL catalog                │
│      db/row_helpers.h       Narrow / SafeString helpers      │
│      db/schema_validator.*  Boot-time fail-fast              │
│      fourstory::db::SessionPool  (shared SOCI pool)          │
└──────────────────────────────────────────────────────────────┘
```

## Domain / Services / Handlers — what goes where

```
domain/        POD data only — no behavior, no service refs, no
               SOCI/Asio includes. Anyone can include freely.
                 CharSnapshot, InventoryRow, SkillRow,
                 QuestProgressRow, NpcRow, MonsterTemplate,
                 SpawnPoint, MonsterInstance, CompanionRow,
                 MapSessionInfo, Position

services/      Interface (I*) + at least one impl. Returns
               domain types; takes scalars / domain types / contexts.
               Implementations:
                 soci_*  — production, talks to SOCI/ODBC
                 fake_*  — in-memory header-only for tests/dev
                 in_memory_* — runtime state stores (session_reg,
                              presence, monster_registry)

handlers/      Per-message-id coroutines. Decode wire body, call
               services, encode wire response. No SQL, no
               business rules that aren't dictated by the wire.
```

## Peer / communication layer

```
┌─────────────────────┐   ┌─────────────────────┐
│  TLoginSvrAsio       │   │  TWorldSvr          │
│  (writes            │   │  (party / chat /    │
│   TCURRENTUSER)     │   │   item routing)     │
└──────────┬──────────┘   └──────────┬──────────┘
           │                          │
           │  shared SOCI             │  MW_ADDCHAR_ACK out,
           │  on TUSER                │  DM_LOADCHAR_REQ in
           ▼                          ▼
       ┌────────────────────────────────────────┐
       │  TMapSvrAsio (this server)             │
       └───────────┬───────────┬────────────────┘
                   │           │
              CT_* inbound     │  UDP audit
                   │           ▼
       ┌───────────▼───┐  ┌────────────────────┐
       │  TControlSvr   │  │  TLogSvrAsio       │
       │  (broadcast,   │  │  (structured       │
       │   kickout)     │  │   events sink)     │
       └────────────────┘  └────────────────────┘
```

## Observability data plane (T4 + T6)

```
Handler dispatch ──counter──→ ops::Metrics ──/metrics HTTP──→ Prometheus
                  │
                  ├─latency──→ ops::Metrics
                  │
                  └─audit────→ audit::AuditLog ──┬──→ spdlog file
                                                  └──→ ILogPeer (UDP) → TLogSvrAsio

Admin TCP shell (ops::AdminShell) ──→ status / kick / broadcast
                                       via HandlerContext services
```

## Boot sequence (main.cpp)

```
1.  Parse argv → config path
2.  LoadConfig (TOML)
3.  Set spdlog level
4.  Build io_context + signal_set (graceful shutdown wired here)
5.  Optional SOCI pool:
      ValidateUserSchema      (TCURRENTUSER)
      ValidateCharSchema      (TCHARTABLE)
      ValidateInventorySchema (TINVENTABLE)
      ValidateNpcSchema       (TNPCCHART)
      ValidateSkillSchema     (TSKILLTABLE)
      ValidateQuestSchema     (TQUESTTABLE + TQUESTTERMTABLE)
      ValidateMonsterSchema   (TMONSTERCHART + TMONSPAWNCHART)
      ValidateCompanionSchema (TCOMPANIONTABLE)
6.  Instantiate services (Soci* + InMemory*)
7.  Instantiate peers (WorldClient, LogPeer)
8.  Instantiate observability (AuditLog, Metrics, RateLimiter)
9.  Build HandlerContext (pointers into all of the above)
10. Construct MapServer with cfg.server + ctx
11. co_spawn MapServer.Run()
12. co_spawn HealthEndpoint.Run() (existing fourstory common)
13. co_spawn MetricsEndpoint.Run() (T6)
14. co_spawn AdminShell.Run() (T6, when configured)
15. io.run()
16. On signal: StopAccepting → drain_ms sleep → io.stop()
```

## What's intentionally outside scope of this server

- **Gameplay rules.** Damage formulas, AI decisions, quest engine
  evaluation, drop tables — none of these are here yet. The
  scaffolding receives the wire and routes to services, but the
  game-rules layer is what the consolidation pass adds.
- **Schema migrations.** DB layout is owned by external tooling;
  we validate at boot but don't run DDL.
- **Cluster orchestration.** World group membership, channel
  affinity, fail-over — handled by TControlSvr.
