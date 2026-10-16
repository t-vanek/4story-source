# Source Map monster allocation — Phase A, unverified

Baseline: local `main`, `0f37aef`. Original sources/headers/attributions, backups,
applied migrations001–043, private extracts and pre-existing local audit output
are unchanged.079, publisher and grants are **written, not executed**.
044–079 remain unapplied; next free migration080. Full Phase A and the monster
lifecycle remain **IMPLEMENTING**, not IMPLEMENTATION_COMPLETE_UNVERIFIED.

**Operational limitation:** native startup now stops at the first source
`CTMonster::Initialize` call whose AI consumer is unported. The exception names
the map, physical channel and actual source instance ID. The listener is not
started and the constructor transaction rolls back. This explicit refusal is
not completed monster gameplay. The previous round-robin prototype is no longer
allowed to populate native PostgreSQL worlds. Prior battle-reader implementation
claims do not imply that a populated native world can currently finish startup.

## Source authority

Original Map and client lineage was checked with local Git:
`c77f79017d7940c1500ba36a5dbf8bda6fefb67c`. The commit's version label and source
TVERSION0x2918 do not prove an executable/assets/region profile. SKYGARDEN is
defined in the original Map build. Original-client compatibility remains
**UNVERIFIED**; the client was neither edited nor run.

Both actual root archives were hashed again during this increment:

| Archive | Actual SHA256 |
|---|---|
| TGAME_RAGEZONE.bak | `2cff1635d523322d3de8ddafaeacc3335581dc338dca901239ac80288f51d259` |
| TGLOBAL_RAGEZONE.bak | `fca545bcb90bcc72aa4f3514a6177c2b88d2b0bf71bf847b0128f090a20877fb` |

| Unchanged reference file | Actual SHA256 |
|---|---|
| Server/TMapSvr/DBAccess.h | `364fcadbbb78e49f9e7e3e6cddb0f8623eb9315ed2a9195bb3e8103c24277d9d` |
| Server/TMapSvr/TMapSvr.cpp | `1b490618105e0f5ca338852d1cd7c007c01301641b381c3b391f857e4973dd7c` |
| Server/TMapSvr/TMap.cpp | `a874b52f3684cdb2a3a6e66bf2ce8a03dbd46087512ecd3c4c3c10d9dafd8008` |
| Server/TMapSvr/TMonster.cpp | `3aa7df2db0bafb5fbddfb7e8f6749d68e52f6fbad9887bcadd7fa0bb52a5f14d` |
| Server/TMapSvr/TAICommand.cpp | `f1ec94018b27d0a90c12fa039a32fcd14fab4c895a16d6b9166fcbe8dd4d226c` |
| Server/TMapSvr/TAICmdRegen.cpp | `8fe1aa3492bcc34daccf93000b94cf1e4967d0ef10ec5f5d7ca4c8b29e41a965` |
| Server/TMapSvr/TAICmdSetHost.cpp | `ae44f3cbf57f0030b5bb83b88aeb5445bb8178d4f55ff5c649f0e6ae5f80ef89` |
| Client/TClient/CSHandler.cpp | `51e3b85b5cd2d5af27be15f564ff45c875a02c2b70bed326648ef80484cd8ce3` |

Private archival rows and original result-order captures are unavailable in
the working tree. Existing public profile counts are navigation, not recreated
content or evidence that the new publisher succeeded. No source rows, query
orders, hashes or client behavior were invented.

## Implemented contracts and actual integration

`DBAccess.h::CTBLMonSpawn` and `TMapSvr.cpp2989` select19 spawn fields using
independent server-only map and unit IN sets.079's source selection and the
publisher preserve FLOAT division followed by truncating signed SMALLINT casts;
they do not correlate map/unit rows or substitute physical/logical channel0/1.
The service loads source WORD/DWORD bits explicitly, refuses NULL/range/nonfinite
coordinates, retains all event/local/region/delay/party fields and reserves the
source extra-ID interval0x8000..0xFFFE minus locally loaded spawn IDs.

The original query's load order builds each nonzero group vector. The final
member is the initial ETERNAL spawn; earlier group members are WAITSPAWN.
`CTBLMapMon` order is separately retained for every owned spawn, including a
real zero-row query. `CTBLMonParty` is also executed **per spawn**, even when
several spawns share a party ID. Each such invocation has its own order receipt
and includes matching rows from the full global spawn chart, including templates
absent from the local server's filtered catalog. No inferred local-only party
or common order for repeated SQL queries is introduced.

