# Source FOLLOW and conditional host movement — Phase A, unverified

Baseline `1070f72`, local `main`, 2026-10-17. Ordinary FOLLOW is now concrete
source command8, **IMPLEMENTED_UNVERIFIED**. Actual accepted host pose reads and
owned MONACTION/MONMOVE publication are **INTEGRATED_UNVERIFIED**, within the
existing actor isolation. Host MONMOVE has a connected conditional normal-monster
flow, but remains **IMPLEMENTING**: source OnDie/out-of-roam and non-PC object/
target registries are still missing. This is not completed hosted gameplay.
All populated admission, Initialize and nonstale AI ACK barriers remain.
Full Phase A remains **IMPLEMENTING**.

No build, test, SQL, migration, import, publisher, service, client, container,
runtime verification, push or deployment ran. Only source/interface/CMake/SQL-text/
diff/inventory inspection and actual backup hashing were performed. No new schema
migration, grant or publisher was needed:044–083 and existing grants/publishers
remain written/unexecuted, next084. Private extracts and pre-existing local
`_rewrite/docs/audits/` remain outside Git.

## Authorities and profile

Inspected original server/shared/client revision:
`c77f79017d7940c1500ba36a5dbf8bda6fefb67c`. A static Git comparison confirms the
inspected original Follow/Getaway/Gohome, CSHandler/CSSender, Map/Monster/ObjBase,
MapType/LoadData, shared NetCode/CSProtocol and client CSHandler paths still match
that revision. Reference trees, attributions/license headers, backups and old
migrations remain unchanged. The implementation profile is ordinary non-BOW/
non-BR normal OT_MON. TVERSION0x2918 is only a source marker; matching executable,
assets, region and actual client compatibility remain unverified.

Actual local backup SHA256, re-read without restoration or SQL interpretation:

| Archive | SHA256 |
|---|---|
| TGAME_RAGEZONE.bak | `2cff1635d523322d3de8ddafaeacc3335581dc338dca901239ac80288f51d259` |
| TGLOBAL_RAGEZONE.bak | `fca545bcb90bcc72aa4f3514a6177c2b88d2b0bf71bf847b0128f090a20877fb` |

Private complete historical manifests and actual original079–083 query-result
order receipts remain unavailable at their publication inputs. No source values,
template IDs, query orders or historical content were invented.

**Concrete protocol disagreement:** `CSProtocol.h511` describes MONACTION with
WORD X/Z and no Y. `CSSender.cpp1084` and `Client/TClient/CSHandler.cpp977` both
serialize/read FLOAT X/Y/Z, followed by target DWORD/BYTE. The implementation
uses those agreeing executable source contracts (22 body bytes), retaining the
original conflicting comment unchanged. The stock binary has not been checked.

## Source to implementation and connected consumers

| Original file / symbol | Modern file / connected consumer |
|---|---|
| `TAICmdFollow.cpp::GetDelay/CanRun/ExecAI` | `native_monster_ai.cpp::CanRun/Delay/Execute`; `native_monster_motion.cpp::Follow`; existing Event/DoCommand/batch ACK/Complete |
| `TMonster.cpp::FindHost`; actual `CTPlayer` body pose | existing `FindNativeMonsterHost`; new required `ReadPlayerPose`, `main.cpp` accepted actor + actual `CharStateStore` callback; current load/replica/handoff/MOVE producers remain authoritative |
| `CSHandler.cpp919::OnCS_MONMOVE_REQ` | `handlers/monster_movement.cpp::OnMonMoveReq`, actual dispatch/replica permission under `SessionOperation`; `DecodeNativeMonMoveRequest`, `PostgreSQLMapService::MoveNativeMonsters`, `MoveHostedMonsters` |
| `TMap.cpp1481::FindMonster`, `1350::OnMove`; `TMapType.h142::GetDistance` | template/non-DISAPPEAR lookup; existing `MoveNativeMonster`/required move callback; one following AT_LEAVE; source FLOAT subtract/multiply/add/sqrt order |
| `TMapSvr.cpp2813`; `DBAccess.h` monster `wChaseRange` | `LoadNativeMonsterCatalog`, `NativeMonsterAllocationTemplate::chase_range`; existing full079 `monster_compat.TMONSTERCHART` view, strict SMALLINT bits -> WORD |
| `TMonster.cpp::LeaveAggro`; `TMapType.h` AT_ATHOME11 | existing real `LeaveAggro` and Event consumers from the host movement path, after actual cells and AT_LEAVE |
| `CSSender.cpp1084/1113/1142`; client action/move handlers977/1665 | `EncodeNativeMonActionAck/MonMoveAck`; captured `NativeMonsterAction/MoveRecord`, `MonsterAction/MonsterMoveBatch`, existing bounded FIFO/current-recipient Asio publisher; reset ACK shares the movement encoder |

