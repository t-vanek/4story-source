# Native Map source topology and ownership — Phase A

Baseline: local `main`, `99d029f`. Only `_rewrite/docs/audits/` was untracked;
it remains untouched and outside Git. No applicable AGENTS.md was found.
Source trees, attributions, backups and migrations001–075 are unchanged.
New076/grants and the modified offline routing publisher are **not executed**.
Recorded044–076 remain unapplied; next077. Full Phase A is incomplete.

## Source contract and exact limits

Original Map/World source lineage was inspected in Git:
`c77f79017d7940c1500ba36a5dbf8bda6fefb67c`. The unchanged client source marker
TVERSION0x2918 does not identify a runnable executable/data/region profile.
Original-client acceptance remains blocked on that missing environment.
Read-only SHA256 was repeated on the actual files:

| Backup | SHA256 |
|---|---|
| TGAME_RAGEZONE.bak | `2cff1635d523322d3de8ddafaeacc3335581dc338dca901239ac80288f51d259` |
| TGLOBAL_RAGEZONE.bak | `fca545bcb90bcc72aa4f3514a6177c2b88d2b0bf71bf847b0128f090a20877fb` |

- `Server/TMapSvr/DBAccess.h::CTBLServerChart` selects physical channel,
  map and unit from TSVRCHART with **server-only** filtering.
  `CTBLChannelChart` joins TCHANNELCHART/TSVRCHART by group, map, unit and
  physical channel and groups map/logical/physical/server. There is no group
  parameter in either original query.
- `Server/TMapSvr/TMapSvr.cpp2499–2573::LoadData` constructs physical channel
  objects, then inserts logical keys only when their physical object exists.
  `TMapType.h880` declares MAPTCHANNEL as `map<DWORD, CTChannel*>`.
  The actual key is MAKELONG(map WORD, WORD(logical BYTE)), ordered as DWORD.
  The separate `CTBLChannelList`/TCHANNEL global list is not supplied by this
  increment; it is not inferred from these per-server objects.
- `SSSender.cpp210::SendMW_CONNECT_ACK` writes Map WORD identity, BYTE key
  count, then **BYTE(key)** for every ordered DWORD key. It does not serialize
  the logical channel from the high word. Equal low bytes remain duplicates.
  More than255 keys wraps the count while still writing all keys: that source
  remainder/framing case remains explicitly unsupported here. No channel list
  is rewritten to hide it.
- `Server/TWorldSvr/SSHandler.cpp592::OnMW_CONNECT_ACK` rejects duplicate IDs,
  records received bytes in a set and emits no registration ACK. Its ordered
  bootstrap is LOCAL, MISSION, active-build SKYGARDEN, BOW/BR connection,
  optional real Relay notification, events, cash sales, castle applicants,
  one-time tactics expiry, month ranking and tournament info/bracket fanout.
  World/Map `stdafx.h` both define SKYGARDEN; it cannot be treated as excluded.
- `Server/TWorldSvr/TWorldType.h231::BATTLE_TYPE` is LOCAL0, CASTLE1,
  TOURNAMENT2, MISSION3, SKYGARDEN4, BOW5, BR6, COUNT7. The existing modern
  claim that the enum was absent was wrong. It incorrectly numbered mission2
  and sky3, even though the existing tournament repository already used2.

The retained historical routing proof records exact TSVRCHART join equivalence
and358 rows, while historical profiles record361 TCHANNELCHART and301 TMAPCHART
rows. These totals do not establish any particular server's key count/group
uniqueness; current private operational rows were not executed or fabricated.

## Implemented paths

`fourstory/db/native_map_topology.h` and `src/db/native_map_topology.cpp` add one
shared PostgreSQL source projection, used by both current server implementations.
It is built into existing fourstory_common, without another server/framework.
`ReadNativeMapTopology` requires076's complete verified four-table routing
release, matching expected manifest when provided, explicit operational-to-source
group mapping and an actual unique operational Map endpoint. It checks NULL,
BYTE and source signed SMALLINT→WORD bits. It executes the server-only queries,
retains ordered DWORD keys and actual physical-channel assignments, and distinguishes a
proved empty result from unavailable data. Physical duplicates collapse as in
the original object map; conflicting physical owners of one logical key fail
instead of choosing an unspecified SQL row. Logical cardinality is bounded
during insertion. No metadata, channel or default world is synthesized.

`PostgreSQLMapOwner` now calls this projection in a read-only consistent snapshot
before replacing ownership or recovering sessions. It records the manifest,
source group and canonical channel shape with its new owner token. It rechecks
after acquiring the owner row, before recovery, to cover an offline-publication
wait. `main.cpp` passes the configured routing hash and logs aggregate counts.
No old checkpoint, inventory or original historical row is backfilled.

`PostgreSQLMapService::CheckCatalogs` and `PostgreSQLMapOwner::Healthy` reject
a changed/missing release or source-group mapping. The existing transaction,
claim, uncertain-save and disconnect paths remain authoritative.

