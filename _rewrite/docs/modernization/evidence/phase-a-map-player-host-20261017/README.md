# Native PC movement and host eligibility — Phase A, unverified

Baseline `73a1eea`, local `main`, 2026-10-17. Source-empty ordinary PC movement,
local ghost/CanHost/observed-clock state, load/handoff inputs and speed/operator
checks are **INTEGRATED_UNVERIFIED**. Host geometry and SetHost candidate selection
are **IMPLEMENTED_UNVERIFIED**. Their populated selected-host execution, monster
event consumers and full MOVE/gameplay remain **IMPLEMENTING**. No populated
process can admit players; neither the latched barrier nor ACK actor guard was
removed. Full Phase A is incomplete.

No build, test, SQL, import, migration, service, client, container, runtime check,
push or deployment ran. Only source/interface/CMake/SQL-text/diff and inventory
metadata inspection. No new migration/schema/transfer field/grant is required.
044–081 remain written/unapplied; next free number082. Private extracts and the
pre-existing `_rewrite/docs/audits/` stay outside Git.

## Source/data authority

Inspected original server/client files retain last modifying commit
`c77f79017d7940c1500ba36a5dbf8bda6fefb67c`. TVERSION0x2918 is source evidence only;
matching executable/assets/region and actual original-client compatibility remain
**UNVERIFIED**. This scope follows ordinary non-BOW/non-BR behavior. No reference
source, client, historical snapshot or old migration was edited.

Actual local backup files were rehashed without restoring/executing their data:

| Archive | SHA256 |
|---|---|
| TGAME_RAGEZONE.bak | `2cff1635d523322d3de8ddafaeacc3335581dc338dca901239ac80288f51d259` |
| TGLOBAL_RAGEZONE.bak | `fca545bcb90bcc72aa4f3514a6177c2b88d2b0bf71bf847b0128f090a20877fb` |

Complete private historical manifests and original query-order receipts required
by079–081 remain unavailable in the checked publication paths. The existing069
operator importer also needs the complete pinned manifest; no import was run.
Its pinned profile is explicitly empty, not permission to create GM IDs or accept
an unpublished catalog as empty. Mutable operator provisioning/reset remains open.
No source SQL Server dependency or invented historical data was introduced.

## Original contracts and actual consumers

| Original file / symbol | Modern implementation / consumer |
|---|---|
| `CSHandler.cpp439::OnCS_MOVE_REQ`; `CSSender.cpp599::SendCS_MOVE_ACK` | Actual native `OnMoveReq` -> `PlanNativePlayerMove` -> `ApplyNativePlayerMove` -> existing cell notices/MOVE encoder/publisher/Asio drain |
| `TPlayer.cpp3998::MoveGhost`, `4050::CanHost`; `TMapType.h::GetDistance` | `native_player_host.cpp::PlanNativePlayerHostMove/NativePlayerCanHost`; canonical `NativeCellPlayer::host` and synchronized cell index |
| `TPlayer.cpp::CTPlayer`; `TObjBase.cpp::CTObjBase`; source partial `MW_ENTERCHAR` | `EnterNativePlayer`: real constructor HP0 mask for fresh partial replicas; actual full-primary HP mask; optional observed MOVE clock, separate raw ghost BYTE and coordinates |
| `SSHandler.cpp4733/5915/6113` full load/reload; `RELEASEMAIN` retained object | `LoadAuthorized/HydrateTransfer`; incoming transfer handler restores receiver-local motion then applies source alive/dead action; `RebindNativePlayer` retains ghost/CanHost/tick and synchronizes actual HP mask |
| `TMapSvr.cpp6614::IsOperator`; `DBAccess.h2859::CTBLOperatorChart` | `AuthorizeNativeMovementSpeed` and shared `ReadNativeOperator`; existing069 `world_compat.operator_release` and native `TOPERATORTABLE`, reused by actual chat delivery |
| `TAICmdSetHost.cpp::ExecAI` candidate portion; `TMap::GetNeighbor(VPLAYER)` | `SelectNativeMonsterHost`, connected to actual native SetHost executor; empty-host branch retains ResetHost/host-key behavior, selected-host branch refuses before assignment/fallback activation |

## MOVE ordering, state and lifetime

