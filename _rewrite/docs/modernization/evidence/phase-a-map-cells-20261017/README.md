# Source Map cells — Phase A, unverified

Baseline: local `main`, `39b94e1`. Cell catalog loading, cell construction,
pre-actor normal-monster membership and the bounded routing consumers below
are **INTEGRATED_UNVERIFIED**. Full actor visibility, host selection, timed AI
execution and the full server remain **IMPLEMENTING**. This is not the
`IMPLEMENTATION_COMPLETE_UNVERIFIED` gate.081/tool/grants are written only;
044–081 remain unapplied, next082. No build, test, SQL, publisher, import,
service, client, container, push or deployment was run.

## Authority and actual inputs

Original Map and corresponding client source lineage remains
`c77f79017d7940c1500ba36a5dbf8bda6fefb67c`. Source TVERSION0x2918 and the Git
version label do not establish a matching executable/assets/region profile.
Ordinary non-BOW Map is the bounded source profile; original-client compatibility
is **UNVERIFIED**. Reference trees, attributions and existing migrations are unchanged.

Actual backups were rehashed in this increment:

| Archive | SHA256 |
|---|---|
| TGAME_RAGEZONE.bak | `2cff1635d523322d3de8ddafaeacc3335581dc338dca901239ac80288f51d259` |
| TGLOBAL_RAGEZONE.bak | `fca545bcb90bcc72aa4f3514a6177c2b88d2b0bf71bf847b0128f090a20877fb` |

No private row extracts or original query-order captures were consumed. Public
schema/profile files establish contracts, not row contents or publication
success. The private complete source manifest and `CTBLChannelList` result-order
capture remain required inputs for later publication. Pre-existing local audits
and private extracts stay outside Git.

## Traceability and concrete behavior

| Original source and symbol | Modern implementation / actual consumer |
|---|---|
| `DBAccess.h3–78::CTBLChannelList/ServerChart/ChannelChart`; `TMapSvr.cpp2499–2575::LoadData` | `PostgreSQLMapService::LoadNativeCellCatalog`; existing `ReadNativeMapTopology`; constructor transaction before allocation/listening |
| `DBAccess.h5054::CSPGetServerID`; `_rewrite/docs/schema/procs/TGAME_RAGEZONE/TGetServerID.sql` | `route_compat.source_cell_owners`; `NativeCellCatalog::Server`; construction and `CellOwner` |
| `_rewrite/docs/schema/TGAME_RAGEZONE.views.sql81–84::TCHANNEL` | Explicit `TGLOBAL_GSP=TGLOBAL_RAGEZONE` publication alias and full global `source_cell_channels` projection |
| `TChannel.cpp::InitChannel`; `TMap.cpp265–423::InitMap/InitCell/InitExtCell`; `TCell.cpp44–52::IsMainCell/IsEnable` | `InitializeNativeCells`, `BuildNativeMapCells`, `NativeMapCell::IsMain/IsEnabled`; actual owned physical maps |
| `TMap.cpp973–1020::EnterMAP(CTMonster*)`; `TCell.cpp117–157,175–204::EnterMonster/AddMonster` | `EnterNativeBootstrapMonster` via actual Regen callback; owned `NativeMonsterMap::cells` and monster-ID sets |
| `TMap.cpp1117–1149::LeaveMAP(CTMonster*)`; `TCell.cpp::DelMonster/LeaveMonster`; `TAICmdRemove.cpp::ExecAI` | `LeaveNativeBootstrapMonster` in actual default-spawn Remove execution before slot/vector erasure |
| `TMapSvr.cpp12125::IsMainCell`; `SSHandler.cpp1300::OnMW_CHECKMAIN_REQ` | `NativeMainCellAt`, `FindNativeCell`, `OwnsCell`; existing World CHECKMAIN handler and its account/session fencing |
| `TMap.cpp1177::OnMove` (connection-selection block) | `NativeCellAt`, `NeighborServersLocked`; existing movement/entry/replica authorization consumers |

Construction preserves physical-channel then WORD-map order, WORD unit order,
all primary cells before extensions,16×16 cells per unit and the original eight
direction order. Neighbor lookup uses the own unit when either component would
be negative; positive overflow wraps each BYTE. A missing procedure row retains
the current server for a neighbor and leaves same-unit enable false. A successful
server0 result remains a present result, distinct from a missing row.