`PostgreSQLChatRepository::MapOwner`, called by the existing live World Map
registration handler, now takes a consistent World-owned snapshot, verifies
the actual Map advisory lock/token and independently recomputes the same source
projection. Manifest/group and JSONB shape must match that exact process record.
Old NULL provenance never authorizes a peer. Existing registration still stores
the validated token in PeerSession for downstream native actor/RPS/social checks.
Constructor probes require the new schema before listening. All blocking work
stays on the existing database workers; no new mutex spans an await.

`tools/database/activate_routing_catalog.py` retains the existing CLI and exact
fingerprint/value/raw-byte checks. It now verifies under stable source-table
locks, uses the shared offline Map/World owner barrier for a changed publication,
refuses incompatible graph receipts/prepared handoffs, and records/rotates old
Map tokens atomically with the routing activation. This also fences workers
whose process lost its advisory-lock backend. An unchanged verified publication
does not retire owners. Earlier schemas with no Map owner table remain usable;
an ownership schema requires076 retirement support. No import/publication ran.

`services/battle_constants.h` now follows the real source enum. Existing
`OnBattleStatusReq` dispatch therefore routes source type3 to MISSION and no
longer treats source tournament2 as a mission command. The existing tournament
loader refers to the same constant, retaining its actual type2 query. This fixes
selectors only; it does not complete the battle scheduler/side effects or add
a missing SkyGarden consumer.

076 adds three read-only source views, nullable process provenance and an offline
retirement audit. Its grants reuse existing Map/World owner privileges and add
only source-view reads plus World endpoint reads. Historical data and applied/
previously written migrations are unchanged;076 and grants are written only.

## Deliberate refusals and incomplete bootstrap

- Cross-source-group reuse of one server ID is refused. The original query is
  unfiltered, whereas native routing uses an explicit world/source-group mapping;
  silently discarding other groups would change its contract. Conflicting
  physical owners of a logical key also require exact data/topology resolution.
- Profiles with more than255 logical keys are unavailable pending source count/
  remainder/framing proof. No truncated list or guessed logical-channel list is
  passed off as compatible. These restrictions can prevent startup for an
  unresolved source profile; no compatible historical topology is assumed.
- The network remains the explicitly documented **RW-as-Map extension**.
  No source MW_CONNECT_ACK handler/ACK or partial-success bootstrap is invented.
  This increment completes the bounded source-topology prerequisite, not source
  registration. Initial state producers and their Map consumers remain missing.
- Needed next: source TBATTLETIMECHART/TCUSTOMTIMECHART loading and timer state,
  including exact mission-time initialization/unsafe source branches, native
  LOCAL/MISSION/SKYGARDEN consumers and side effects; then the remaining ordered
  guild/tactics/rank/tournament/bootstrap dependencies before changing admission.
  Current PostgreSQL World still constructs several legacy SOCI repositories;
  native topology does not remove those outstanding SQL Server contracts.
- Source operators/messages in RW ACK are not an original Map bootstrap ACK.
  Original Relay role/lifecycle and duplicate Map cleanup remain separate work.
  Broader gameplay, BR/late-replica RPS and client acceptance remain open.

## Deferred Phase B acceptance

| Scenario | Required evidence |
|---|---|
|076 upgrade/grants | Old owner rows stay NULL; no source/graph backfill; least-privilege Map and World can read all required projections. |
| Original projection | Compare actual physical and logical maps with both original queries; signed WORD bits, duplicate units/low bytes, missing physical objects and genuine empty results. |
| Unsupported topology | Missing/NULL/mismatched publication, mapping or endpoint; multiple groups, conflicting physical keys and255/256-key boundary. No partial owner/recovery writes on refusal. |
| Map claim/publication race | Publication before/during owner claim; recheck after table wait; old generation cannot run recovery/admit clients under a new mapping. |
| World registration | Current advisory token plus exact manifest/group/JSONB shape succeeds; old NULL, drift, dead owner and takeover fail without implicit topology. |
| Offline routing publication | Live Map/World rejected; stable source verification; incompatible active/recovered graphs and prepared transfers blocked; changed run records token retirement; idempotent run preserves owners. |
| Failure/unknown COMMIT | Activation/retirement/owner update/recovery rollback or explicit reconciliation; no automatic unknown-write replay and no player graph rewriting. |
| Raw battle selectors | Type2 is tournament and produces no mission broadcast; type3 uses original MISSION packet. Sky4 remains an explicit incomplete branch. |
| Full source admission | After remaining producers/consumers exist: exact Map BYTE body, duplicate handling, no ACK, all initial packets in original order, disconnect/reconnect and unmodified client evidence. Currently IMPLEMENTING. |

Only static source/interface/SQL-text/diff review and metadata parsing/hashing
are performed. No build, test, SQL, import, migration, service, client, container,
push or deployment ran. No IMPLEMENTATION_COMPLETE_UNVERIFIED claim is justified.

Structural review:242 unique capability IDs with13 fields;65 database mappings
with6 fields; both new shared files appear once in existing CMake. Migration076
is unique, next077. The modified publisher's Python AST parses without importing
or invoking project code. Source Map/World/client/protocol trees and001–075 have
no diff. Whitespace checks are clean. No compiler or SQL validator was run.
