# TWorldSvrAsio — modernized cluster coordinator

Wire-compatible replacement for `Server/TWorldSvr/` (38 851 LOC,
30 files) running on the `FourStoryCommon` infrastructure (SOCI
pool, spdlog audit, health endpoint) and the `boost::asio` reactor
that the four shipped Asio daemons already use.

> Cluster context: [main README](../../README.md#overall-progress) ·
> patch catalog vs legacy Araz sources:
> [`_rewrite/docs/PATCH_README.md` §6](../../_rewrite/docs/PATCH_README.md#6-tworldsvr)

## Status — W3a-1 guild infrastructure

| Phase | Scope | Status |
|---|---|---|
| W1 | Scaffold + transport + dispatch stub | ✅ |
| W2 | CharRegistry (partitioned) + SessionPool + worker thread_pool + MW_ADDCHAR_ACK + MW_CLOSECHAR_ACK | ✅ |
| **W3a-1** | GuildRegistry + IGuildRepository (SOCI + Fake) + schema validator + DM_GUILDLOAD_ACK | ✅ |
| W3a-2 | PeerSession + SSSender (~30 senders) + 30 more guild handlers (Establish / Update / Disorg / Member add+leave+duty) | ⏸ |
| W3a-3 | Remaining ~45 guild handlers (tactics / volunteers / articles / cabinet / fame / contribution / PvP point) | ⏸ |
| W3b | Party + Corps | ⏸ |
| W4 | Friend + Chat + Soulmate | ⏸ |
| W5 | War + Castle + Tournament / TNMT | ⏸ |
| W6 | BR + Bow + Event + RPS + APEX / ARENA / BATTLEMODE | ⏸ |
| W7 | Item + Cash + MonthRank + CMGift + cutover hardening | ⏸ |

### W3a-1 — what's done, what's deferred

The 76 guild handlers split into three PRs because the W3a-1
**infrastructure** alone (registry + repo + schema validator + 1
handler) needs to land first so W3a-2/-3 can focus on handler
wiring without infrastructure churn.

**Done in W3a-1:**
* `services/guild_registry.{h,cpp}` — same 16-shard partitioning
  as CharRegistry; `TGuild` POD with the fields OnGuildLoadAck
  and (W3a-2) OnGuildInfoAck read. Per-guild mutex for the
  actor-model writes guild handlers need.
* `services/guild_repository.h` — `IGuildRepository` interface
  with the read path (`LoadAll`, `FindById`). The write path
  (`Save`, `Disorg`, member CRUD) lands in W3a-2 alongside the
  matching handlers.
* `services/soci_guild_repository.{h,cpp}` — SOCI implementation
  against `TGUILDTABLE` + `TGUILDMEMBERTABLE`. Two batched
  queries on boot warmup (one for guilds, one for all members
  joined back in-memory by `dwGuildID`) — beats per-guild
  fan-out at the legacy population sizes.
* `services/fake_guild_repository.{h,cpp}` — in-memory test impl
  with `AddGuild` seed API; deep-copies on `LoadAll` /
  `FindById` so test mutation doesn't bleed into the seed.
* `db/schema_validator.{h,cpp}` — fail-fast on missing
  `TGUILDTABLE` / `TGUILDMEMBERTABLE` columns. Optional warns
  for `TGUILDARTICLETABLE`, `TGUILDCABINETTABLE`,
  `TGUILDTACTICSTABLE` (the tables W3a-2/-3 will demand).
* `main.cpp` wires the guild registry, runs the schema validator
  when `[database]` is set, and warms the cache from
  `IGuildRepository::LoadAll`.
* `handlers/handlers_guild.cpp` — `OnGuildLoadAck` (DM_GUILDLOAD_ACK,
  wID=0x58FA). Parses the 21-field guild row + chief char back-link
  + cabinet count (items themselves are W3a-2 — discarded for now).
  Inserts a `TGuild` into the registry with the founder as the
  first member; gates on char-registered + key-match.
* Tests: `test_guild_registry` (5 scenarios), `test_guild_handlers`
  (4 wire scenarios + framer-survives-truncated-body), and
  `test_fake_guild_repo` (deep-copy isolation + nullopt on miss).

**Deferred to W3a-2 (each gated on a piece that doesn't exist yet):**
* **PeerSession wrapper** — every handler that needs to know which
  map server is on the other end. RW_RELAYSVR_REQ sets the wID;
  PeerRegistry indexes the live peers. `OnGuildLoadAck`'s
  `SendMW_GUILDESTABLISH_REQ` reply blocks on this.
* **SSSender layer** — the 4046 LOC counterpart to
  `Server/TWorldSvr/SSSender.cpp`. ~30 senders ship with W3a-2
  (`SendMW_GUILDESTABLISH_REQ` / `_INFO_ACK` / `_MEMBERLIST_ACK`
  / `_DISORGANIZATION_ACK` / etc.).
* **Cabinet item parser** — `OnGuildLoadAck` currently skips
  `wCabinetCount` items because the legacy `CreateItem` consumes
  a multi-field struct (Lib/Own/TProtocol). W3a-2 lands the item
  codec and the `TGuildItem` storage.
* **Guild-level table** (`FindGuildLevel(bLevel)` → max members,
  cabinet size, etc.) — a separate small table-cache PR;
  unrelated to handler porting.

**Deferred to W3a-3:**
* The remaining ~45 guild handlers (tactics members, alliance,
  volunteers, articles board, PvP record, fame contribution,
  point rewards).

### W2 — what's done, what's deferred

A grep of the legacy code showed that **TWorldSvr does NOT load
characters from the database** — it's a cluster-wide in-memory
index of chars that map servers have already loaded. The legacy
`DBAccess.h` has 78 `CSP*` stored-procedure wrappers but none of
them are char-load (the char-load CSPs live in TLoginSvr +
TMapSvr). So "char persistence" in TWorld really means **char
registry** — the in-memory `m_mapTCHAR` + `m_mapACTIVEUSER` —
which is exactly the global-lock target named in PATCH_README §6
W-2.

**Done in W2:**
* `services/char_registry.{h,cpp}` — 16-shard hash map with
  per-shard `std::shared_mutex` (fast read path under the W3
  guild lookups) + per-char `std::mutex` for field-level mutation
  ("per-char actor model" from W-2). Active-user index is a
  separate sharded set.
* DB infrastructure in `main.cpp` — when `[database]` is set, a
  `fourstory::db::SessionPool` + `boost::asio::thread_pool` come
  up. W2 doesn't issue any queries; W3a is the first phase that
  exercises this.
* `handlers/handlers_char.cpp` — `OnAddCharAck` (MW_ADDCHAR_ACK)
  inserts into the registry, marks user active, and handles the
  "additional connection" branch (TCharCon push). `OnCloseCharAck`
  (MW_CLOSECHAR_ACK) removes the entry, deactivates the user
  when no other char of theirs is online.
* Tests: `test_char_registry` (6 scenarios — insert/find/remove,
  duplicate-insert rejection, snapshot consistency, active-user
  index, concurrent inserts × 16k chars, shared_ptr lifetime),
  `test_char_handlers` (5 wire scenarios — happy path, additional
  connection, wrong key, close + user deactivation, stale close).

**Deferred to W3+ (each gated on a piece that doesn't exist yet):**
* **Peer-server registry** — `OnAddCharAck` records
  `TCharCon::server_id = 0` because the WorldSession doesn't yet
  carry the map-server's `wID`. W3 introduces a `PeerSession`
  wrapper (parallel to `Server/TControlSvrAsio/peer_session.h`)
  that knows which map server is on the other end.
* **`SendMW_ENTERSVR_REQ`** — the ACK back to the map server
  saying "yes, the char is registered". Requires a sender layer
  (TWorldSvr's `SSSender.cpp` counterpart) which is 4046 LOC; the
  first batch lands with the W3a guild work because the guild
  acks need it too.
* **`SendMW_INVALIDCHAR_REQ`** — fired on wrong-key collision
  ("possible session hijack" branch in the legacy module). Same
  blocker as ENTERSVR_REQ.
* **Cross-map `MW_DELCHAR_REQ` cleanup** — when CLOSECHAR_ACK
  arrives for an unknown char (stale close), the legacy server
  fires DELCHAR back to the map so the map cleans up its half.
  W2 logs it and skips; the map server's own close-loop converges
  the state without our reply.
* **Guild/party/BR/BoW side-effects on ADDCHAR_ACK** — the
  legacy module touches `m_pBOWModule`, `m_pBRModule`, and looks
  up guild membership when a char enters. Each of those modules
  is its own W3+/W6 PR.

The W2..W7 split was sized against a real `grep` of
`Server/TWorldSvr/SSHandler.cpp` (287 unique handlers across 4
families: OnMW=160, OnDM=88, OnCT=23, OnSM=16) plus 3 handlers in
`RWHandler.cpp`. See §4 for the per-feature handler counts that
drove the split.

> **Gating clause** (from PATCH_README §6): the legacy World↔Map
> wire contract is the bigger constraint for this port. W1 ships a
> transport-only scaffold that is safe to land now because the
> packet IDs aren't dispatched yet. **Per-feature handler work in
> W2+ should wait until [`TMapSvrAsio`](../TMapSvrAsio/README.md)'s
> MW wire shape stabilises**, otherwise W2 handlers may need to be
> reshaped when TMap's side lands. Holding the scaffold is OK; the
> infrastructure work in W2 (Soci char repo, dispatch family-file
> split) is independent of the wire shape.

## 1. Directory layout

```
Server/TWorldSvrAsio/
├── CMakeLists.txt                  — wired into root CMakeLists.txt
├── README.md                       — this file
├── tworldsvr.example.toml          — annotated reference TOML
├── main.cpp                        — boot, signals, SessionPool +
│                                     worker pool + CharRegistry
├── config.{h,cpp}                  — toml++ → AppConfig POD
├── world_server.{h,cpp}            — accept loop on DEF_WORLDPORT,
│                                     max_connections gate
├── world_session.{h,cpp}           — CPacket framing per peer
│                                     (plain TCP, no RC4 on SS link)
├── db/
│   ├── schema_validator.h          — boot-time TGUILD* column check
│   └── schema_validator.cpp        — required + optional probes
├── services/
│   ├── char_registry.{h,cpp}       — 16-shard partitioned char
│   │                                 index + active-user set
│   ├── guild_registry.{h,cpp}      — 16-shard partitioned guild
│   │                                 index, per-guild mutex
│   ├── guild_repository.h          — IGuildRepository interface
│   ├── fake_guild_repository.{h,cpp}
│   │                               — in-memory test impl w/ seed
│   └── soci_guild_repository.{h,cpp}
│                                   — SOCI impl, batched LoadAll
├── handlers/
│   ├── handlers.h                  — HandlerContext + decls
│   ├── dispatch.cpp                — switch on wID, drops unknown
│   ├── handlers_char.cpp           — MW_ADDCHAR_ACK +
│   │                                 MW_CLOSECHAR_ACK
│   └── handlers_guild.cpp          — DM_GUILDLOAD_ACK (W3a-1)
├── wire_codec.h                    — POD reader/writer + length-
│                                     prefixed string (CPacket layout)
└── tests/
    ├── test_dispatch.cpp           — wire framing + checksum
    ├── test_char_registry.cpp      — 6 scenarios (incl. 16k-char
    │                                 concurrent insert)
    ├── test_char_handlers.cpp      — 5 wire scenarios for ADD/CLOSE
    ├── test_guild_registry.cpp     — 5 scenarios (incl. 16k-guild
    │                                 concurrent insert)
    ├── test_guild_handlers.cpp     — 4 wire scenarios + framer-
    │                                 survives-truncated-body
    └── test_fake_guild_repository.cpp
                                    — deep-copy isolation + nullopt
```

W3a-2 adds `peer_session.h`, `peer_registry.{h,cpp}`, the first
~30 SSSender functions (`senders/senders_guild.{h,cpp}`), and ~30
mutating guild handlers (Establish / Update / Disorg / Member
add+leave+duty / Kickout). W3a-3 fills tactics / volunteers /
articles / cabinet / fame / contribution.

## 2. Build

The scaffold is wired into the root `CMakeLists.txt` next to the
four shipped Asio daemons:

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target tworldsvr_asio
ctest --test-dir build -R tworldsvr_asio --output-on-failure
```

W1 has one test — `tworldsvr_asio_dispatch` — that stands the
server up on an ephemeral port, frames a known wID, an unknown
wID, and a corrupt-checksum frame, and verifies the framer accepts
the first two and closes on the third.

## 3. Running

```bash
./build/Server/TWorldSvrAsio/tworldsvr_asio --config Server/TWorldSvrAsio/tworldsvr.example.toml
```

The W1 binary accepts SS connections on `DEF_WORLDPORT` (3815),
logs every inbound packet through the dispatch stub, and exposes
a `/healthz` HTTP endpoint on port 18087. It does NOT touch the
database — `[database]` in the TOML is parsed but unused until W2.

## 4. Phasing — how the SSHandler.cpp surface splits

Real counts from `grep` of the 287 unique `On(MW|DM|CT|SM)_*` names
in `Server/TWorldSvr/SSHandler.cpp` (the legacy monolithic switch),
plus 3 `OnRW_*` in `RWHandler.cpp`. The per-feature totals drive
the W2..W7 PR sizing — guild is its own PR (W3a) because at 76
handlers it is bigger than every other phase combined.

| Phase | Buckets | Handler count |
|---|---|---|
| W2 | CHAR (4) + USER (2) + OnRW (3) + ACTIVECHARUPDATE + GETCHARINFO + ADDCHAR + CHANGECHARBASE + CHECKCONNECT | ~15 |
| W3a | GUILD | 76 |
| W3b | PARTY (8) + CORPS (6) | 14 |
| W4 | FRIEND (18) + CHAT (3) + SOULMATE (8) | 29 |
| W5 | WAR (2) + CASTLE (5) + TOURNAMENT (15) + TNMT (3) | 25 |
| W6 | BR (3) + BOW (1) + EVENT (11) + RPSGAME (3) + APEX / ARENA / BATTLEMODE | ~30 |
| W7 | ITEM (5) + CASH (5) + MONTHRANK (3) + CMGIFT (7) + HELPMESSAGE + PVPRECORD + RESERVEDPOST + cutover hardening | ~30 |

Each ported handler in W2..W7 also gets its matching sender —
`Server/TWorldSvr/SSSender.cpp` is 4046 LOC / ~196 sender funcs,
so realistic effort per phase is **2× the handler count** (handler
+ sender + DB wrapper where applicable).

## 5. Architectural patches that land alongside the handler work

From PATCH_README §6:

| ID | Severity | Patch | Status |
|---|---|---|---|
| W-1 | 🟡 | Async DB + per-shard write queue (legacy `m_hDB` is a single DB thread serving all TMapSvr instances) | 🟡 W2 wired `SessionPool` + `boost::asio::thread_pool`; **W3a-1 ships the first SOCI consumer** (`SociGuildRepository::LoadAll` at boot). Per-shard write queue arrives in W3a-2 with mutating guild handlers. |
| W-2 | 🟡 | Partition the global `m_mapTCHAR` / `m_mapTGuild` locks (per-char actor model, per-guild grain) | ✅ **W3a-1 closes the in-memory half.** Char (W2) + guild (W3a-1) both run on 16-shard registries with per-entry mutex. The DB-side per-shard write queue is the remaining piece (W3a-2). |
| W-3 | ✅ | TWorldSvrAsio binary doesn't exist yet | ✅ closed by W1 scaffold |

Other related concerns from `_rewrite/docs/MODERNIZATION_PLAN.md`:

* **`SSHandler.cpp` 14 615 LOC monolithic switch** — replaced by
  one `handlers/handlers_<feature>.cpp` per W2..W7 phase, dispatched
  through the W1 `Dispatch` stub which grows one `case` per ported
  handler. Same shape as `Server/TControlSvrAsio/handlers/`.
* **`DMSender.cpp` raw-pointer handoff** — 48 `new` / 1 visible
  `delete` for outbound DB write packets. The Asio port owns
  outbound bodies through `std::vector<std::byte>` + move semantics
  (see `WorldSession::SendPacket`), so the lifecycle hazard never
  reappears.
* **Threading model** — legacy `_ControlThread` + `_WorkThread` +
  `_BatchThread` (plus a `MAX_THREAD` worker pool) collapse onto a
  single `io_context`. CPU-bound batch work that needs off-reactor
  execution rides on the `db_pool` introduced in W2.
