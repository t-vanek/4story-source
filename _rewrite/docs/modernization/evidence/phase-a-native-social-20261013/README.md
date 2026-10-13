# Phase A — native World social persistence and original Map friendship relay

Code `1f1eb59` / `e709281`, based on local `374d56a`, main only. Scoped production
paths are **INTEGRATED_UNVERIFIED**; migration/import/grants are
**IMPLEMENTED_UNVERIFIED**. Phase A is incomplete, with no full PG-only runtime,
client executable identity, Phase B verification or container startup claim.

## Source contract and actual integration

`Server/TWorldSvr/SSHandler.cpp` friend handlers5769–6500 and original backup
TFriendInsert/Delete/GroupMake/Delete/Name/Change define directed edges and group
buckets. New `PostgreSQLFriendRepository`, selected by the actual World main's
native PG pool, implements reads, single erasure, atomic mutual acceptance and
all four group writes. Original existing-edge groups survive insertion. Group
SQL deliberately has no group FK/reset: the source permits orphan buckets;
World rejects ordinary deletion of an occupied group. Forward/reverse hydration
preserves FT_FRIEND/FT_FRIENDFRIEND/FT_TARGET, offline level/class, source protected
lookup direction and exact remembered group bytes. Load errors stop World;
stale claims return an explicit rejected load, never a successful empty graph.

`Server/TMapSvr/CSHandler.cpp:6961–7085,8441–8555` and
`SSHandler.cpp:10083–10405` now have modern Map dispatch/response counterparts
in `handlers/friend.cpp`. Eight original client friend/group requests reach
World with the original char/key prefix. Ten World results/presence packets
return with that prefix removed. Source self-ASK, secure-code prompts, any-row
protected checks, group name limits/rename double-response quirk and basic BOW
no-ops are retained. Native protected ADD/ERASE already produce committed
MW_PROTECTEDCHECK; World now persists its directed erase before presence/cache
publication. Chat, party, guild, deal and other protected consumers are separate
unfinished responsibilities.

The old World FRIENDLIST encoder emitted only a nonzero soulmate ID and then
immediately group counts. Original `SSSender.cpp:1723` and independent
`Client/TClient/CSHandler.cpp:9338` require the conditional name, level, class,
connected and region fields. Both World callers now supply that descriptor;
Map validates/relays the original full layout. Counts do not wrap or silently
truncate; groups/friends sort in original unsigned key order.

Original `TSoulmateReg` writes **one initiator row**, not two reciprocal rows.
`TSoulmateEnd` stamps dwTime, whereas `TSoulmateDel` deletes the exact directed
pair. The native repository implements these distinctions, target-name
TVIEW_SOULMATE joins, incoming rows and 24-hour load expiration; future stamps
remain silence. World caches distinguish own and inbound relationships.
Reg/search/end commit before cache, and online level changes stamp only each
qualifying owner. Fresh entry restores online regions in own/incoming projections.
The source TVIEW_SOULMATE incoming descriptor joins the target, while live
RegSoulmate's incoming cache describes the owner; that source discrepancy is
retained and must be covered independently in Phase B.

Fresh World entry emits original standalone MW_SOULMATE after CHARINFO/ROUTE and
before FRIENDLIST. Map now applies its World-authoritative cluster projection
and sends CS_SOULMATE_ACK only for nonzero ID. CS_SOULMATEEND_REQ retains BOW,
ProtectTutorial and absent-mate no-ops, reaches native World end persistence and
returns CS_SOULMATEEND_ACK BYTE result after Map cache publication. The silence
DWORD is server-side only. Independent client handlers12910/13110 confirm these
response fields. Map checkpoints do not write this normalized relationship.

**Incomplete soulmate scope:** initial-load level-gap CheckSoulmateEnd, complete
search/registration/preview Map NPC and money/item transaction, soulmate party XP,
all special-mode branches and recovery/durable response delivery. World's
existing search/reg handlers alone do not complete that client gameplay system.
They must not be exposed as a completed economic transaction until integrated.

## Ownership, concurrency and intentional modern differences

Migration066 introduces new operational World ownership/audit/import structures
and source-shaped native social tables. A dedicated libpq session holds the
2-int advisory `(0x34574C44, world)` for process lifetime. Worker transactions
lock the matching random-token owner row and validate its backend advisory lock;
heartbeat failure stops World without reclaim/replay. Reserved delivery and
offline bill transactions now require this same owner token. This does not fence
the remaining legacy World repositories.

Native social writes lock sorted Map owner rows, unsigned account rows and then
primary claims/current-user/characters, matching Map's process→account→claim order.
Mutual acceptance validates both actors in one transaction. Source secure erase/
group-delete rechecks enabled/unlocked_key under the same account lock, without
reading code credentials. Group and social cache validation uses a coroutine
permit across DB await/commit/projection, preserving the original serial World
batch decisions. Post-await object/key/peer/lifecycle checks protect successors.

Explicit modern differences requiring later acceptance:

