# Native monster host reset and relocation — Phase A, unverified

Baseline `f006388`, local `main`, 2026-10-17. The existing native executor now
connects ResetHost, ChgMode output, the shared AI request/batch path and normal
monster cell relocation. Source global actor lookup and teardown retirement use
the existing session registry. These connections are **INTEGRATED_UNVERIFIED**
within actor isolation. Populated recipient/relocation branches and battle-mode
clock/output calculations are **IMPLEMENTED_UNVERIFIED**. All latched admission
and actual-player Initialize/ACK guards remain. Full hosting and Phase A remain
**IMPLEMENTING**; this increment does not establish populated gameplay support.

No build, test, SQL, import, migration, service, client, container, runtime check,
push or deployment ran. Only source/interface/CMake/diff/inventory inspection and
read-only backup hashing were performed. No migration was added:044–082 remain
written/unapplied, next083. Private extracts and pre-existing local
`_rewrite/docs/audits/` remain outside Git.

## Authority and database boundaries

The inspected original server/client source revision is
`c77f79017d7940c1500ba36a5dbf8bda6fefb67c`. Reference trees, attributions and
backups were not changed. TVERSION0x2918 is a source marker; matching executable,
assets, region and actual original-client compatibility remain **UNVERIFIED**.
The ported branch is ordinary non-BOW/non-BR OT_MON; recall/companion movement
and real party-instance maps are not silently treated as ordinary monsters.

Actual local archives were rehashed without restoring or interpreting their data:

| Archive | SHA256 |
|---|---|
| TGAME_RAGEZONE.bak | `2cff1635d523322d3de8ddafaeacc3335581dc338dca901239ac80288f51d259` |
| TGLOBAL_RAGEZONE.bak | `fca545bcb90bcc72aa4f3514a6177c2b88d2b0bf71bf847b0128f090a20877fb` |

Reset/host/cell state is source process-local RAM, with no source database write.
The ACK consumes the current actual spawn center/direction from079, AI bindings
from080, channels/cells from081 and existing082 skill inputs where subsequent
events need them. Existing PostgreSQL owner and release checks fence pending AI
work; no host-row persistence or SQL Server dependency was introduced. Complete
private historical manifests and original query-order receipts required for
079–081 remain unavailable in the checked publication paths. Nothing was seeded
to replace those inputs. Historical `legacy_game`/`legacy_global` remain intact.

## Source to modern implementation

| Original source and symbol | Modern symbol and actual connection |
|---|---|
| `TMonster.cpp2392::ResetHost` | `native_monster_host.cpp::NativeMonsterAiExecutor::ResetHost`; actual Regen and no-candidate SetHost/CHKHOST invoke it |
| `TObjBase.cpp295::ChgMode`; `TMonster.cpp54::GetColor` | `ChangeMode`; minimal `ProjectNativeMonsterColor/NativeMonsterColor`; source-ordered mode/color notices use the existing bounded outbox |
| `TMapSvr.cpp6370::FindChar`; accepted CONNECT and close membership | `InMemorySessionRegistry::FindActor/RetireActor`; `main.cpp` binds actual lookup once; `MapServer::HandleConnection` retires after cell departure and before DB awaits |
| `SSSender.cpp133::SendSM_RESETHOST_REQ`; `SSHandler.cpp439::OnSM_RESETHOST_REQ` | tagged `NativeAiInvocation::ResetHost`, existing `NativeMonsterAiQueue::Publish/Turn/TakeAcknowledgement`; direct request-to-batch forwarding without a timer |
| `SSHandler.cpp464::OnSM_RESETHOST_ACK`; `TMap.cpp::FindMonster` | existing `DispatchNativeMonsterAi` resolves physical map; `ResetHostAcknowledgement` checks lifetime/template/status/current host, consumes current spawn and invokes the required movement callback |
| `TMap.cpp1350::OnMove(CTMonster*)`; `TCell.cpp::EnterMonster/LeaveMonster/LeaveChannel` | `native_monster_cells.cpp::MoveNativeMonster`; shared real cells, captured source ADD/DEL, one subsequent executor AT_LEAVE, then reset MONMOVE |
| `CSSender.cpp::SendCS_CHGMODE_ACK/SendCS_CHANGECOLOR_ACK/SendCS_MONMOVE_ACK`; `Client/TClient/CSHandler.cpp1665/1952/7106` | existing `PublishNativeCellNotices` explicitly serializes captured mode6/color7/move26-byte bodies and uses owned Asio PostPacket/drain |

The two new translation units are registered once in the existing Map CMake
target. Enter/Leave monster implementations were moved from `native_map_cells.cpp`
into `native_monster_cells.cpp`; no second implementation or registry was added.

## Preserved order, state and output

Reset resolves the previous host through the global accepted character registry
before mutation. Lookup has no Ready, primary, map or channel predicate. Accepted
pre-Ready and other-map actors are distinct from an absent actor. The native
admission domain excludes character ID0; its no-host lookup is absent even during
boot before callback binding. A nonzero lookup without the actual callback fails.

The source action becomes TA_STAND0, then ChgMode assigns MT_NORMAL0 and the
current DWORD last-attack tick. Each reverse-neighbor recipient gets mode followed
immediately by color, even when the mode was already normal. The minimal color
capture preserves source GetColor branching without requiring ADDMON maxima or
effect projection. Missing own-AI presence is retained for the actual dereference
branch, with no DEFAULT_AI substitution. Battle-mode helper calculation writes
both recovery ticks as wrapping DWORD tick+5000. The separate AC_CHGMODE command
CanRun/transition remains unported; its support is not inferred from this helper.

