# Legacy ↔ C# Gap Analysis

Date: 2026-05-17. Source for legacy counts: grep across `Server/T*Svr/*.cpp` for
`DWORD CT*SvrModule::On{CS|SS|CT|MW|DM|RW|SM}_*`.

## Executive summary

The rewrite has **wire-level + login + lobby + char lifecycle + Map handshake + World coordination** working. That covers the path from "TCP connect" to "client sits at the character-info screen". What's missing is **gameplay** — movement, combat, items, skills, quests, AI, party/guild, BR/BoW, auction — i.e. ~99% of what TMapSvr and TWorldSvr do.

In hard numbers (wire-visible handlers only, excluding internal SM_* and DB-bridge DM_*):

| Server | Legacy handlers (external) | C# ported | Coverage |
|--------|---------------------------:|----------:|---------:|
| TLoginSvr (Login) | **14** (CS_/CT_) | **11** | **79%** |
| TWorldSvr (World) | **186** (MW_/CT_/RW_) | **2** | **1%** |
| TMapSvr (Map) | **502** (CS_/MW_/CT_) | **3** | **0.6%** |
| TControlSvr (Control) | **65** (CT_) | **0** | **0%** |
| TPatchSvr | unknown (TODO) | **0** | 0% |
| TLogSvr | UDP only | **0** | 0% |
| TBRSvr | derived from TWorldSvr | **0** | 0% |
| TBoWSvr | derived from TWorldSvr | **0** | 0% |

Counts exclude:
- **DM_*** (legacy database-manager bounce) — collapsed by design in the C# port (Map/World talk to EF Core directly). 104 DM_ handlers in Map and 88 in World are intentionally absent, not gaps.
- **SM_*** (server-internal queue messages, never on the wire) — model maps to in-process commands; 24 in Map / 16 in World / 1 in Login.

---

## 1. TLoginSvr — 11 / 14 = **79% covered**

The most-complete server. Login flow + lobby flow + char lifecycle all in place. Missing handlers:

| Handler | What it does (legacy) | Port priority |
|---------|----------------------|---------------|
| `OnCS_TERMINATE_REQ` | Client-initiated clean logout. We handle disconnect via TCP close + `SessionTerminator` already — explicit TERMINATE just stamps timestamps. | 🟡 Low (covered indirectly) |
| `OnCS_TESTLOGIN_REQ` | Debug-stress login for Svr Tester. | ❌ Skip |
| `OnCS_TESTVERSION_REQ` | Test-mode version probe. | ❌ Skip |
| `OnCT_CTRLSVR_REQ` | Receives a control-server announcement. | 🟢 Needed when TControlSvr lands |
| `OnCT_EVENTMSG_REQ` / `OnCT_EVENTUPDATE_REQ` | GM broadcasts (server announcement, event start/end). | 🟢 Needed for ops |
| `OnCT_SERVICEDATACLEAR_ACK` / `OnCT_SERVICEMONITOR_ACK` | Stats / metrics for the GM dashboard. | 🟢 Replace with OTel + a small admin endpoint |
| `OnSM_QUITSERVICE_REQ` | Graceful shutdown signal. | 🟡 .NET host already handles SIGTERM |

**Effective coverage for player-facing functionality: 100%.** Admin/ops surface needs work once TControlSvr is ported.

### Login-side TODOs already in code

- `LobbyHandlers.cs:325` — TUSERINFOTABLE.bAgreement upsert on CS_AGREEMENT_REQ. Trivial when that entity is scaffolded into use.

---

## 2. TWorldSvr — 2 / 186 = **1% covered**

We have:
- `MW_CONNECT_ACK` (MapConnectHandler) — when a Map peer registers with us
- `MW_ADDCHAR_ACK` (MapAddCharHandler) — when Map relays a client's CS_CONNECT, we load TCHARTABLE and reply MW_ENTERSVR_REQ (custom inline form)

What's missing — **160 MW_* handlers** from `Server/TWorldSvr/SSHandler.cpp`. The big categories:

### 2.1 Subsystem-level gaps (entire features absent)

