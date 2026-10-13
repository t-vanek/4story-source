# Phase A static source review — 2026-10-13

This review accompanies the owner's analysis/implementation request. It is a
bounded continuation of the existing `Server/T*SvrAsio/` implementation, not a
claim that the full original source inventory has been reconstructed. Original
sources, client, backup-derived SQL and published native code were read only.
No build, test, executable, server, client, container, migration, importer or
runtime verification was invoked by the analytical agent. Historical test and
compilation evidence is not evidence for the changes made in this turn.

## Baseline and implementation boundary

The current README, modernization README, capability matrix, continuation,
protocol and full-rewrite status consistently describe Phase A as incomplete.
The latest recorded native social slice is `1f1eb59` / `e709281`, migrations
through066. Current code confirms native World process ownership, directed
friends/groups/soulmates and reserved/offline mail. World main still constructs
legacy guild/item/cash/service/rank/war/event/RPS/tournament/BOW repositories
after the legacy validator (`Server/TWorldSvrAsio/main.cpp:203` onward). This is
not a complete PostgreSQL-only World bootstrap.

`legacy-modern-coverage.csv` retains `ALL.RESPONSIBILITIES` as `NOT_ANALYZED`.
`evidence/source-inventory.json` includes additional source servers and tools,
including TBRSvr, TBoWSvr and Happy. A small set of scoped capability rows cannot
be used as proof of complete source responsibility coverage.

## Priority handoff: soulmate search/registration economy

These are the nearest connected social gaps. At review start modern Map
`handlers/friend.cpp` and `handlers/dispatch.cpp` implement END and incoming
standalone state but not client SEARCH/REGREADY/REG and their economic results.
World already has candidate selection and persistence handlers; exposing only
new Map relays would create a split relationship/currency transaction.

| Responsibility | Original evidence | Contract |
| --- | --- | --- |
| SEARCH request | `Server/TMapSvr/CSHandler.cpp:13034` | Ordinary primary/map; ProtectTutorial; two BYTE NPC bag/slot fields; silence; sufficient search money; optional NPC item; lowest neighbor level with absolute level difference less than3, default200; original World relay |
| Preview REGREADY | `Server/TMapSvr/CSHandler.cpp:13125` | String name/BYTE bag/BYTE slot; silence; self-name silently ignored; existing own name ALREADY; optional NPC gate; `bReg=FALSE`; no price or item consumption |
| REG request | `Server/TMapSvr/CSHandler.cpp:13198` | Secure-code unlock prompt before body decode; tutorial/silence/self/already gates; sufficient registration cost; optional NPC gate; `bReg=TRUE` |
| SEARCH response | `Server/TMapSvr/SSHandler.cpp:14230` | Rechecks funds/NPC; consumes one NPC item and charges search price even when World reports no match; emits MONEY before final SEARCH ACK; own relationship updated only on success |
| REG response | `Server/TMapSvr/SSHandler.cpp:14307` | `bResult=SUCCESS && bReg=TRUE` consumes optional NPC item, charges registration money, sets directed relation, emits MONEY; preview responds with REGREADY fields |
| Price catalog | `Server/TMapSvr/TMapSvr.cpp:2267`; `Server/TMapSvr/DBAccess.h:1047` | `TLEVELCHART.dwRegCost` and `.dwSearchCost` directly; no substituted formula; current archival schema preserves both integer DWORD bit patterns (`database/postgresql/002-legacy-snapshots.sql:2668`) |
| NPC token | Request/response handlers above | Active only if bag is not INVEN_NULL and slot is not INVALID_SLOT. Allowed maps0/8; item exists, kind IK_NPCCALL, count positive on request. Map/position errors vary between request REG and response REG; do not unify return codes |
| Selection window | Original World/Map handlers and `soulmate_constants.h` | SEARCH uses strict distance less than3. REG rejects distance greater than3, so exactly3 is accepted for explicit registration |
| Directed ownership | `_rewrite/docs/schema/procs/TGAME_RAGEZONE/TSoulmateReg.sql` | Inserts/updates initiator row only. Target may own an independent relation. END stamps time, DEL deletes exact directed pair |