Both new C++ files are registered once in the existing Map core CMake target.
No new runtime, registry, service, language or parallel monster implementation
was added. The existing development prototype is not used as source authority.

## FOLLOW order and host lifetime

Eligibility first checks WAKEUP1, BATTLE1, event-host equality, nonzero target,
spawn roam type !=4 and source CheckAttack; only then run base conditions. Delay
is0. Exec separately resolves the actual neighboring host; unlike BEGINATK,
CanRun does not require FindHost to succeed. When present, the host receives
TA_FOLLOW9 plus its current real body XYZ and current target ID/type. This does
not move the monster immediately. MONACTION capture precedes SelectSkill(FALSE),
then the host key increments. Missing host still increments that key and calls
base completion. Source AICOMPLETE/repeat behavior is unchanged.

Pose comes from the current accepted connection/role/epoch and actual character
snapshot on the existing IO turn. Map ID and cell XZ must agree; all XYZ must be
finite. The callback performs no SQL and adds no pose cache or zero-Y fallback.
Actual player load, descriptor, confirmed role handoff and body/ghost MOVE already
produce that snapshot. Shutdown clears the callback along with country/global
lookup. The captured notice retains the recipient claim and source generation.

## MONMOVE conditional source flow

The decoder validates a complete WORD count and exactly27 bytes per record:
DWORD id, BYTE type/channel, WORD map, three FLOAT, WORD pitch/direction, BYTE
mouse/key/action. Framing and all finite values validate before any mutation.
Record channel/map are deliberately decoded but unused: lookup uses the real
reporting claim and its actual ordinary local map. Both main and replica can
report source-hosted monsters; dispatch retains the existing gameplay operation
and exact Ready connection/role/epoch lifecycle.

For OT_MON2, missing object/template or DISAPPEAR0 is ignored, as FindMonster
does. Only the actual host may report. Old TA_DEAD7 ignores the record; HP0 forces
keys4, without inventing a death transition. Nonpositive X/Z ignores the record.
Defined finite WORD cell coordinates are required. The handler assigns new keys,
action, pitch/direction and Y before existing OnMove cell processing. Changed
cells produce source leave/channel/enter notices followed by one AT_LEAVE8 with
zero arguments; same-cell moves produce no such event.

The MP recovery predicate stays after OnMove and compares the now-current action
against the incoming STAND0. It does not compare a remembered old action. Usually
the assigned action makes it false; a synchronous AT_LEAVE can change that result.
This unusual source order is retained, with DWORD tick+5000 wrap.

BATTLE1 with OT_PC1 deliberately treats the reporting player itself as target,
even when the stored target ID differs. Missing actual reporter map membership
invokes LeaveAggro and skips this record's insertion. With the actual target
present, strict source `wChaseRange < distance(start,current)` invokes LeaveAggro.
This uses the real template WORD, separately from the normal-mode constant64.
GOHOME2 invokes AT_ATHOME11 at distance<=3 using current host/target arguments.
Other modes permit movement within64; greater distance reaches the explicit
incomplete OnDie boundary described below.

A DWORD-ordered map retains the first accepted object generation per ID. After
the entire input, the final current object state is captured once, so duplicate
records report the final pose instead of the first pose. There is no extra final
DEAD filter: source retains an already inserted live pointer across later input
mutations. Deletion or ID reuse is fenced by generation instead of dereferencing
a freed source pointer. That intentional safety deviation can omit an invalidated
record; it never serializes its replacement as the old object.