Only after those captures does Reset clear host/target/type/helper/help-count and
the owned aggro, signed64 damage and DWORD guild-damage maps. It preserves host
key, keeper, status, next skill, bars, stored keys and start/next coordinates.
Adding those owned maps and their clearing does not implement missing combat and
aggro accumulation producers. If the global previous host exists, NotifyHost
captures its FALSE first, then reverse current neighbors, excluding that ID.
Only this previous-global-host notice admits the accepted pre-Ready/other-map
recipient scope; the publisher still fences key/connection/role/epoch and socket.

The reset request is emitted only with that previous host and source FLOAT
sqrt(start-current) strictly greater than32. It shares the existing bounded FIFO,
timer-before-one-request processing, separate batch dispatch, cancellation and
retirement. It has no artificial delay, binding or host-key fence. The normal
command path still requires its actual binding and positive delay.

ACK drops a missing/stale lifetime, missing template, OS_DISAPPEAR0 or currently
rehosted monster. Ordinary maps ignore party in lookup; unsupported instance
templates remain excluded. Current spawn center supplies start/next XYZ and
current Y, WORD direction, pathID0 and pathDirection1. Cell movement supplies X/Z.
After movement output, exactly one AT_LEAVE with trigger0/zero arguments occurs
only for a changed cell. Final MONMOVE uses actual post-event XYZ/pitch/direction
and forced wire keys4/4/action0 without overwriting stored keys/action.

Normal monster OnMove preserves source WORD-before-/64 coordinates and sorted
z/x neighbor traversal. Same-cell movement changes only X/Z. Old membership and
old main-channel collection require an enabled old origin; old-only neighbors
get DEL before position changes. Enabled new origin computes source BYTE-prefix
enter/lost channels, adds membership, and emits lost-channel DEL followed by
new-channel/new-neighbor ADD(newMember FALSE). Normal OT_MON never uses the
recall/companion whole-channel leave branch. The changed-cell AT_LEAVE still runs
with a missing or disabled destination. EnterMAP keeps its distinct source rule:
ordinary monster insertion ignores channel enable.

All notice payloads own captured state and lifetime identity across subsequent
events/removal. The existing65536-record/64MiB limit now accounts for mode/color/
move bodies as well. Output is captured inside the existing AI lock and drained
on the same IO path without suspension between ACK/application and publication.
Explicit wire serialization retains field widths, order and existing framing.

## Intentional safety deviations and scope limits

Queued reset work carries a native object generation and is cancelled on Remove
or authority/shutdown retirement. Source reset ACK has no host-key check and that
rule is preserved; generation fencing prevents an old DWORD ID addressing a new
object. After reentrant AT_LEAVE, ACK re-finds the exact generation before final
output, avoiding the source dangling-pointer access if an event removed it.

Source movement from a disabled old origin can leave its cell index intact. The
modern path preserves that observable index while the monster lives, records
every owned cell membership, then removes all such indexes on Leave/Remove.
This cleanup deliberately prevents a dangling reference/ID-reuse alias after
destruction; it does not silently remove the live source remnant during movement.
Invalid/nonfinite geometry refuses rather than reproducing undefined casts.

Source global actor membership ends after world/cell departure. Native failed
save/release reservations must survive asynchronous teardown, so the existing
registry now records actor retirement separately before the first DB wait.
Failed persistence retains the reservation/snapshot but cannot keep it eligible
as a global monster host. Existing accepted-history/service statistics were not
redefined by this actor lookup. No extra distributed component was added.

Populated public execution remains barred. PC monster Enter/Leave/channel and
MoveGhost events, LeaveAggro and live aggro/target producers, host-produced
CS_MONMOVE_REQ/attack/skill Use, remaining awake/dead commands, non-PC targets,
effects/transform, non-fresh death/loot and other dynamic lifetimes remain open.
No source CS_MONHOST_REQ exists in the inspected profile. The new reset recipient
and relocation code is structurally connected but cannot establish playable
hosting while those producer/consumer dependencies and barriers remain.

## Deferred Phase B scenarios and exact handoff

Future verification must cover global previous-host lookup for pre-Ready, replica,
other-map and Closing actors; retirement with failed save/release; absent host
versus host with zero data; unchanged mode output; per-recipient mode/color/host
order; minimal color with maxima unavailable; battle tick wrap; exact32 and >32
reset distances; FIFO reset interleaving with delayed commands; first/same-tick
reset forwarding; current rehost drops and host-key-only changes; generation
reuse/cancellation; current spawn center versus randomized old start; same-cell,
missing/disabled cell, logical channel changes and BYTE-prefix cases; disabled-old
index retirement; one AT_LEAVE and reentrant Remove; captured MONMOVE wire keys
versus stored state; complete packet decoding and bounded output failure.
No scenario in this paragraph was executed or promoted from historical results.

Next implement `TMonster.cpp::LeaveAggro` with actual aggro/target producers and
PC Enter/Leave/channel/MoveGhost event transitions, including their inventory
lock-release dependencies. Reuse this reset/mode/shared-queue/monster-cell path;
then implement host movement/attack/skill and remaining awake/dead commands.
Keep admission and actual-player AI guards until coexistence is connected.
Inventory now has322 unique IDs/13 columns; DB map75 rows/6 columns. These counts
are navigation metadata, not completion percentages. Next migration083; Phase
B/C remain deferred and full Phase A is still incomplete.