SEARCH and REG ACKs are **not** just a result/identity/name/region tuple.
`Server/TMapSvr/CSSender.cpp:4604` and `:4634` always append three DWORD money
fields, including failure replies. The independent client
`Client/TClient/CSHandler.cpp:12925` and `:13045` reads them on success. Exact
layout: BYTE result, DWORD target, INT32-length raw string, DWORD region,
DWORD gold, DWORD silver, DWORD copper. REGREADY is BYTE result/string/BYTE
bag/BYTE slot (`CSSender.cpp:4622`, client`:12993`). Existing separate MONEY
packet order must also survive.

`MW_SOULMATE_ACK` exists as a protocol define, but no original handler/sender
body was found. It must not be treated as a proved wire contract for a new
commit confirmation. Server-side durable receipts or a documented internal
extension require their own ownership/recovery design and must add no fields
to original client packets.

An implementable atomic repair is a World-created pending selection bound to
World generation, current Map generation/session and operation identity; Map's
existing native mutation finalizer can lock/consume this authority and commit
NPC count, purse, relationship, quest effects and checkpoint together. A
relation committed by World followed by a separate Map charge is incomplete.
Same valid request bytes after completion are new source actions, not a content
hash replay. Unknown COMMIT must not create another charge or registration.
Cache projection after durable commit and response-loss recovery are separate
required responsibilities.

## Initial-load and other soulmate gameplay

Original `TWorldSvr.cpp:4288` CheckSoulmateEnd ignores absent/timed rows and
stamps only the owner's row when absolute owner/partner level gap exceeds3.
Original `SSHandler.cpp:9129` onward applies this on load for own and incoming
online owner projections. The pre-turn native load expires silence rows but
does not perform this initial-level check. The actual entry path is
`handlers_char.cpp::OnAddCharAck` → LoadForChar → ApplyFriendLoad →
OnEnterSvrAck, with standalone state before FRIENDLIST. Ending a relation during
initial load must not leak the old nonzero descriptor in the subsequent list.
Incoming source TVIEW joins describe the directed target; live RegSoulmate
describes the owner. That documented source discrepancy must remain explicit.

Party soul experience has a narrowly placed original producer:
`Server/TMapSvr/TMonster.cpp:613` through`:676`, after party distribution,
level-rate and benefits/buff bonus, before the party member's own GainExp(TRUE).
`TPlayer.cpp:4667` requires giver's own directed mate and no silence; recipient
must be in the same selected party vector, primary and alive. It gives
`DWORD(dwEXP * 0.1)` via GainExp(FALSE), followed by explicit EXP_ACK even for
zero gain. Applying this bonus indiscriminately to quest/ITEMUSE/AUTOEXP changes
the original game. Modern combat's old killer-only EXP prototype
(`handlers/combat.cpp:190`) is not the complete native party death producer.

Soul HP remains a separate original responsibility. `TPlayer.cpp:4686` onward
excludes duel/tournament, requires receiver below15% maximum HP, then examines
nearby primary donors whose own directed soulmate targets the receiver, with
same nonzero party and no silence. `TransSoulHP` at`:4741` keeps donor at65%
maximum, transfers surplus with receiver cap and broadcasts donor HPMP. Its
call-site integration, multi-character transaction and effects are still open;
implementing soulmate XP alone cannot close soulmate gameplay.

## Static ownership/concurrency audit

Confirmed native social protections:

- `postgresql_world_owner.cpp::BeginWorldTransaction` locks the current owner
  token row and validates its backend advisory lock before a transaction.
- `postgresql_friend_repository.cpp::LockActors` locks sorted Map process rows
  before sorted unsigned account locks. GuardActor holds claim/current-user/
  character/owner SHARE locks, verifies current Login key, nondeleted character,
  primary server and phase through commit. Load explicitly permits claimed/
  loaded; mutations require ready. Mutual acceptance fences both participants.
- Secure erase/group delete reads disabled/unlocked flags under the same account
  lock as Map security; no credential bytes are exposed. Source protected erase
  intentionally bypasses unlock.