| Subsystem | Files (legacy) | LOC | Handlers | C# status |
|-----------|---------------|----:|---------:|-----------|
| **Party** | references in SSHandler | n/a | ~15 (MW_PARTY*) | ❌ no `TParty` equivalent |
| **Guild** | `TGuild.cpp` | 756 | ~35 (MW_GUILD*) | ❌ |
| **Tactics** (sub-guild bands) | in TGuild.cpp | n/a | ~12 (MW_TACTICS*) | ❌ |
| **Friends list** | in SSHandler | n/a | ~10 (MW_FRIEND*) | ❌ |
| **Chat routing** (global / guild / party) | in SSHandler | n/a | ~5 (MW_CHAT*) | ❌ |
| **Battle of War** (BoW) | `BowSystem.cpp` | 647 | ~10 (MW_BOW*) | ❌ |
| **Battle Royale** (BR) | `BRSystem.cpp` | 1419 | ~12 (MW_BR*) | ❌ |
| **Castle / siege** | in TWorldSvr.cpp | n/a | ~8 (MW_CASTLE*) | ❌ |
| **Tournament** | in TWorldSvr.cpp | n/a | ~6 (MW_TOURNAMENT*) | ❌ |
| **Auction house** | in SSHandler | n/a | ~5 (MW_AUCTION*) | ❌ |
| **Post / mail** | in SSHandler | n/a | ~6 (MW_POST*) | ❌ |
| **Char info propagation** | MW_LEVELUP, MW_CHGSEX, MW_CHGNAME etc. | n/a | ~20 | ❌ |
| **Cash shop** | in SSHandler | n/a | ~8 | ❌ |
| **Event / quest broadcast** | in SSHandler | n/a | ~5 | ❌ |

### 2.2 World-side TODOs already in code

| File | Line | TODO |
|------|------|------|
| `MapAddCharHandler.cs` | 102 | Negative-result paths of MW_ENTERSVR_ACK (CN_ALREADYEXIST / CN_INTERNAL) |
| `MapAddCharHandler.cs` | 131 | In-memory `TCHARACTER` cache for channel-hop |
| `MapAddCharHandler.cs` | 138 | Re-entry path for already-loaded char (validate (key, ip, port) tuple) |
| `MapAddCharHandler.cs` | 143 | Party / guild / tactics rehydration on enter |
| `MapConnectHandler.cs` | 95 | World-bootstrap fan-out (battle-time / cash-item / event tables to fresh Map peer) |

---

## 3. TMapSvr — 3 / 502 = **0.6% covered**

The big one. We have:
- `CS_CONNECT_REQ` (ConnectHandler) — auth client + tell World
- `CS_CONREADY_REQ` (ReadyHandler) — client ready, gate the InitMap fan-out
- `MW_ENTERSVR_REQ` (EnterSvrHandler) — receive char snapshot from World, populate state

Plus the **InitMap fan-out** in `MapInitOrchestrator` (sends CS_ADDCONNECT_ACK + CS_CHARINFO_ACK with empty list bundles).

### 3.1 Gameplay subsystems entirely absent

These are the foundational entity-and-state types. Until at least the entity hierarchy exists, no gameplay handlers can be ported.

| Subsystem | Legacy file | LOC | What it is |
|-----------|------------|----:|------------|
| **TObjBase** | `TObjBase.cpp` | 4992 | Base class for every entity (player/monster/npc/item-on-ground). AOI broadcast lives here. |
| **TPlayer** | `TPlayer.cpp` | 7833 | Player entity. Stat math, equipment effects, HP/MP regen, all things player. |
| **TSelfObj** | `TSelfObj.cpp` | (TBD) | Self-managed object (different from monsters). |
| **TMap** | `TMap.cpp` | 2111 | The map/zone. Owns cells + spawn tables + bb-tree. |
| **TCell** | `TCell.cpp` | 671 | Spatial grid cell. AOI lookups. |
| **TChannel** | `TChannel.cpp` | (TBD) | Channel = parallel instance of a map. |
| **TInven** / **TItem** | `TItem.cpp` | 701 | Inventory + item state, equip rules, refine, gems. |
| **TSkill** / **TSkillTemp** | `TSkillTemp.cpp` | 578 | Active/passive skills, cooldowns, maintain effects (HoT/DoT). |
| **TMonster** | `TMonster.cpp` | 2444 | Monster entity. |
| **TMonsterAI** + **TAICmd\*** | 13 TAICmd_*.cpp files | ~900 total | State-machine AI (Roam, Attack, Follow, Getaway, Gohome, Lottery, Refill, …). |
| **TNpc** | `TNpc.cpp` | (TBD) | NPC entity (quest givers, shopkeepers). |
| **TRecallMon** | `TRecallMon.cpp` | 374 | Summoned pets / mercs. |
| **TCompanionClass** | `TCompanionClass.cpp` | (TBD) | Companion / mount system. |
| **Quest dispatch** | `Quest.cpp` + 24 `Quest*.cpp` files | ~1700 total | 24 quest condition/reward types. |
| **TTextLinker** / **TTextLinkData** | `TTextLinker.cpp` | (TBD) | Chat with item/skill/quest hyperlinks. |
| **TParty** (server-side state) | `TParty.cpp` | (TBD) | Mirrors World's authoritative party state. |
| **TGuild** (server-side state) | `TGuild.cpp` | (TBD) | Mirrors World's authoritative guild state. |
| **TCorps** (tactics in-instance) | `TCorps.cpp` | (TBD) | Tactics squads in BoW/BR. |
| **BRSettings** / **TBowSettings** | header constants | n/a | Per-mode tuning. |