The26-byte request and27-byte ACK retain explicit existing serialization. Native
MOVE reads the header map WORD but uses the actual admitted map, as the source
uses `m_pMAP`; that input cannot move a character between maps. The development
adapter retains its earlier header-map check. No alternate gameplay architecture
or client change was added.

Speed is a FLOAT. The source strict positive `>3.40f` condition is preserved;
negative finite speed is not converted to an absolute-speed violation. For that
branch only, existing DB offload checks live Map ownership/account and current
Ready primary/replica claim, then the exact verified operator publication and ID.
No missing release, zero rows without proof or GM-name heuristic grants authority.
Failure closes the client. Normal speed needs no new query. Locking reads write
no gameplay rows; transaction release/failure uses existing native ownership behavior. The source
writes transient speed even before rejecting a speed violation; native rejection
closes before publishing a move plan. No durable or client speed output exists
on that rejected path; full teardown/monitor acceptance is still deferred.

Accepted movement writes speed and the currently sampled native AIThread DWORD
tick even on the ghost early return. Observed tick0 is present, distinct from no
MOVE yet. The source declares `m_dwMoveTick` but the inspected CTPlayer constructor
has no initializer and the only found assignment is MOVE. No allocator-dependent
initial value is asserted. Host selection refuses if it actually needs an
unobserved tick; it never substitutes0 or imports another Map's clock. The native
AI loop supplies the shared source-style sampled clock rather than creating a
new per-player wall clock. Timing/scheduling acceptance remains Phase B work.

When actual local HP is0, input mouse/key are changed to TKDIR_N4 before the
normal branch. Initial replicas really have source CTObjBase local HP0 even
though their authority snapshot deliberately has unknown combat bars. A separate
local mask avoids mistaking snapshot defaults for actual source HP. Actual
primary snapshots refresh that mask; confirmed handoff carries actual former/new
primary HP without resetting connection-local ghost/CanHost/tick. No unknown
replica bars become public actor descriptors.

Ghost stores the original BYTE and its own X/Z. FLOAT subtraction, multiplication,
addition and `sqrt(float)` preserve the source distance calculation; no double
hypot or squared-distance substitute. Near is strictly `<32`, far is `>=32`.
Raw CanHost changes before the previous action check. If the previous action is
TA_DEAD7, only speed/ghost/CanHost/tick change: body/pitch/direction/keys/action
stay, no cell move, routing, presence update or MOVE ACK. Otherwise ghost forces
action7 while retaining the old body XYZ; input pitch/directions still apply.
Exiting ghost clears its raw BYTE. The later active-action CanHost=TRUE check
stays after MoveGhost; a first far ghost can therefore reactivate the raw flag.
Actual CanHost in ghost mode ignores that raw flag and requires BOTH body-to-
ghost and body-to-monster distances strictly below32.

Source assigns m_bAction before its SITDOWN comparison, making that particular
recovery-clock reset condition false. This code preserves the observed order;
it does not repair the apparent intent by restarting recovery on sit input.
Native ENTER now serializes the actual source movement action, including action
accepted after a dead-body non-ghost MOVE. The previous encoder forced TA_DEAD
from bDead; that was a rewrite difference from source SendCS_ENTER_ACK. Full
loads establish action7 when actual HP0; incoming full reload with HP>0 resets a
retained action7 to stand0. Other receiver-local movement, including speed, stays.

Route/World waits retain an owned plan with exact socket claim and local cell
revision. Its original cell comes from the canonical actor after any operator
DB wait, rather than an older presence copy. Current Ready/connection/role/epoch is rechecked after every suspension;
application also checks actual map and revision before mutation. Every native
body move, including non-MOVE revival relocation, advances the nonwrapping local
revision. Revival does not fabricate ghost input or stamp a MOVE tick. Body/cell,
snapshot, presence and ordered packet publication then run without suspension
on the existing game executor under the existing gameplay lease. Close discards
this state with the cell lifetime. Bounds/nonfinite/overflow checks refuse unsafe
source conversions rather than reproduce undefined memory/arithmetic behavior.
Capacity/authority failure is not a successful truncated update.

## SetHost candidate selection and explicit limits