- SocialOperationGate is a coroutine permit over validation/offload/projection;
  ordinary friend/soulmate and level/entry handlers acquire it. Native repository
  faults poison future writes and stop World; there is no automatic replay.
- Original invitation authority is bounded to inviter key and recipient object;
  refusal consumes it. Post-await actor registry/peer/key checks exist on
  ordinary mutations. Map incoming friend handlers wait for pending local
  mutation and recheck exact client connection before cache use.

Remaining review/fix boundaries at the initial source snapshot:

1. Soulmate SEARCH candidate scan does not reject closing/logout/unready
   candidates, and post-await checks only target ID existence, not the chosen
   object's identity (`handlers_soulmate.cpp::OnSoulmateSearchAck`). REG checks
   pointer identity, but its original repository mutation validates only the
   initiator's current claim. New atomic offers must record/revalidate the
   selected target, country/level and live-generation assumptions.
2. CloseChar is outside SocialOperationGate (`handlers_char.cpp:654`). Region,
   topology and lifecycle can change while a social database task runs; the
   permit alone does not prove successor protection. Do not add gate acquisition
   to nested CloseChar paths without checking existing callers already holding
   the permit.
3. Mutual friendship post-await checks counterpart key/closing/logout, while
   ready-primary peer topology is only fully validated for the requester in
   cache projection. PG protects both claims through commit, but later fanout
   still needs exact current audience checks and recovery scenarios.
4. Native incoming load deletion can expire an incoming directed row while
   locking only the logged-in participant account. World serialization currently
   orders its own social writes, but any new Map-side relationship writer must
   preserve a compatible account/row lock order and expiry/publication barriers.
5. There is no durable proof that a successful cache/packet projection reached
   the client. Commit-before-send is correct persistence but is not reliable
   recovery/delivery by itself.

## Concrete next World slices and hard dependencies

| Slice | Production path and original data | Limit of a bounded port |
| --- | --- | --- |
| Native guild-level catalog | `main.cpp:206` and`:377` → IGuildLevelRepository::LoadAll → GuildLevelCache → member/peer/cabinet/tactics handler caps. Complete15-column contract in `soci_guild_level_repository.cpp:19`; backup TGUILDCHART has11 rows; archive002`:1944` | Needs separate proved operational publication/receipt, unsigned DWORD conversion, strict missing/duplicate checks and actual native main/validator selection. It does not complete guild mutable state or member admission |
| Guild mutable bootstrap | `main.cpp:204`; SociGuildRepository::LoadAll uses unqualified legacy ORM tables and optional reward history | Outer failure returns empty guild vector (`soci_guild_repository.cpp:90`), not valid native hydration. Map fresh load explicitly refuses any app_world.guild_membership (`postgresql_map_load.cpp:494`), so World-only repo cannot complete guild login |
| Admin item state | CT_ITEMSTATE → `handlers_ctrlsvr.cpp:41` → ChangeState → original successful prefix Map/Control replies; TItemStateChange.sql returns1 when no item then updates bInitState | Needs authoritative operational state overlay/publication shared with immutable Map catalogs, native World ownership and all Map consumers. Current simple SOCI update is not a completed PG-only feature |
| Cash sales | CT campaign → Map confirmation → `handlers_cashshop.cpp:164` → SociCashSaleRepository | EXEC TGAME TCashItemSale forwards to original TGLOBAL procedure. Cross-DB sale authority, full cash-shop buyer/refresh, persistent campaign recovery and reply correlation remain open |
| Help messages | CT_HELPMESSAGE → `handlers_service.cpp:89`; THelpMessage.sql keyed bID with SMALLDATETIME/VARCHAR2048 | Current World broadcasts before best-effort ignored save. Modern Map load/live/timer/user consumers are missing; original ones are TMapSvr.cpp:1882/11440 and SSHandler.cpp:18539. A new PG repository alone is not end-to-end |
| Map cleanup | SM_DELSESSION → `handlers_service.cpp:158` → CloseChar awaits → ClearMapCurrentUser | Actual source global procedure deletes by resolved Map endpoint IP/port. A broad native endpoint delete after await could delete a successor; require current generation/session claim fencing and revalidation, not just an initial peer check |