Edge checks retain neighbor-server entries only at the relevant unit edge.
Extension IDs wrap negative cell components to WORD; extensions enable the
logical channel and mark it extended, including already existing primary cells.
The source loop's mutations of enabled cells remain visible during later
extension processing. Registration physical channels and cell logical indexes
are distinct. `MAX_CHANNEL` is **51**, not the registration packet's BYTE limit.
Original channel result order and duplicate channel values across groups remain
in the catalog; no world filter or inferred list replaces `CTBLChannelList`.

The two position operations are deliberately separate. `NativeCellAt` implements
`WORD(position)/64` from EnterMAP/LeaveMAP/OnMove. `NativeMainCellAt` implements
`WORD(position/64)` from module IsMainCell. Their valid ranges differ. No native
object layout, unchecked floating-to-WORD conversion or implicit integer wrap
substitutes for either operation.

Normal-monster EnterMAP inserts its actual ID into the existing cell even if the
logical channel is disabled; a missing cell still assigns map membership.
Bootstrap then emits AT_ENTER exactly once. No player has been admitted during
this synchronous constructor scope; the original neighbor `EnterMonster` and
normal-monster `LeaveMonster` therefore have zero PC recipients. No empty packet
or invented ADDMON descriptor is sent. Remove now erases actual cell membership
before erasing the default-spawn vector and slot; the source wrapper and stale
leader ID remain. Dynamic cleanup, actor output and full lifetime remain open.
`InitMonster` already assigns its owning map before Initialize (`TMap.cpp570,623`),
so Remove before EnterMAP is a valid source branch. `entered_map` records the
publication boundary, not existence of that map association; normal-monster
LeaveMAP does not clear the owning-map association.

All cell/catalog structures are owned by the existing PostgreSQL Map service.
The bootstrap executor is stack-bound before TCP setup; no callback outlives its
catalog or borrows asynchronous buffers. Routing reads the immutable graph after
startup, under existing account/claim/owner transactions. OwnsCell consumes main
versus extended status; movement/entry/replica connection sets consume the cell's
actual neighbor-server entries, deduplicated in source BYTE-map order.
Movement's CHECKCONNECT set includes self in that same sorted order; a valid
ordinary-map move into an absent/disabled/extended cell emits the source empty
set rather than an invented main owner or a successful nonempty set. An invalid
or unauthorized call remains distinct and does not produce that ACK. CellOwner
uses the exact global procedure join, with no previously inferred world filter,
and pins the same publication for its existing transfer/revival/combat callers.
This does not certify those broader gameplay contracts.
Movement packet-field sequencing and the complete recipient visibility flow
remain outside this connection-set slice and require separate source acceptance.

Module CHECKMAIN can inspect an instance template as the original direct map
lookup does. Movement still refuses an instance template: native party-instance
creation/lifetime is missing. No party0 live map is invented.

## PostgreSQL and intentional safety boundaries

081 adds `map_cell_catalog/sources/channel_order` and read-only release/channel/
ownership views. Publication is tied to the exact079 full run, manifest and
allocation order, plus the active routing manifest. Five source receipts cover
four routing tables and global TCHANNEL. All four active routing tables must
match their full-snapshot values and original text bytes. Counts include genuine
empty tables; missing publication is not an empty catalog.

`tools/database/import_map_cells.py` is an unexecuted offline atomic publisher.
It rechecks the complete pinned backup manifest, actual archived/active values,
bytes and global TCHANNEL archival row identities before publication. It requires
an explicit alias and a private order file with exactly
`source_manifest_sha256`, integer `server_id`, and `channel_query` containing
every original global TCHANNEL source ordinal once. Source order is independent
of archival order; no capture is fabricated. Unknown COMMIT outcomes are not
retried. The grants file gives Map SELECT only on the three needed views.

Deliberate safety differences, not new gameplay rules:

- Source `TOP 1` without ORDER BY is refused if a needed tuple has distinct
  owners, including cross-group conflicts. Equal duplicate owners are safe.
- Real PostgreSQL/read errors stop construction/operation; they do not become
  missing procedure rows or successful default ownership.