Candidates use actual source z/x neighboring cells and DWORD player order, with
no invented role/main/logical-channel filter. The first CanHost player is
preselected. FLOAT Manhattan distance is added before checked DWORD conversion;
strict smaller-than retains source tie order. Only a distance smaller than the
current best reads the observed move tick, then DWORD wrapping elapsed `<3000`.
The first raw CanHost=FALSE/non-ghost candidate is represented as fallback with
an activation marker. No speculative flag or host assignment is committed.

Actual native SetHost calls this selector. No candidate retains the existing
host-key increment/ResetHost/FALSE/no-completion flow. A selected candidate still
throws before mutation: NotifyHost, full SelectSkill/its inputs and awake/host
consumers are not complete. The marker is preparation, not implemented fallback
activation. This does not enable populated AI: Initialize and nonstale ACK still
refuse actual players, and admission stays latched for surviving source slots/
jobs. CHKHOST has different selection rules, including no SetHost recency filter;
its missing executor must be ported separately. There is CS_MONHOST_ACK, no
CS_MONHOST_REQ in the inspected source.

PC-only ghost event loops are vacuous because the process has no source monster
slots. Populated MoveGhost near AT_ENTER/far hosted AT_LEAVE, ghost-exit/active-
action AT_ENTER, PC cell Enter/Leave/LeaveAggro, NotifyHost, selected-host skill
state, monster movement/other requests and concrete awake/dead commands remain
missing. MOVE stand-hide erasure and position quests are also still missing.
Nonempty monster effects/stat producers, non-fresh Regen/death/loot, dynamic/
quest/battle/party-instance lifetimes and broader inventory gaps remain open.
These are implementation dependencies, not just unexecuted tests.

## Deferred Phase B scenarios — none executed

1. Compare exact26/27-byte wire output, real admitted map versus different header
   map, main/replica recipient order, finite negative speed, exactly3.40f and its
   next representable FLOAT, missing/changed/verified-empty operator publication,
   stale claims and DB failure. Exercise a future source-backed operator profile
   only after real provisioning exists; do not seed this historical empty profile.
2. Exercise initial primary alive/dead and partial-replica local HP0, first ghost,
   repeated ghost after action7, raw ghost255, ghost exit and near/far sequences.
   Check body XYZ held versus ghost X/Z, pitch/keys/action on both branches, and
   no MOVE/cell/routing output on the early return.
3. Exercise32 exactly and neighboring FLOATs in each predicate, first far ghost
   followed by the later raw CanHost activation, and non-ghost TA_STAND versus
   nonzero action. Confirm SITDOWN does not restart the unreachable source clocks.
4. Check observed tick0 versus absent, DWORD wrap, elapsed2999/3000, candidate
   order/ties/FLOAT-to-DWORD conversion, first preselection and raw fallback.
   Missing-tick and selected-host guards must precede any fabricated grant.
5. Suspend operator/routing/World work across close, rebind, role/epoch changes,
   same-ID replacement and revival relocation. Confirm no stale plan overwrites
   body/state or publishes client packets; check failure after World routing
   publication against existing transfer/close recovery. Confirm overflow refuses and close cleanup works.
6. Verify full reload action from actual HP, receiver-local motion/ghost/clocks
   across both ownership directions and CONREADY refresh, dead-body accepted
   action in ENTER, source-empty enabled/disabled/missing cell cases and main/
   replica output sequencing. Retain all populated actor barriers until their
   real event/host/skill consumers exist.

## Exact next handoff

First port `TMonster.cpp1461::SelectSkill` together with actual learned skill
objects/metadata and its `CheckPrevAct`, `GetTargetDistance`, `CanUseSkillNOW`
inputs. Connect source selected-host assignment, old/new `NotifyHost` ordering
and CS_MONHOST_ACK/CS_MONSETSKILL_ACK to the existing native cell outbox. Port
CHKHOST's distinct selector and real PC cell/MoveGhost monster events, host
requests such as CS_MONMOVE_REQ, and awake/dead commands before removing actor
isolation. Do not replace absent skill state with skill0 or an empty success ACK.
Then continue the broader lifetime/effect/combat/bootstrap inventory. Next082;
Phase B/C execution stays deferred. Inventory now has310 unique IDs/13 columns;
the DB map has74 rows/6 columns. These are record counts, not completion metrics.