The old cabinet LIST comment says "stub", but current
`handlers_guild.cpp:2666` copies and sends cabinet_items. Keyword matches alone
are not sufficient to classify a branch. Conversely an installed handler with
an unported repository or explicit fresh-load rejection is not a complete path.

### Guild-level publication source detail

`Server/TWorldSvr/DBAccess.h:6` through`:38` proves the exact15-column contract.
All columns are NOT NULL in backup metadata and reconstructed
`database/legacy-reconstructed/TGAME_RAGEZONE.tables.sql:724`. `dwEXP` is a
signed SQL INT holding the original DWORD bit pattern; the other14 columns
are TINYINT/BYTE0..255. The exact order is bLevel, dwEXP, bMaxCnt, bMinCnt,
bCabinetCnt, bTacticsCnt, bBattleSetCnt, bGuardCnt, bRoyalGuardCnt, bTurretCnt,
bPeer1, bPeer2, bPeer3, bPeer4, bPeer5. Primary key is bLevel.

Original World load (`TWorldSvr.cpp:906`) inserts every chart row without a
level filter. FindGuildLevel at`:3976` performs exact lookup and returns NULL
for missing keys. MAX_GUILD_LEVEL10 caps progression (`NetCode.h:72`,
`TGuild.cpp:286`); it does not justify dropping original level0. CTGuild starts
at0 and successful guild creation sets1 (`TGuild.cpp:7`, `SSHandler.cpp:3135`).
The pre-turn modern cache drops0 and returns NULL for Find(0); retaining level0
is a source-fidelity correction. The public exact-count profile proves11 rows,
but does not by itself prove the precise key set or table value hash.

The pinned backup fingerprints are in
`_rewrite/docs/database-reconstruction/evidence/artifact-inventory.json:308`:
TGAME_RAGEZONE.bak34,009,088 bytes,
SHA2562cff1635d523322d3de8ddafaeacc3335581dc338dca901239ac80288f51d259;
TGLOBAL_RAGEZONE.bak36,560,896 bytes,
SHA256fca545bcb90bcc72aa4f3514a6177c2b88d2b0bf71bf847b0128f090a20877fb.
No private full archival manifest/row value hash was available in the locations
inspected in this turn. Those values must be supplied by the existing full pinned
archive, not reconstructed from count or invented. The publication pattern is
`migrate.py::load_snapshot(include_historical=True)` plus
`activate_actor_catalog.py::verified_actor_import` and offline_owner_barrier:
fresh checkpoint/row/value/text-byte comparison, stable source table locks,
World/Map offline namespace barriers, a dedicated catalog selection/history and
release view. `011-character-reference-catalog.sql` is an established view/release
model; a new guild publication must leave existing applied migrations untouched.

## Review of the in-progress implementation in this turn

This is a static review of newly written sources, not compiled or tested code.
The new Map `postgresql_map_soulmate.cpp` uses source level costs, including the
proved first-level fallback (`TMapSvr.cpp:6420`), source purse width/tails,
NPC-call sentinel/map/kind/count gates and delayed item/purse/relationship
finalization in the existing ChangeQuest/checkpoint transaction. Its registration
response repeats secure/funds/target guards. These response-time rechecks and
atomic commit intentionally repair the original async relationship/Map charge
split and require acceptance as modern differences.

New migration067 holds server-only pending/prepared/consumed/cancelled/abandoned
offer authority, exact request/response bytes, source Map owner/connection/epoch,
World token and selected target identity. World prepares source-shaped replies
without committing the relationship; Map consumes the prepared offer once with
NPC item, purse, directed row, social audit and quest operation/checkpoint. A
successful cache poll is not a second purchase or client success. World
RunSoulmateMaintenance now projects only the current committed directed link
into cache and uses exact-target END to avoid stamping a newer pairing.