`TMapSvr.cpp3463`, `TChannel.cpp::AddMonSpawn` and
`TMap.cpp::AddMonSpawn/InitMonster` map to
`native_monster_spawn.cpp::AddNativeDefaultSpawns/InitMonster`:

- Iterate source spawn WORD IDs, then physical channel BYTE IDs. Only DEFAULT0
  events create boot wrappers; other event templates remain present in the
  catalog. Missing physical map skips that channel. Instance-map templates get
  wrappers but no slots; duplicate wrapper insertion returns unchanged.
- A missing/empty candidate vector does not produce any slots. `bCount0` means
  zero ordinary slots. Ordinary slots initially have **no monster template or
  attribute**, rather than a made-up template0 or round-robin candidate.
- Instance IDs are exactly `(spawnID << 16) | (physicalChannel << 8) | index`.
  Region/country/channel and DISAPPEAR0 state follow the original allocation.
  The source default inventory3 exists independently of item-template presence;
  the latter is recorded separately. Slots are not live registry entries.
- Essential candidates are additional slots in original query order. A missing
  monster template skips without consuming an index. Attributes use the raw
  **attribute family plus level**, not006's projected monster-ID attribute view.
  Missing attributes return from the entire InitMonster while retaining earlier
  slots; the rejected template ID is retained for diagnosis. There is no per-slot
  continue masquerading as this source branch.
- Existing class/race/level template references remain optional; missing and
  present ID0 differ. Existing skill templates are deduplicated as in the source
  map, with level1. Each essential leader assignment occurs before Initialize;
  the last executed leader assignment wins.
- Every ETERNAL insertion invokes its synchronous Initialize consumer immediately,
  before the next slot exists. WAITSPAWN does not initialize. The callback cannot
  be replaced with an end-of-batch list: group Regen can redirect to the last
  slot of a previously created wrapper, whose contents change during this loop.

Actual startup chain in `PostgreSQLMapService` is the existing owner-fenced,
repeatable-read transaction →079 catalog→078 battle/map environments→source
allocation. The service owns both catalog and allocation objects; no raw legacy
pointers, detached tasks, buffers or additional server architecture are added.
The two new C++ units are registered in `tmapsvr_asio_core`. Battle boss validity
and zone spawn vectors now consume this same native catalog. Source templates
remain distinct from successfully initialized monsters. Battle readers also
recheck the pinned079 release/order SHA before consuming cached spawn bindings.

The unresolved initializer throws `NativeMonsterInitializationRequired` before
listening. That specific diagnosis survives constructor validation wrapping.
No initialized/visible state, accepted event, generic success, fake HP or packet
is produced. `main.cpp` runs `SpawnAllStatic` only without native ownership;
the existing prototype AI is already similarly excluded. Prototype comments
now state its mismatched zero-HP and missing-attribute policies explicitly.

## PostgreSQL publication

079 adds `monster_compat`, a world/server-scoped publication, seven source
receipts and separate spawn/candidate/party result-order tables. Views expose
immutable full-run source data. Counts and hashes match verified archival
checkpoints; six spawn/monster/attribute/candidate/class/race tables must also
equal the active content release, and levels must equal the character release.
The publication is bound to the same078 full source,
routing, character and content releases.078 already checks actual item/skill
values/bytes against that full source. Raw source family/level attribute keys
and class/race/level existence are retained. Runtime completeness counts are
computed independently from source rows, including repeated party queries.

`tools/database/import_map_monsters.py` requires the complete verified pinned
historical manifest, rechecks actual full and active values/ANSI-byte hashes and
contiguous exact archival row ordinals, acquires offline Map/World barriers and
stable locks, validates query-result membership, and atomically publishes all
orders and receipts. Duplicate identities/order keys or replacement of an
existing publication refuse. No unknown COMMIT is retried. No historical data
or operational player/monster state is rewritten or synthesized.

Its private `--source-order` JSON has exactly these members:

- `source_manifest_sha256`: the complete historical manifest's canonical hash.
- `server_id`: the actual original server BYTE, equal to the CLI argument.
- `spawn_query`: every archival ordinal selected by that server's CTBLMonSpawn,
  once each, in the original result order.
- `map_mon_queries`: decimal unsigned spawn-ID keys for every selected spawn;
  each value is the complete CTBLMapMon result as TMAPMONCHART archival ordinals.
- `party_queries`: decimal unsigned **owning spawn-ID** keys for every selected
  spawn with nonzero party; values are that invocation's CTBLMonParty result as
  full TMONSPAWNCHART archival ordinals. These are not party-ID keys.