Final output uses reverse GetNeighbor order around the reporter's current X/Z,
excluding only the reporter ID. Each recipient receives the same immutable batch:
WORD count, then24 bytes per normal monster (DWORD id, BYTE type2, XYZ FLOAT,
pitch/direction WORD, mouse/key/action BYTE). The actual map sender returns
immediately for an empty result; no WORD0 success packet is invented. Source
scalar reset movement uses the same encoder with one captured record. New wire
helpers explicitly assemble LE integers and IEEE754 FLOAT bits without native
object layout; older POD consumers were not broadened by this change.

All cell/event notices precede the final batch in the existing bounded FIFO.
The native service holds its AI mutex throughout and drains owned notices to the
same fenced Asio PostPacket path without a SQL/World await. Count/byte budgets
charge the full22-byte action or2+24*N movement body per recipient, even when
the immutable vector is shared. A partial event/movement failure retires AI
authority, stops its queue and discards undelivered output; it cannot return
success with an incomplete graph. A later publication exception also invokes
the same existing `StopNativeMonsterAi` through the native player-service
interface, after releasing the AI mutex. Existing recipient lifetime/drain rules
apply; external packet/session callbacks do not execute under that mutex.

## Remaining implementation and exact handoff

`MAP.MONSTER.MOVE.NORMAL` remains **IMPLEMENTING**. Recall7, self11 and companion18
actors and non-PC battle targets require their actual reporter-owned registries;
the missing registry is not treated as an empty successful lookup. Other unknown
input types keep the original no-object switch behavior. Instance/dynamic maps,
effect-aware maxima/ADD projection and PC monster entry/move/leave/loot-lock
events remain incomplete. Remaining GETAWAY/REFILL/ROAM/GOHOME/LEAVE/LOTTERY AI
commands are still open. No populated actor guard is removed by this increment.

The next implementation is `MAP.MONSTER.MOVE.DEATH`: trace and port ordinary
`CTMonster::OnDie(0,OT_NONE,0)` from `CSHandler.cpp` normal distance>64. Follow
`Transformation` first (real maintained skill/size/maxima/HPMP/DEFEND), then
`CTObjBase::OnDie` (status/action/bars, trans-attack cleanup, maintained release,
reverse-neighbor DIE), then actual damage/keeper/party reward and quest/MW
branches, battle/event drop/level limits, `AddItem` money/drop RNG, pet collection
and AT_DEAD. It must work with zero damage as well: source can still generate
items, so zero damage is not permission to omit loot. Complete required dead AI,
removal/respawn and source data producers before replacing the explicit boundary.
Trace required native PostgreSQL player reward/quest contracts and write only
needed new migrations (next084), without SQL execution.

Then complete non-PC movement/target registries and the remaining motion AI;
connect the client's OT_MON SKILLUSE/LOOP/FINISH/DEFEND chain, resource debit,
effect/maintained/remain/instance mutation and expiry, combat SetAggro and PC
event producers. Other server areas remain open in the same unified inventory.
Full Phase A has not reached IMPLEMENTATION_COMPLETE_UNVERIFIED.

Phase B scenarios prepared, **not executed**: FOLLOW each gate and base condition
order; host disappearing between eligibility and Exec; nonzero Y/body/ghost pose;
role/epoch mismatch; MONACTION exact22-byte order; key wrap and AICOMPLETE removal;
short/extra/count/NaN/infinite/out-of-range movement; mismatched wire routes;
wrong host/template/disappear/old-dead; HP0 keys; same-cell, changed and missing
destination cells; event-induced action/host/removal/generation changes; MP
post-action predicate; PC target-ID mismatch; exact chase/3/64 boundaries;
LeaveAggro/ATHOME arguments; duplicate IDs/final state/status and sorted order;
empty input or accepted set produces no packet; reverse reporter-neighbor
recipients, replica reporter and excluded reporter ID; immutable captures across
later object mutation; framing/outbox limits and partial-failure retirement.
OnDie/non-PC refusal is a known missing branch, not a successful parity test.

Static inventory:343 unique IDs/13 columns; DB map78 rows/6 columns. These are
navigation counts, not completion percentages. No old test result is evidence
for this increment. Phase B and C remain deferred.