Observed lock order: World preparation acquires World SHARE → sorted Map
process SHARE → sorted unsigned accounts UPDATE → actor claim/current-user/
character SHARE → offer UPDATE. Map finalization acquires World SHARE → source
and target Map process SHARE → sorted unsigned accounts UPDATE → own claim →
offer UPDATE → target claim → directed link. Map process SHARE order is not
sorted, but these locks are mutually compatible and acquired before accounts;
no account/process inversion was found in the inspected paths. This is an
inspection conclusion, not deadlock/concurrency proof. Cross-Map mutual choices,
takeover/update locks, timeout/rollback and high-bit identities require later PG
acceptance.

Distinctly-cased self registration deserves explicit treatment: original Map's
raw name equality is case-sensitive, while original World FindTChar uppercases
(`TWorldSvr.cpp:2736`). The new finalizer rejects selected self ID and rolls back;
this is a safe modern behavior difference, not proof of the source silent gate.
Read-only REGREADY carries no economic receipt. New code must still be reviewed
for all final production registrations, grants, startup queries and callback
leases after integration finishes. Initial-load checking is now deferred to a
ready-primary periodic maintenance path; exact original-client packet timing and
response-loss/restart recovery are unverified. Full party soul XP/HP producers
remain open.

## Deferred verification, no execution in this phase

After the complete implementation gate, build all six production targets and
inspect warnings; apply only new migrations in an isolated PG database with
actual runtime grants and pinned offline imports. Run source-derived layout
checks for every soulmate result including failure purse tails, exact source
money/item ACK order, preview bag/slot echo and non-ASCII bytes. Independently
exercise SEARCH gap2/3, REG gap3/4, candidate sort/ties, NPC maps0/8 and sentinels,
missing/count-zero NPC items, price affordability, secure prompt-before-decode,
silence boundary and initial-load own/incoming checks.

Atomicity acceptance must cover NPC/title/purse/relationship/checkpoint rollback,
concurrent secure/item/protected edits, two-Map takeover during awaits, dead
World generation, target logout/replacement/level-up, commit-success response
loss, unknown commit without replay, repeated legitimate identical requests,
stale offers and consumed-offer recovery. Test counterpart lock ordering and
source party XP/HP only through their actual native producers. Upgrade/import
barriers and malformed/tail/frame bounds need separate adversarial coverage.

The unchanged real client still lacks an executable identity/build/assets in
the available evidence. Source packet consumers can identify required fields,
but cannot prove binary compatibility. Full game functionality and actual
client acceptance remain unverified. Quest dynamic actions/non-request producers,
general combat/effects/AI/rewards, storage/trade/crafting variants, guild/party/chat
protection, pets/mounts, wars/events/ranks/tournaments/BOW/BR and the remaining
original operational tools are retained in scope and remain open.

## Follow-up static review of the written continuation

The subsequent source reads include migration068, the native guild-level reader,
the offline publication tool, standalone Map soulmate confirmation and the
explicit maintained-effect sampler. These observations still do not establish
compilation, database execution or unchanged-client acceptance.

### Guild-level publication and cache

`068-native-world-guild-levels.sql` requires an explicitly activated verified
archival import and a TGUILDCHART checkpoint with the same source/target value
hash and11 rows. `activate_world_guild_levels.py` verifies the complete historical
manifest, both pinned backups, the source manifest, checkpoint hashes, a fresh
target value hash and original text-byte hash where present. It uses the existing
offline Map/World owner barrier and stable archival locks. It refuses an actual
keyset other than0..10; because only the11-row count is public evidence, an
unexpected actual keyset must be reconciled with the private source snapshot.
The numerical chart's actual value hash has not been observed in this review.

The native repository reads all15 fields explicitly, checks BYTE ranges,
preserves signed SQL INT bits as DWORD EXP, rejects missing/duplicate/out-of-range
level keys and fails bootstrap instead of returning an empty/default chart.
Main chooses this repository only with the native World owner. Required legacy
guild/war/event checks remain in the validator, so this does not establish a
complete PostgreSQL-only World bootstrap. The cache now preserves sourcelevel0;
its count increments only for a previously absent key and resets on reload.
Existing `tests/test_guild_level_cache.cpp:62,65,82` assertions described the old
discarded zero row when first reviewed. The implementation owner repaired them:
the fixture now expects four levels and a present level0, with additional
duplicate-key and reload count assertions. These are statically inspected test
changes; no test was executed.