An empty array is valid only when the corresponding actual query returned no
rows. Supplying a permutation does not prove it was observed in the original
server. Captured source query-order evidence remains required. The input file
and extracts stay outside Git. `deploy/sql/map-monster-runtime-grants.sql` grants
only the required source-view reads; it has not run.

## Deliberate differences and limits

Source BYTE slot-index overflow aliases existing IDs; native refuses before
inserting index256 rather than copying the collision/leak. Duplicate source
keys and invalid casts also refuse instead of selecting an unspecified row or
copying undefined memory access. No ordinary game arithmetic or selection rule
was replaced. Positive/zero HP/MP calculation belongs to Regen, which is still
missing; the original max-HP/max-MP minimum1 must be preserved there, not the
prototype's zero-HP quarantine.

This increment does **not** implement TAICHART/TAICMDCHART/TAICONCHART loading,
CTMonster::Initialize/OnEvent, actual Regen, cell registration, host selection,
movement/combat, death/respawn/loot or dynamic/quest/local/instance realization.
Nor does it complete switches, GodBall, battle enable effects, occupation/rewards,
native guild/hero mutation, original admission/full World bootstrap, or the wider
server inventory. The initializer refusal is an implementation dependency,
not unavailable historical data being replaced with a synthetic world.

## Static review and deferred verification

Only source/API/metadata/SQL-text/CMake-entry/diff inspection, Python AST parsing,
CSV/JSON metadata parsing and actual file hashing were performed. **No build,
test, SQL, migration, publisher/import, service, client, container or runtime
verification ran.** No push/deployment. Historical results are not evidence for
the new code. Implementation and verification are recorded separately in the
existing unified CSV inventory and database map.

Phase B scenarios, prepared but not executed:

1. Reconcile real source query-order captures with every per-server/per-spawn
   receipt; include identical party IDs with different invocation orders,
   members outside the local projection, zero-row candidates and malformed,
   partial, duplicate-key or wrong-run profiles. Verify atomic rollback and
   unknown-COMMIT handling using isolated copies, never archive mutation.
2. Compare server-only independent filters, negative/fractional boundary casts,
   physical channel ordering, DEFAULT versus other event types, missing maps,
   fresh instance templates, group last-member order and extra-ID boundaries.
3. Compare ordinary0/positive counts, essential-only spawns, missing template
   without index consumption, missing family/level attributes retaining earlier
   slots and skipping later ones, actual ID0 references, duplicate/absent skills,
   leader overwrites and the index255/256 refusal.
4. Observe synchronous callback boundaries, including visible earlier wrappers,
   leader assignment and current slot/vector size. Verify that the real native
   constructor stops before the listener at the first unresolved Initialize,
   identifies the actual object and never runs the round-robin prototype.
5. After the AI port, extend to original queued/delayed events, initial Regen
   ordering, actual actor/cell/host visibility, packets, disconnect/restart and
   full source-client acceptance. Do not infer these from allocation checks.

## Exact next implementation dependency

Port `TMapSvr.cpp2405–2481` AI command/condition/binding producers from the full
original tables with separate original query-order evidence. Extend the current
allocation-only monster projection from `TMapSvr.cpp2798` with the actual AI,
flags, size and full stat/skill/path producers needed by those commands; mere
attribute/template existence is not a hydrated AI/combat object. Then replace the
**throwing initializer** only with actual `TMonster.cpp::Initialize`: own AI or
DEFAULT_AI1, AT_DELETE6/trigger0, ordered CanRun+Exec directly, ignoring delay and
the returned repeat flag on this path. Preserve the source AN_CHGHOST condition's
actual missing-return behavior; do not silently replace it with intended logic.

Implement its entire dependent `TAICmdRegen` flow, including BYTE narrowing
after group selection, redirection without a second CanRun, probability and
weighted nonessential selection, FLOAT radial placement, actual family stats,
leader/occupation/skills/host resets and EnterMAP→AT_ENTER. Carry this through
`DoAICMD`, the timer request/ACK queue, host-key fencing and the concrete source
command graph. `TCell::EnterPlayer/EnterMonster`, SetHost/CanHost and source host
packets are producers/consumers, not a replacement generic server AI tick.
Only then can initialized monsters be published to the actual native registry
and the guard be removed. Keep full monster lifecycle **IMPLEMENTING** until
those paths exist. The next free migration is080; do not execute it in Phase A.