- Persistence/audit commits precede cache and success; unknown COMMIT and other
  native persistence faults stop World with exit1. No automatic write replay or
  invented success/error packet. Equal valid request bytes remain new actions.
- A client REPLY requires an actual invitation bound to the inviter's login key
  and recipient object generation. Decline/protected refusal consumes it. The
  original unchecked REPLY could create another player's edge without consent.
  Pending distinct dialogs are capped at64 and return existing FRIEND_BUSY.
- Remembered protected presence is respected through relogin/presence updates;
  no unconditional live-registry lookup exposes a blocked connection. Full
  source protection semantics/other consumers still require review.
- Malformed tails, embedded NUL and oversized counts are rejected. Map World
  social responses drain an existing local gameplay operation before reading
  its committed projection; a15-second operational timeout closes the client.
  Current friend/end sends do not wait for replies while holding that operation.
- Production World requires an explicit DB and worker pool; its old no-DB fake
  guild bootstrap is removed. Existing fake adapters/tests remain available.
  Native guild/war/event bootstrap is still missing; no validator or unsupported
  repository is bypassed to manufacture a runnable cluster.

`native_world.client_text_encoding` must match the supported client's ANSI wire
profile and Map setting. Empty permits ASCII only. Non-ASCII values require
lossless PostgreSQL codec conversion; Python import codec aliases must denote
the same encoding. Neither backup collation nor source protocol version proves
an executable's encoding/build. Full historical character-name/collation import
and production profile acceptance remain required.

## Historical import and permissions — written only

`tools/database/import_social_state.py` requires the **complete** pinned archive
manifest, verified archival value/byte receipts, an exact injective reviewed
character map and explicit source-VARCHAR/wire codecs. It projects all three
historical social tables, including group0, duplicate names, orphan buckets,
nonmutual edges and signed INT dwTime bits. Missing identities, ambiguous keys,
unrepresentable bytes or existing social state/audit are explicit reconciliation
failures; no rows/identities/defaults are invented and the archive is untouched.

The tool holds all World and Map process namespaces, requires settled sessions
and commits native rows/import receipt together. Existing entitlement, cooldown,
quest, PvP, protected, mailbox and reserved-mail importers now also hold the World
namespace. Actor/quest publication's existing offline barrier additionally locks
world_runtime_owner when present and checks both process namespaces. Legacy
unfenced binaries must still be stopped; those tools do not claim to control them.

After the later isolated upgrade, apply existing World mail grants **and**
`deploy/sql/world-social-runtime-grants.sql` with the actual world_role. Social
permissions are explicit and account/Map row-lock privileges are column-scoped;
secure-code credentials/archive tables are not granted. Migration066, imports
and grants have **not** been executed.001–065, backups and original sources are
unchanged. Next unused migration is067.

## Compilation, skipped execution and exact continuation

Executed on GCC16.2.1/CMake4.3.0/Fedora44, C++20, native PostgreSQL enabled,
BUILD_TESTING=OFF, legacy Windows TNetLib disabled:

```sh
cmake --build build/phase-a-compile --target tmapsvr_asio tworldsvr_asio -j1
cmake --build build/phase-a-quest-release --target tmapsvr_asio tworldsvr_asio -j1
```

Both final commands exit0. Configuration excerpts, file/binary/backup hashes,
source references and logs are in [implementation.json](implementation.json).
Earlier failed iterations are retained: missing cached friend descriptors and
an incorrect attempt to place soulmate fields in CHARINFO were corrected to
original separate packets. Existing guild/connection/map-server warnings remain.
The only post-build action copies the OpenSSL provider; no server/validation ran.
Nine changed Python tools were AST-parsed without importing/executing them.

**Not executed:** any unit/integration/regression/protocol/PG/client tests,
sanitisers/fuzz/benchmarks, SQL migrations, historical imports/reconciliation,
server/client processes, container builds/startups/restarts. Only uncompiled
client source is available; executable hash/build/assets and actual acceptance
remain unavailable. Zero executed tests is not a passing verification matrix.

Continue Phase A with native World schema validator/main, guild/war/event/rank/
item/service repositories, all SOURCE soulmate Map costs/consumption/XP and
initial-load checks, response-loss recovery/notification retention, full social/
combat/AI/economy/quests/modes/tools. Existing source inventory still contains
unreviewed responsibilities. Do the whole static gate before Phase B/C.

Deferred Phase B scenarios: new/existing directed pairs and orphan groups;
all group failure codes/order; forged/stale/declined/blocked invitations; same
bytes repeated legitimately; nonzero soulmate descriptor and silence response;
own/incoming relationships and independent targets; group/invite races from
multiple Maps; concurrent secure/protected edits; Login/Map/World takeover during
await; row/first/second-write failure; unknown COMMIT and lost socket responses;
crash/reconnect/handoff with SQL authority; all imports/upgrade/role permissions;
ASCII/non-ASCII source byte comparisons and real unchanged client acceptance.