### Soulmate authority and projection

Map commits the pending request receipt before sending the source-shaped MW
relay. World changes only pending→prepared while fencing the current source and,
on success, target actors. Map consumes the prepared offer in the same native
transaction as NPC debit, purse debit, directed relationship, quest receipt and
checkpoint. It checks the exact response bytes, current World process, source
Map owner/connection/epoch and current target key/server, owner, readyclaim,
name/country/level/class before committing. The offer stores the selected target
key/server rather than the target's complete connection/epoch tuple; target
leave/return and takeover remain explicit later acceptance cases.

SEARCH NOTFOUND still spends the source search fee and becomes cancelled;
successful linkage becomes consumed. REG failure, money/NPC/security refusal
becomes cancelled with a committed operation receipt. REGREADY is a read-only
preview and has no offer/economy write. A consumed/stale/missing offer cannot be
replayed from equal response bytes. Equal subsequent legitimate CS request
bytes may create a new invocation. Unknown write outcomes close/mark uncertain;
the receipt poll projects cache only and never sends another purchase success.

The new REG reply NPC validation precedes the additional affordability check,
as in the source response order. The source response ignored UseMoney's return;
the native atomic refusal and repeated security check are deliberate stricter
modernization behavior, not proof of identical source failure behavior.

`ConfirmSoulmateState` checks the current native directed SQL row, exact target
and rawname decoding or exactsilence before standalone cache publication.
The `BeginProjection` operation spans DBawait, cache update and packet send.
Although transfer may enter TransferOut while an operation is active,
`handlers_main_transfer.cpp:31` drains all prior operations before Freeze;
therefore the inspected projection does not race a completed frozen transfer.
World's own cache reconciliation still requires restart/reload/packet-loss
acceptance and must not be described as an already-proven recovery path.

The observed multi-actor order is WorldprocessSHARE→MapprocessSHARE→sorted
unsigned accountUPDATE→claim/currentuser→offer/relation. No concrete lock cycle
was found in these inspected paths. The existing generic Map LockAccount and
LockClaim bind positive unsigned identities as bigint, while the new soulmate
and World account locks bind their signed32-bit SQL representation. This
high-bit discrepancy is a fail-closed availability limitation until reconciled
across the actual admission/service paths; unsigned-range parity is unverified.
Specifically, `postgresql_map_service.cpp:44,51` binds user/key/character as
positive bigint values, whereas `postgresql_map_soulmate.cpp:15,136` and World
`postgresql_friend_repository.cpp:11,82` preserve their signed SQL INT bits.
For example, source DWORD 0x80000000 is stored as INT -2147483648 but the generic
Map guard searches for bigint 2147483648. The continuation does not silently
authorize a different account; it may refuse an otherwise valid source actor.
High-bit user, character and session-key cases require a coherent admission,
claim, checkpoint, transfer and social audit rather than a localized claim that
the complete DWORD range is already supported.

### Maintained-effect save and transfer contract

Original `TSkill.cpp:100–113` samples exact millisecond remaining durations;
zero start means an indefinite maintained effect and its remaining value is0.
A finite expired effect is omitted, never converted into indefinite0.
`SSSender.cpp:1093–1135` writes learned skill/rank/cooldown and the eight durable
maintained fields at `m_dwSaveTick`, excluding TSTORE/localzone skills for save.
The transfer writer at2197–2232 samples at its captured currenttick and includes
the same eight durable fields with source transfer field order/count widths.
Client `CSHandler.cpp:505–550,10891–10919` consumes the remaining DWORD and creates
its own local deadlines. Process-local clocks are not database/wire fields.

Save serialization itself only omits expired maintained rows and optionally
emits SKILLEND. It does not run AUTOEXP or HangExpBuff. The original periodic
path `TPlayer.cpp:955–1005` runs CheckMaintainSkill before SAVECHAR, and
`TObjBase.cpp:2527–2559` performs runtime erase plus903 benefit renewal there.
The reconstructed TSaveSkillMaintain inserts directly into the maintained table;
TSaveCharDataEnd's skill/maintained DELETE/INSERT blocks are commented out.
These modified procs do not prove a clean durable timer flow; the runtime and
packet sampling contracts above are the reliable source evidence.