### 3.2 Handler-level gaps (top 10 by visible impact)

Sorted by what blocks the smallest set of next observable behaviors:

| Handler | What it does | Unlocks |
|---------|-------------|---------|
| `OnCS_MOVE_REQ` | Player movement | Walking around |
| `OnCS_ACTION_REQ` | Stop / sit / wave / emote | Visible behavior |
| `OnCS_ROTATE_REQ` | Turn in place | Camera/visual |
| `OnCS_NORMAL_ATTACK_REQ` | Auto-attack | Combat opens |
| `OnCS_SKILL_REQ` | Cast skill | Real combat |
| `OnCS_CHAT_REQ` | Say / shout / whisper / global | Social |
| `OnCS_ITEM_REQ` | Use item from inventory | Potions / consumables |
| `OnCS_EQUIP_REQ` | Equip / unequip item | Gear visible |
| `OnCS_NPC_REQ` | Talk to NPC | Quest pickup, shops |
| `OnCS_REVIVAL_REQ` | Respawn after death | Death loop closes |

These 10 alone make the game **playable in single-player against monsters**. The rest of the 287 CS_* handlers cover: parties, guilds, friends, post, auction, cash shop, gambling, fishing, mounts, companions, hatching, tournaments, sieges, gods/godballs, secure code, PCBANG, anti-cheat hooks, ...

### 3.3 Map-side TODOs already in code

| File | Line | TODO |
|------|------|------|
| `ReadyHandler.cs` | 130 | Original "port InitMap" — now DONE in `MapInitOrchestrator`. Comment is stale, can be removed. |
| `MapConnection.cs` | 12 | Factor out shared `TcpHostServer` to dedupe Login/Map/World connection code. |
| `MapInitOrchestrator.cs` | 150-151 | dwPrevExp / dwNextExp from TLevelChart (needs the chart table scaffolded into a service). |

---

## 4. TControlSvr — 0 / 65 = **0% covered**

The GM/ops dashboard backend. Untouched. Functions include:
- Service start/stop control of remote workers (legacy IPC, modern would be systemd / k8s)
- Live monitor (user count, sessions) — replaced by OpenTelemetry metrics
- Kick user / ban chat / item find / cash shop operations
- Event manager (start / end / configure events)
- Patch deployment trigger
- GM authority levels

**Replacement strategy** (not a 1:1 port):
- Per-server `/healthz` + `/metrics` endpoints (Phase E2 OTel skeleton ready)
- Small ASP.NET Core admin API (auth via opaque token) for the GM operations
- The 65 CT_* handlers map to ~15 REST endpoints

---

## 5. TPatchSvr, TLogSvr, TBRSvr, TBoWSvr — 0% covered

| Server | Scope | Replacement plan |
|--------|-------|------------------|
| **TPatchSvr** | File-transfer protocol for client patches (~12 packets) | Replace with HTTP/CDN serving versioned files. Match client behavior at the protocol layer. |
| **TLogSvr** | UDP audit log aggregator (79 packet types) | Replace with Serilog → Seq or OTel → log store; legacy packets convert to log events. |
| **TBRSvr** | Battle Royale matchmaker | Variant of TWorldSvr; port after main World gameplay loops are stable. |
| **TBoWSvr** | Battle of War scheduler | Same — wait for World stability. |

---

## 6. Database / Stored Procedures — partial coverage

| | Legacy | C# port |
|---|--------|---------|
| **TGLOBAL_RAGEZONE procs** | 60 | 2 ported (`TLogin` → `AuthService`, `TCreateChar`+`TDeleteChar` → `CharService`). 58 untouched. |
| **TGAME_RAGEZONE procs** | 263 | 0 ported. |

