# TWorldSvrAsio — modernized cluster coordinator

Wire-compatible replacement for `Server/TWorldSvr/` (38 851 LOC,
30 files) running on the `FourStoryCommon` infrastructure (SOCI
pool, spdlog audit, health endpoint) and the `boost::asio` reactor
that the four shipped Asio daemons already use.

> Cluster context: [main README](../../README.md#overall-progress) ·
> patch catalog vs legacy Araz sources:
> [`_rewrite/docs/PATCH_README.md` §6](../../_rewrite/docs/PATCH_README.md#6-tworldsvr)

## Status — W3a-4d CoOffloadIf wiring + guild-level table cache

| Phase | Scope | Status |
|---|---|---|
| W1 | Scaffold + transport + dispatch stub | ✅ |
| W2 | CharRegistry (partitioned) + SessionPool + worker thread_pool + MW_ADDCHAR_ACK + MW_CLOSECHAR_ACK | ✅ |
| W3a-1 | GuildRegistry + IGuildRepository read API + schema validator + DM_GUILDLOAD_ACK | ✅ |
| W3a-2 | PeerSession + PeerRegistry + first SSSender batch + OnRW_RELAYSVR_REQ + OnGuildLoadAck fires its reply | ✅ |
| W3a-3 | CharRegistry name index + TChar identity fields + OnMW_CHANGECHARBASE_ACK + OnRW_ENTERCHAR_REQ + OnRW_RELAYCONNECT_REQ | ✅ |
| W3a-4 | TChar.guild_id back-pointer + TGuild::FindMember/RemoveMember + OnMW_GUILDLEAVE_ACK | ✅ |
| W3a-4b | `services/guild_constants.h` + IGuildRepository write API (SetDisorg / UpdateMemberDuty / UpdateFame / RemoveMember) + OnGuildDisorganizationReq + OnGuildDutyAck + OnGuildFameAck (broadcast) | ✅ |
| W3a-4c | `services/guild_broadcast.h` (BroadcastToGuildMembers helper) + IGuildRepository::AddMember / IncrementContribution + OnMW_GUILDKICKOUT_ACK + OnMW_GUILDCONTRIBUTION_ACK + OnDM_GUILDMEMBERADD_REQ | ✅ |
| **W3a-4d** | All SOCI writes wrapped in `fourstory::db::CoOffloadVoidIf(ctx.db_pool, …)` — closes the last W-1 work from PATCH_README §6 + `GuildLevelCache` mirror of TGUILDCHART (10-row immutable lookup) + `SociGuildLevelRepository` + `FakeGuildLevelRepository` + schema_validator extension + HandlerContext.guild_levels | ✅ |
| W3a-5+ | OnMW_GUILDPEER_ACK (needs guild-level cache — now available) + Invite / Article / Cabinet / Tactics / Volunteers / PvP record / Point reward / Cabinet item codec | ⏸ |
| W3b | Party + Corps | ⏸ |
| W4 | Friend + Chat + Soulmate | ⏸ |
| W5 | War + Castle + Tournament / TNMT | ⏸ |
| W6 | BR + Bow + Event + RPS + APEX / ARENA / BATTLEMODE | ⏸ |
| W7 | Item + Cash + MonthRank + CMGift + cutover hardening | ⏸ |

### W3a-4d — what landed

Pure infrastructure PR. **Closes the last remaining item on
PATCH_README §6 W-1** (async DB + per-shard write queue): every
SOCI write the guild handlers issue now runs on the
`fourstory::db::thread_pool` worker — the io_context coroutine
suspends across the DB roundtrip instead of blocking the reactor.
Also introduces `GuildLevelCache` so the W3a-5+ peerage / member-
cap gates have the legacy `m_pTLEVEL` lookup they need.

* `services/guild_level_cache.{h,cpp}` — small read-only mirror
  of `TGUILDCHART` (10 rows max, indexed by `bLevel`). `LoadFrom`
  drops out-of-range rows with a debug log; `Find(level)` is a
  lock-free direct-index lookup (the table is immutable post-load).
  Mirrors `tagTGUILDLEVEL` field-for-field including the 5-slot
  `bPeer[]` array.
* `services/guild_level_repository.h` — `IGuildLevelRepository`
  read-only interface. The chart is operator-tuned + offline-
  reloaded, so no write API.
* `services/fake_guild_level_repository.{h,cpp}` — in-memory
  impl with `AddRow` seed API. Used by tests + the no-DB dev
  path.
* `services/soci_guild_level_repository.{h,cpp}` — SOCI
  `SELECT * FROM TGUILDCHART` once at boot.
* `db/schema_validator.cpp` — required-column check for
  TGUILDCHART (bLevel / dwEXP / bMaxCnt / bCabinetCnt /
  bPeer1 / bPeer5). Missing chart aborts boot when `[database]`
  is configured.
* `main.cpp` — instantiates GuildLevelCache, calls
  `LoadFrom(guild_level_repo->LoadAll())` before the listener
  spawns, wires `ctx.guild_levels`.
* `handlers/handlers.h` — HandlerContext gains
  `const GuildLevelCache* guild_levels`.

**CoOffloadVoidIf wiring** — every `ctx.guild_repo->*` write
call in `handlers/handlers_guild.cpp` now reads:

```cpp
co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
    [repo = ctx.guild_repo, ...] {
        repo->XxxYyy(...);
    });
```

Covered handlers: `OnGuildLeaveAck` (RemoveMember),
`OnGuildDisorganizationReq` (SetDisorg), `OnGuildDutyAck`
(UpdateMemberDuty × 1 or 2 for chief promotion),
`OnGuildFameAck` (UpdateFame), `OnGuildKickoutAck`
(RemoveMember), `OnGuildContributionAck`
(IncrementContribution), `OnGuildMemberAddReq` (AddMember).

When `ctx.db_pool == nullptr` (no `[database]` configured + no
`worker_threads` set) the lambda runs in-line on the current
coroutine thread — `CoOffloadVoidIf` is the nullptr-tolerant
overload. Tests rely on this fall-through.

Build verified: cmake + ctest -R tworldsvr_asio (13/13 passed)
on GCC 13.3 Ubuntu noble. The existing wire tests pass
unchanged — the offload is observationally transparent.

Test added
- `test_guild_level_cache` — 4 scenarios: empty cache, LoadFrom
  filters out-of-range rows (level 0 + level > kMaxGuildLevel
  dropped), second LoadFrom replaces, FakeGuildLevelRepository
  round-trips.

Deferred to W3a-5+ (now unblocked by this PR)
- OnMW_GUILDPEER_ACK with full `CheckPeerage` validation
  (reads `guild_levels->Find(level)->peer_slots[bPeer-1]`)
- OnDM_GUILDCABINETMAX_REQ (reads `cabinet_count`)
- Member-cap gate on the W3a-5 OnMW_GUILDINVITEANSWER_ACK path
  (reads `max_count`)

### W3a-4c — what landed

Three more mutating-guild handlers + the reusable broadcast
helper that handlers can compose into multi-peer fan-outs
without rewriting the LOBYTE-of-wID lookup every time.

* `services/guild_broadcast.{h,cpp}` — `BroadcastToGuildMembers`
  walks a list of member char_ids, finds each TChar, resolves
  `main_server_id` → PeerSession, and `co_await`s a caller-
  supplied `GuildPacketBuilder` per recipient. Returns the
  number of recipients actually reached. Used by
  `OnGuildFameAck` (broadcast new fame to every online member)
  and `OnGuildKickoutAck` (1-element broadcast to the kicked
  char's main peer). Future Article / Peer / Announcement
  handlers ride the same path.
* `services/guild_repository.h` — write API gains `AddMember`
  and `IncrementContribution`. Mirrors CSPGuildMemberAdd +
  CSPGuildContribution.
* `services/fake_guild_repository.{h,cpp}` — impls plus
  `Call::Kind::{kAddMember, kIncrementContribution}` records
  so tests assert "the right CSP fired with the right args".
  The `Call` struct grew from 5 → 8 fields (`c/d/e` cover the
  contribution's silver/cooper/pvp); existing W3a-4b
  push_back call sites updated to zero-fill the new fields.
* `services/soci_guild_repository.cpp`
  - `AddMember`: INSERT into TGUILDMEMBERTABLE, fall back to
    UPDATE on PK conflict (legacy CSPGuildMemberAdd has
    upsert semantics via the SP). `bLevel` is documented as
    unused — legacy stores it on TCHARTABLE, not the member
    row.
  - `IncrementContribution`: two UPDATEs — one against
    TGUILDTABLE for the totals delta (gold/silver/cooper/exp/
    pvp_total_point/pvp_useable_point), one against
    TGUILDMEMBERTABLE for the member's `dwService`.
* `handlers/handlers_guild.cpp`
  - `OnGuildKickoutAck` (MW_GUILDKICKOUT_ACK, wID=0x903B):
    enforces the "officer kick only by chief" gate from
    SSHandler.cpp:3364, removes the target from the guild +
    clears their `TChar.guild_id`, persists via
    `RemoveMember`, then double-broadcasts MW_GUILDLEAVE_REQ
    (reason=kLeaveKick) — one reply to the requesting peer,
    one routed to the kicked char's main peer via
    `BroadcastToGuildMembers` (single-element list).
  - `OnGuildContributionAck` (MW_GUILDCONTRIBUTION_ACK,
    wID=0x90D8): validates not-disorg, rejects exp on
    max-level guild with `kFail`+zeros (legacy
    SSHandler.cpp:4052-4058), bumps guild totals + member
    service score in-memory, persists via
    `IncrementContribution`, replies with
    MW_GUILDCONTRIBUTION_REQ.
  - `OnGuildMemberAddReq` (DM_GUILDMEMBERADD_REQ, wID=0x58D8):
    pure DB write via `AddMember` — no reply. Legacy fires
    CSPGuildMemberAdd from the BatchThread without an ACK
    path; in-memory member insertion happens on the
    OnMW_GUILDINVITEANSWER_ACK side (W3a-5+).
  - `OnGuildFameAck` refactored from an inline member-loop to
    a single `BroadcastToGuildMembers` call.
* `senders/senders.h` + `senders_guild.cpp`
  - `SendMwGuildContributionReq` (8-field reply).
* Dispatcher routes the 3 new wIDs.

Test changes
- `test_guild_mut_handlers` extended 9 → 12 scenarios:
  cancel-disorg + KICKOUT producing 2 MW_GUILDLEAVE_REQ
  replies (chief + kicked target) with `kLeaveKick`
  reasons; CONTRIBUTION applying deltas + reply + repo call
  recorded; DM_MEMBERADD persisting + no reply + framer
  stays alive.

Deferred to W3a-4d (focused infrastructure PR)
- Wire all SOCI writes through
  `fourstory::db::CoOffloadIf(ctx.db_pool, …)` so DB I/O
  doesn't block the io_context thread (closes the W-1
  remaining work from PATCH_README §6).
- Guild-level table cache (`FindGuildLevel(bLevel)`) — the
  legacy `m_pTLEVEL` lookup that backs `CheckPeerage` and
  `MAX_GUILD_LEVEL` gates. Needed before OnMW_GUILDPEER_ACK
  can ship its full validation.

Deferred to W3a-5+
- OnMW_GUILDPEER_ACK (peerage change with CheckPeerage gate)
- OnMW_GUILDINVITE / INVITEANSWER (in-memory member-add side)
- OnMW_GUILDARTICLE* (guild board)
- OnMW_GUILDCABINET* (guild storage) + cabinet item codec
- Tactics / volunteers / PvP record / point reward (~20)

### W3a-4b — what landed

Three more guild mutating handlers (`DM_GUILDDISORGANIZATION_REQ`,
`MW_GUILDDUTY_ACK`, `MW_GUILDFAME_ACK`) plus the IGuildRepository
write API and the single-source-of-truth enum header that the
remaining ~20 guild handlers in W3a-4c will rely on.

Pre-W3a-4b bug fixes
- `senders::kGuildLeaveSelf = 1` was wrong — NetCode.h:448 puts
  `GUILD_LEAVE_SELF` at offset 12. Wire-incompatible against
  legacy peers; not caught by W3a-4's round-trip test.
- `kGuildDutyChief = 1` was wrong — NetCode.h:1983 puts
  `GUILD_DUTY_CHIEF` at offset 2 (offset 1 is VICECHIEF). Same
  silent round-trip bypass. Fixed in `services/guild_constants.h`
  as a single source of truth; old per-file ad-hoc constants
  removed.

Adds
- `services/guild_constants.h` — every GUILD_RESULT + GUILD_DUTY
  value mirrored from `Lib/Own/TProtocol/include/NetCode.h` with
  the line refs kept in sync. Imported by all handler / sender
  files that need a guild enum.
- `services/guild_repository.h` — write API: `SetDisorg`,
  `UpdateMemberDuty`, `UpdateFame`, `RemoveMember`. Mirrors the
  legacy CSP* set 1:1.
- `services/fake_guild_repository.{h,cpp}` — in-memory impls
  + a `Calls()` snapshot of all mutating calls so handler tests
  can assert "the right CSP-equivalent ran".
- `services/soci_guild_repository.{h,cpp}` — UPDATE / DELETE
  against TGUILDTABLE / TGUILDMEMBERTABLE; soft-fail on driver
  errors (logs + returns false rather than aborting the handler).
- `handlers/handlers_guild.cpp`
  - `OnGuildDisorganizationReq` (DM_GUILDDISORGANIZATION_REQ,
    wID=0x589F): persists via `SetDisorg`, flips
    `TGuild.disorg`+`disorg_time` under the guild lock, ACKs
    the requesting peer with `MW_GUILDDISORGANIZATION_REQ`.
  - `OnGuildDutyAck` (MW_GUILDDUTY_ACK, wID=0x9036): validates
    requester is in-guild, gates against "same duty / self /
    vice-cap reached / target has tactics", does the chief-handover
    dance (legacy SSHandler.cpp:3457-3465: outgoing chief →
    DUTY_NONE before promoting target), persists, ACKs the
    requesting peer.
  - `OnGuildFameAck` (MW_GUILDFAME_ACK, wID=0x90E0): checks the
    PvP-point budget (`kPvPointCostFameChange = 30 000`), debits
    points, updates `TGuild.fame`/`fame_color`, broadcasts
    `MW_GUILDFAME_REQ` to **every online guild member's main map
    peer** (legacy SSHandler.cpp:4391-4407). `kNoPoint` reply
    branch for insufficient budget.
  - `OnGuildLeaveAck` now wires the persistence call
    (`RemoveMember`); the in-memory remove and reply paths from
    W3a-4 are unchanged.
- `senders/senders.h` + `senders_guild.cpp`
  - `SendMwGuildDisorganizationReq` (3-field reply)
  - `SendMwGuildDutyReq` (4-field reply)
  - `SendMwGuildFameReq` (6-field reply; broadcast-shaped)

Test changes
- `test_guild_mut_handlers` extended from 4 → 9 scenarios.
  Added: load second guild with 2 members + 50 000 PvP points,
  duty promotion + fake-repo call recorded, fame change + 2-member
  broadcast + PvP points debited, disorg flip + persistence,
  disorg-rejected duty change (legacy gate).
- `test_guild_handlers`: chief member's `duty` assertion updated
  to the correct `GUILD_DUTY_CHIEF = 2`.

Deferred to W3a-4c
- Cross-peer broadcast helpers (RW_CHANGENAME_ACK fan-out,
  DM_FRIENDERASE_REQ on rename, MW_GUILDPEER routing)
- Wire OnGuild* handlers through `fourstory::db::CoOffloadIf`
  so SOCI writes don't block the io_context thread
- The remaining ~20 mutating guild handlers (Establish ACK /
  Member add / Invite answer / Kickout / Peer / Contribution /
  Article / Cabinet)
- Cabinet item codec (`Lib/Own/TProtocol/ITEM` struct port)
- Guild-level table cache

### W3a-4 — what landed

The first mutating-guild handler — `OnMW_GUILDLEAVE_ACK` —
exercises the full stack the remaining ~24 guild handlers will
use: locate the char's guild via the new `TChar.guild_id`
back-pointer, mutate `TGuild.members` under the per-guild lock,
clear the back-pointer atomically with the member removal, and
fire the matching MW sender back to the originating peer.

* **TChar.guild_id** — back-pointer matching legacy
  `pTCHAR->m_pGuild->m_dwID`. 0 = no guild. The registry-owned
  `TGuild` holds the strong reference; this is a non-owning id
  so there's no shared_ptr cycle between TChar and TGuild.
* **TGuild::FindMember / RemoveMember** — in-place helpers that
  expect `TGuild.lock` to be held by the caller. Linear over
  `members` (typical size < 200); a name-keyed secondary index
  arrives in W3a-4b once benchmarks justify it.
* **OnGuildLoadAck update** — sets `TChar.guild_id = guild_id`
  on the founder when the guild is inserted, closing a W3a-1
  TODO. The char→guild link now survives across handler calls
  without requiring a `FindByName` scan.
* **OnEnterCharReq update** — now resolves the requesting
  char's guild fields (`guild_id`, `guild_chief`, `duty`) from
  the `GuildRegistry` instead of always returning zeros. Disorg
  guilds short-circuit to `guild_id=0` (legacy parity). Stale
  back-pointers (guild unloaded but `TChar.guild_id` still set)
  log a warning and reply with zeros — defensive, since W3a-4b
  may add a Disorg sweep that clears these on the next pass.
* **OnGuildLeaveAck** (MW_GUILDLEAVE_ACK, wID=0x9034) — full
  port of the legacy `SSHandler.cpp:3571` semantics: char +
  key validation, member removal under the guild lock,
  `TChar.guild_id = 0`, `MW_GUILDLEAVE_REQ` reply with
  `GUILD_LEAVE_SELF`. The legacy `SendDM_GUILDLEAVE_REQ` DB
  persistence + cross-peer broadcast defer to W3a-4b once the
  `IGuildRepository::RemoveMember` write API lands.
* **SendMwGuildLeaveReq** — second sender in the MW-guild
  family (after SendMwGuildEstablishReq from W3a-2). Wire
  layout matches `SSSender.cpp` exactly.

New test: `test_guild_mut_handlers` — 4 wire scenarios across a
real loopback peer: load → leave → ENTERCHAR after leave returns
zero. Verifies the `TChar.guild_id` ↔ `TGuild.members` invariant
is preserved across mutation.

### W3a-3 — what landed

Three handlers that turn the relay-map ↔ world handshake from a
stub into a real round trip, plus the TChar identity expansion
that all subsequent guild / party / chat handlers need to reach
beyond `dwCharID`.

* **CharRegistry name index** — sharded secondary index
  (16 shards × `std::shared_mutex`) keyed by `ToUpper(name)` so
  `FindByName` is case-insensitive (matches legacy
  `m_mapTCHARNAME` semantics). `Rename(char_id, new_name)` is
  atomic: insert under new name → store on TChar → drop old
  entry, rejecting cluster-wide name collisions. `Rename(id, "")`
  drops only the index entry — used by future CloseChar prep
  paths.
* **TChar identity fields** — `name`, `country`, `aid_country`,
  `klass`, `level`, `race`, `sex`, `face`, `hair`, `map_id`,
  `pos_x/y/z`. Each is the modern mirror of one legacy
  `tagTCHARACTER` member; OnRW_ENTERCHAR_REQ now answers with the
  real country / map id instead of zeros.
* **OnChangeCharBaseAck** (MW_CHANGECHARBASE_ACK, wID=0x911B) —
  branches on `bType` for FACE / HAIR / RACE / SEX / COUNTRY /
  AIDCOUNTRY / NAME. NAME drives `CharRegistry::Rename` and
  refuses on collision; the friend / soulmate / guild-app
  notification fan-out defers to W4 / W3a-4 (they need the
  matching registries).
* **OnEnterCharReq** (RW_ENTERCHAR_REQ, wID=0x999C) — looks up by
  name, validates `dwCharID`, replies with `RW_ENTERCHAR_ACK`
  carrying the char's cluster state. Guild / party / corps /
  tactics ids stay zero-default in W3a-3 (those registries don't
  hold member back-pointers yet); W3a-4 fills them.
* **OnRelayConnectReq** (RW_RELAYCONNECT_REQ, wID=0x99A5) —
  routes `MW_RELAYCONNECT_REQ` to the peer matching the char's
  `main_server_id` (LOBYTE of the peer's wID, set at
  RELAYSVR_REQ time). Legacy parity for `FindMapSvr(bMainID)`.
* **PeerRegistry::SnapshotExcept** — efficient
  "every other peer" iterator. Used by `OnRelaysvrReq` to fan
  out the legacy `(*it).second->SendMW_RELAYCONNECT_REQ(0)`
  broadcast when a new relay registers.
* **Sender batch** — `SendRwEntercharAck` (16-field reply,
  legacy RWSender.cpp:34) + `SendMwRelayconnectReq` (legacy
  SSSender.cpp:3062).

New tests:
* `test_char_name_index` — 7 scenarios: insert+rename round trip,
  case insensitivity, rename clears the old entry, drop-only
  rename (empty new_name), collision refusal, Remove drops both
  indices, concurrent renames on disjoint chars × 4096 entries.
* `test_char_base_handlers` — 5 wire scenarios driving every
  `bType` branch on a real socket + a follow-up valid packet
  proves the framer stays alive after the unknown-bType drop.
* `test_rw_lookup_handlers` — 4 wire scenarios across two real
  peer connections: ENTERCHAR hit + miss, RELAYCONNECT routing
  by main_server_id, and the cluster broadcast triggered by a
  second RELAYSVR registration.

### W3a-2 — what landed

PeerSession wraps every accepted WorldSession so handlers see
map-server identity (wID + nation flag) without going through
PeerRegistry on every packet. PeerRegistry indexes the live
peers by wID; OnRelaysvrReq registers the peer on RW_RELAYSVR_REQ
and WorldServer's HandleConnection exit-path unregisters cleanly.
The dispatch signature changes to `shared_ptr<PeerSession>` —
all char/guild handlers updated.

The first batch of SSSender lands under `senders/senders_*.cpp`:
* `SendRwRelaysvrAck` — reply to RW_RELAYSVR_REQ (nation + empty
  operator/msg lists; full lists arrive in W5 castle-war).
* `SendMwGuildEstablishReq` — completes the OnGuildLoadAck round
  trip (was a TODO in W3a-1; now actually fires the ACK back to
  the originating map server).

OnAddCharAck now stamps the real `server_id` (LOBYTE of peer wID)
on the inserted `TCharCon` — closes a W2 TODO. OnGuildLoadAck
now sends its long-pending MW_GUILDESTABLISH_REQ reply, completing
the legacy `SSHandler.cpp:9019` round-trip.

New tests: `test_peer_registry` (6 scenarios — empty, Register,
sentinel rejection, duplicate retains original, idempotent
Unregister, Snapshot) and `test_relay_handlers` (5 wire scenarios
— register+ACK with nation echo, sentinel reject, duplicate-wID
on a second socket keeps original entry, disconnect unregisters).

### W3a-1 — what was done

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

**Deferred to W3a-3 (each gated on a piece that doesn't exist yet):**
* **OnRW_ENTERCHAR_REQ** — needs `CharRegistry::FindByName`
  (secondary name index) + the per-char `country` / `name` fields
  that arrive with the W2 OnMW_CHANGECHARBASE_ACK port.
* **OnRW_RELAYCONNECT_REQ** — needs `PeerRegistry::Find` (already
  shipped in W3a-2) plus `SendMW_RELAYCONNECT_REQ` (one-line
  sender, lands with the next batch).
* **Cabinet item codec** — OnGuildLoadAck still skips items.
  The legacy `Lib/Own/TProtocol/ITEM` struct is non-trivial; W3a-3
  ports both the codec and the storage container.
* **Guild-level table cache** (`FindGuildLevel(bLevel)`) — a small
  separate read-only table; lands with the first mutating handler
  that needs it (probably `MW_GUILDLEAVE_ACK`).
* **~25 mutating guild handlers** — Establish / Update / Disorg /
  Member add+leave+duty / Kickout / Fame / Contribution / Peer.
  Each adds 1–2 senders to the sender table.
* **Cluster-wide RELAYCONNECT broadcast** — when a relay registers,
  legacy fans an MW_RELAYCONNECT_REQ to every other peer. Drops
  in once the broadcast helper lands (it's also needed by the
  W3a-3 announcement/chat-broadcast handlers).

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
├── peer_session.h                  — wraps WorldSession with wID +
│                                     nation (per-map-server identity)
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
│   ├── soci_guild_repository.{h,cpp}
│   │                               — SOCI impl, batched LoadAll
│   └── peer_registry.{h,cpp}       — by-wID hash index of live
│                                     PeerSession objects
├── handlers/
│   ├── handlers.h                  — HandlerContext + decls
│   ├── dispatch.cpp                — switch on wID, drops unknown
│   ├── handlers_char.cpp           — MW_ADDCHAR_ACK +
│   │                                 MW_CLOSECHAR_ACK
│   ├── handlers_guild.cpp          — DM_GUILDLOAD_ACK
│   └── handlers_relay.cpp          — RW_RELAYSVR_REQ (W3a-2)
├── senders/
│   ├── senders.h                   — outbound packet builders;
│   │                                 family-file split inside
│   ├── senders_relay.cpp           — SendRwRelaysvrAck
│   └── senders_guild.cpp           — SendMwGuildEstablishReq
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
    ├── test_fake_guild_repository.cpp
    │                               — deep-copy isolation + nullopt
    ├── test_peer_registry.cpp      — 6 scenarios for Register /
    │                                 Find / Unregister (W3a-2)
    └── test_relay_handlers.cpp     — 5 wire scenarios for
                                      RW_RELAYSVR_REQ / ACK (W3a-2)
```

W3a-3 lands OnRW_ENTERCHAR_REQ + OnRW_RELAYCONNECT_REQ + ~25
mutating guild handlers (Establish / Update / Disorg / Member
add+leave+duty / Kickout / Fame / Peer / Contribution) plus the
cabinet item codec and the guild-level table cache. The remaining
~50 guild handlers (tactics / volunteers / articles / pvp record /
point reward) ship in W3a-4.

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