The written native sampler is explicit only for checkpoint/logout/transfer.
Its ephemeral before/at metadata proves a monotonic sample with unchanged
seven identity fields, and it does not get encoded. Fresh checkpoints write
the sampled eight-field rows; graph validation aligns only the proved buff
sample. Periodic checkpoint defers an already-expired effect to the runtime
producer and publishes sampled clocks only after commit. Frozen TransferOut
logout retains the exact prepared graph. General effect expiry/producers remain
open: an unsupported finite effect in a full incoming graph can make periodic
sampling defer indefinitely because the current progression producer covers
only its supported native effect set. This needs later source-driven acceptance,
not a claim that all original combat/effects are implemented.

### Implemented World bootstrap: original server-message catalogue

TSVRMSGCHART has51 backup-proved rows, an INT key interpreted as DWORD and a
NOTNULL VARCHAR256 Latin1_General_CI_AS_KS value (`DBAccess.h:665–675`,
`TGAME_RAGEZONE.tables.sql:2360–2363`). Source World loads the whole DWORD map
at `TWorldSvr.cpp:875` and resolves exact key or NAME_NULL at5481–5487.
The continuation reuses immutable059
`game_compat.server_message_release/server_messages` and the existing
`activate_quest_catalog.py --server-messages` publication. It adds no competing
publication or new message migration. `postgresql_server_messages.cpp` checks
the active verified Map/character/actor receipts and TSVRMSG checkpoint, equal
source/target value and byte hashes, all51 raw rows, unique unsigned keys, no NUL
and source width at most256 bytes. The read uses the native World owner fence
and a repeatable snapshot. Public evidence still does not supply the real
value/byte hashes or the complete observed source keyset.

The continuation corrects the modern RW sender's former string key to the
source DWORDkey+STRINGvalue layout (`RWSender.cpp:25–27`). Source constants in
`CTProtocol.h:55–61` give TMS_NORECEIVER5, NAME_OPERATOR6 and MSG_CHAR_LOGOUT7.
The actual relay now receives the loaded DWORD map. Event announcements resolve
NAME_OPERATOR from it, and the two TMS no-receiver branches use the source
uppercase hexadecimal NetString envelope (`TWorldSvr.cpp:5490–5495`), whose
lengths count source raw bytes. `ServerNetString` implements the eight length
digits followed by unchanged header/body bytes, with explicit WORD bounds.

Non-ASCII publication bytes require the configured source ANSI profile and an
exact PostgreSQL encoding round trip. The repository returns the original
bytes, not a rewritten UTF-8 message. Missing keys resolve to source NAME_NULL;
bootstrap errors throw instead of substituting fabricated catalogue text.
Main/context/CMake and the new runtime SELECT grants are present in the
inspected production path. Encoding round trips and source readers establish
the intended contract, not acceptance by the unchanged binary client.

The final read found a mandatory key5 bootstrap condition not required by the
existing publisher, which requires1/6. Original GetSvrMsg accepts a missing key
with NAME_NULL. The implementation owner removed this unsupported condition;
the retained operator6 check agrees with existing publication. The actual pinned
keyset remains unobserved, and the empty NetString body preserves the original
missing-key fallback.

Existing relay/TMS/event fixtures were inspected for stale string-key or empty
no-receiver expectations; no concrete stale assertion was found there. Their
mostly empty relay catalogue does not cover the corrected nonempty DWORD/raw
byte layout. Later acceptance must cover nonempty localized RW rows, both TMS
failure envelopes, event operator bytes, stale owner/release mismatch and
publication encoding. No test was executed. The operator list/whisper path and
native Map chat/TMS consumers remain open; a World catalogue and World packet
change alone cannot prove their client end-to-end path. Full World bootstrap,
all original functional producers and the complete implementation gate remain
incomplete.