The plan from `_rewrite/docs/SCHEMA.md` stands: business logic gets re-implemented in C# services, not translated to PL/pgSQL. The 321 untouched procs are tracked there.

Notable untouched groups:
- **TGetCharInfo / TLoadCharacter / TSaveCharacter** — read/write the active char snapshot (needed by Map when not using inline form).
- **TGetGuildInfo / TLoadGuild / TSaveGuild** — guild persistence.
- **TPostSend / TPostRead / TPostDelete** — mail / post.
- **TAuction\*** — auction house lifecycle.
- **TBattleRank\* / TArenaRank\*** — ranking systems.

---

## 7. Infrastructure / cross-cutting

| Concern | Status |
|---------|--------|
| `FourStory.Shared` namespace organisation | ✅ ErrorCode, LoginResult, HwidValidator, TelemetrySetup |
| Wire framing + crypto | ✅ Full (`FourStory.Protocol`) |
| Per-session TCP framing | ✅ `PacketSession` |
| Session state per connection | ✅ Login `SessionState`, Map `MapSessionState`, World `WorldConnectionState` |
| Persistence — EF Core scaffold | ✅ 295 entities, MSSQL provider |
| Persistence — Postgres migration | ❌ Not started. Code-first migrations not generated. |
| Password hashing | ✅ BCrypt with transparent upgrade |
| Disconnect cleanup | ✅ `SessionTerminator` (TCURRENTUSER + TLog.timeLOGOUT) |
| Observability — Serilog → console | ✅ |
| Observability — OpenTelemetry tracing/metrics | ✅ Skeleton (opt-in, OTLP) |
| Health checks | ❌ No `/healthz` endpoint (workers don't host HTTP yet) |
| Docker compose | ✅ `_rewrite/deploy/` |
| Wolverine messaging | ❌ Not integrated (user wants this in stack) |
| Orleans grains | ❌ Not integrated (planned for game-state in Phase 3+) |
| Mapster mapping | ❌ Not integrated |
| Anti-cheat | ❌ Replaced by no-op stub (CS_SECURITYCONFIRM_REQ accepts everything) |
| Graceful shutdown | 🟡 .NET host handles SIGTERM; sessions close cleanly. No "drain mode" yet. |
| Rate limiting on login | ❌ |
| Audit log of GM actions | ❌ |

---

## 8. Recommended next-up sequence

Given the gap distribution, the natural sequence to unblock real gameplay is:

### Phase 3a — **Entity foundation** (no external observable change yet)
Port `TObjBase` + `TMap` + `TCell` + `TPlayer` as C# types. This is **prerequisite** for any movement / combat / AOI handler.

**Modern shape:** these are perfect Orleans grain candidates.
- `PlayerGrain` (one per active charId)
- `MapGrain` / `ChannelGrain` (one per (worldId, channelId, mapId))
- `MonsterGrain` (one per spawned monster)
- AOI via map grain's spatial index

**Estimate:** week of focused work for skeleton; entity types take time.

### Phase 3b — **First 10 gameplay handlers** (visible: walking + chat)
With entities in place, port the top-10 list in §3.2.

### Phase 3c — **Subsystem ports** (party + guild + post + auction, in order)

### Phase 4 — **TControlSvr replacement** (admin API)

### Phase 5 — **Postgres migration** + Wolverine for inter-server / Mapster for DTOs

### Phase 6 — **TPatch + TLog + TBR + TBoW**

---

## 9. Done vs. remaining (one-line view)

```
Phase 0 (protocol RE)             ████████████████████ 100%
Phase 1 (.NET skeleton + crypto)  ████████████████████ 100%
Phase 2 (Login + lobby + char)    ████████████████████ 100%
Phase 2.5 (Map handshake + World coord)  ████████████ 60%
Phase 3 (gameplay handlers)       ░░░░░░░░░░░░░░░░░░░░ 1%
Phase 4 (admin/control)           ░░░░░░░░░░░░░░░░░░░░ 0%
Phase 5 (Postgres + Wolverine + Orleans + Mapster) ░░░░ 0%
Phase 6 (Patch/Log/BR/BoW + client) ░░░░░░░░░░░░░░░░░░ 0%
```

The thing that lights up an actual game world is **Phase 3a (entities) + 3b (top-10 handlers)**. Everything else is polish or future work.