- Source channel values≥51 would index outside the arrays and refuse publication.
  The standalone IsMain/IsEnabled helper retains the internal51 sentinel, but
  client routing refuses it before indexing the neighbor array.
- Non-finite or out-of-range WORD conversions refuse. Existing native client
  negative-position rejection remains. Defined BYTE/WORD integer wraps above
  are explicit; undefined floating conversion is not emulated.
- The complete cell graph has a262144-cell process limit, checked before every
  extension allocation; raw joined owner rows have a1000000-row limit. Exceeding
  either rejects the graph without truncating it. These are documented resource
  limits requiring later measured acceptance.
- Event iteration stops if Remove erased its slot, avoiding subsequent access
  through the source's deleted object. It does not invent a missing map pointer.

Required delayed AI publication still throws at its actual producer. If all
direct initialization completes with a surviving entered monster, the
constructor now stops at the explicit admitted-actor visibility/ADDMON/client
host boundary before listening. This barrier prevents publication through the
prototype monster registry. It is not completion of visibility or gameplay.

## Deferred verification scenarios — not executed

1. Against original query captures, compare all channel rows including repeated
   BYTE values from different groups. Missing alias/order/full manifest and
   changed full/active routing values/bytes must fail atomically.
2. Compare all256 cells of a unit for each source logical channel: own/missing/
   foreign same-unit owner; all eight edge masks; missing neighbor; actual0/255
   owner; equal duplicate rows; conflicting cross-group TOP1 owners.
3. Compare two adjacent owned units and foreign edges across several logical
   channels: primary-to-extension marking, already existing primary cells,
   negative WORD extension IDs, own-unit negative fallback and255+1 BYTE wrap.
4. Compare constructor order across multiple maps and physical channels,
   including disabled cells, empty TCHANNEL and instance templates. Exercise
   exact resource-limit boundaries; no partial graph may reach listening.
5. Compare valid coordinates immediately below/on64 and1024 boundaries, signed
   zero, fractional positions and the two different conversion orders; refuse
   NaN/infinity/undefined raw-WORD positions. CHECKMAIN and movement must use
   their respective arithmetic, rather than a common inferred unit lookup.
6. Drive actual source Initialize→Regen→EnterMAP→AT_ENTER→Remove chains without
   actors. Compare cell ID sets, event count/order, missing-cell assignment,
   vector deletion and retained leader ID. Test self-removal during event
   iteration and avoid use-after-erase.
7. With the future published catalog, compare CHECKMAIN, native entry replica
   grants and movement connection sets against original main/extension cells;
   stale owner/session/release must fail without output or partial durable writes.
8. Prove that a delayed command and a surviving entered monster each hit the
   stated pre-listen barrier. Actor admission, complete ADDMON and host lifecycle
   require their own later acceptance; historical test results do not cover this.

## Exact implementation handoff

Static inspection performed in this increment: original call/arithmetic/order
comparison, constructor and routing call-site tracing, ownership/erase review,
written SQL/view/grant inspection, Python AST parsing without importing or
executing the publisher, unique inventory IDs/CSV field counts (290×13 and73×6),
two CMake source-path registrations and `git diff --check`. No C++ compiler,
test runner, SQL parser connected to a database or runtime path was executed.

Next: implement the original DoAICMD→SM_AICMD_REQ→OnTimer→SM_AICMD_ACK queue in
the existing Map service/Asio lifecycle, with owned bounded request/timer/batch
queues, source DWORD elapsed arithmetic, separate delivery, physical map/party
resolution, stable binding identity, host-key **and object-generation** fences,
cancellation and source CanRun/Exec/repeat ordering. Do not copy the source
OnTimer invalidated-vector-iterator bug or run the bootstrap's fixed clock0 and
empty-host assumptions after actors exist.

Then port actual player cell entry/exit/visibility, full source ADDMON fields,
CanHost/NotifyHost and the host request consumers, together with the concrete
awake/dead commands they produce. Replace the admitted-actor barrier only with
those real effects. Keep full monster lifecycle and full Phase A IMPLEMENTING.
No new SQL is needed merely to persist source RAM timers; next free migration082
is available if a documented additional database contract requires it.
