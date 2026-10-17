# Native aggro, host changes and mode commands — Phase A, unverified

Baseline `fbc91a7`, local `main`, 2026-10-17. Source CHGHOST/AddAggro/LeaveAggro,
leader/party follower events and CHGMODE now connect to the existing native AI
executor. Current accepted-player snapshots supply the documented supported
country branch. The existing constructor/events/delayed ACK/factory/outbox paths
are **INTEGRATED_UNVERIFIED** within actor isolation. Conditional populated
command/recipient branches are **IMPLEMENTED_UNVERIFIED** within the scope below.
All latched player-admission and actual-player Initialize/ACK guards remain.
Full hosted gameplay and Phase A remain **IMPLEMENTING**.

No build, test, SQL, migration, import, service, client, container, runtime check,
push or deployment ran. Static source/interface/CMake/diff/inventory inspection
and read-only backup hashing only. No new migration, grant or publisher change;
044–082 are written/unapplied, next083. Private database extracts and pre-existing
`_rewrite/docs/audits/` remain outside Git.

## Authority and database provenance

Inspected original server/client sources remain at
`c77f79017d7940c1500ba36a5dbf8bda6fefb67c`. Their trees, attributions and headers
were preserved. TVERSION0x2918 is a source marker; corresponding executable,
assets, region and actual original-client compatibility remain **UNVERIFIED**.
This increment follows ordinary non-BOW/non-BR OT_MON source behavior.

Actual local archives were rehashed without executing/restoring their data:

| Archive | SHA256 |
|---|---|
| TGAME_RAGEZONE.bak | `2cff1635d523322d3de8ddafaeacc3335581dc338dca901239ac80288f51d259` |
| TGLOBAL_RAGEZONE.bak | `fca545bcb90bcc72aa4f3514a6177c2b88d2b0bf71bf847b0128f090a20877fb` |

These source operations mutate process-local host/target/aggro/clock state, not
database rows. Existing079 actual spawn/leader/party order,080 AI graph,081 owned
cells and082 skills are consumed through the native PostgreSQL constructor and
existing subsequent owner/release checks. No dynamic aggro/host table or SQL
Server dependency was added. Complete private manifests and original query-order
receipts for079–081 remain unavailable in the checked publication paths; no
synthetic metadata or historical snapshot edits replace them.

## Concrete source to modern connections

| Original source / symbol | Modern implementation and consumer |
|---|---|
| `TMonster.cpp323::AddAggro`; `TMapType.h20::MAKEINT64/1249::TAGGRO` | `native_monster_aggro.cpp::AddAggro/Key`; real CHGHOST inserts/increments `NativeMonsterSlot::aggro`, later consumed by LeaveAggro and ResetHost |
| `TMonster.cpp87::LeaveAggro`; `TMap.cpp689::FindNeighbor` | `NativeMonsterAiExecutor::LeaveAggro`; source neighboring PC/normal-monster lookup; actual CHGHOST missing-new-host branch consumes the result before reset/re-entry |
| `TAICmdChgHost.cpp::GetDelay/CanRun/ExecAI` | native command kind4 eligibility, `ChangeHost`, existing zero-delay DoCommand, Initialize and queued-ACK dispatch; NotifyHost/SelectSkill use the same bounded output path |
| `TMonster.cpp255::SetEventToFollower`; `TMap.cpp::FindMonSpawn/FindMonster` | `SetEventToFollower`; current real spawn vectors, source ordered per-spawn party memberships and existing synchronous Event/DoCommand graph |
| `TObjBase.cpp4122/4140/4962::HaveDisguiseBuff/GetCountry/GetWarCountry` | required `ReadPlayerCountry` callback; actual main session registry/current snapshot and existing PostgreSQL `ReadPostures/ReadProgressEffects` proof; unsupported/unloaded disguise state refuses |
| `TAICmdChgMode.cpp`; `TObjBase.cpp2594::CheckAttack/295::ChgMode` | native command kind5 eligibility receives real event arguments, `CheckAttack`, source mode transition, existing mode/color outbox and recovery clocks before host-key/base completion |

The new translation unit is registered once in the existing Map CMake target.
No parallel server, actor registry, event bus or persistence model was introduced.

## Preserved semantics and boundaries

Aggro keys retain source object-DWORD-in-high32/type-in-low32 bits and signed64
map ordering. AddAggro increments the DWORD with wrapping arithmetic while
preserving an existing record's host/object/type/country. A new CHGHOST record
uses the actual event host, real target/type, current host war country and value1.
This is the original AddAggro producer; combat `SetAggro` damage/heal/class/active
arithmetic is a separate function and remains unimplemented.

LeaveAggro erases the specified key if present. A missing key matching the current
target emits AT_LEAVELB immediately, even before the GOHOME test. Otherwise it
selects the first strict maximum in signed-key order whose raw country differs
from the monster's raw country; it does not compare war country or sort by value.
GOHOME then returns FALSE without further lookup. An absent/zero-object best
record emits AT_LEAVELB with the current invocation arguments. A present best
neighbor differing from the current target emits AT_DEFEND and returns TRUE.
An identical current target returns FALSE. An absent best neighbor is removed
through the next source recursive step. The outer recursive caller always
returns FALSE, even if an inner AT_DEFEND succeeds; this behavior is retained.

FindNeighbor searches the same real neighboring cells, without role/channel/
distance filters or FindMonster's status/template filters. PC indices are fenced
against canonical current actor claims. Normal monster indices must refer to an
owned object, including the retained disabled-old-cell membership from the prior
increment. Recognized recall/self/companion lookups refuse pending their real
registries. Other source-unrecognized types return absent. Unknown supported-type
state is never replaced with a fabricated object or a zero-valued object.

CHGHOST tests WAKEUP before source base conditions and has delay0. It resolves
old/new hosts through neighboring cells. With no new host it increments host key,
invokes LeaveAggro, and only on FALSE resets host then emits AT_ENTER with all
zero event arguments. This branch never calls base completion/repeat. With a new
host it first compares monster raw country with actual player GetCountry; equality
returns FALSE without mutations. Changed host increments key, assigns host,
notifies and selects skill **before** assigning the new target. The same-host
branch skips those outputs but still writes target, AddAggro and follower events.
Base completion follows follower events; CHGHOST does not assign wakeup or mode.

Follower notification preserves leader precedence over party routing. The leader
branch visits its own current spawn vector; a nonleader with a party uses the
actual per-spawn catalog query order, retaining duplicate party memberships.
Each receiver uses source FindMonster's actual-template/status-not-DISAPPEAR
predicate plus targetID0. No additional wakeup, main, channel or self exclusion is
added. Every notification reads the initiating monster's then-current host/target/
type, as earlier synchronous events can change them. Vector size and indexed
member are re-read after each event, preserving removal/skip behavior instead of
replaying a stale copied receiver list.

The player-country callback is bound once alongside global actor lookup before
network/AI startup. It checks the exact accepted lifetime, then reads the current
native character snapshot, with no DB work or suspension inside the AI turn.
The existing PostgreSQL posture/progression readers reject transform entries;
`SupportedPostures` therefore proves GetCountry equals raw country for the actual
supported maintained collection. GetWarCountry uses actual aid-country unless
neutral3. Missing payload/maintained evidence, general disguise transforms and
replica projections without complete effect authority refuse. This limited proof
does not implement full GetCountry/replica disguise semantics. The snapshot store
now outlives native callbacks; retirement clears both country and actor callbacks.

CHGMODE tests WAKEUP, matching event host and NORMAL-only CheckAttack, in that
order before base conditions/RNG. CheckAttack rejects actual getaway or disabled
attack before inspecting effects; an actually empty maintained vector permits
attack. Nonempty monster status/AI metadata remains a required missing input and
refuses instead of assuming no blocking effect. The getaway field is the real
CTObjBase constructor FALSE; Getaway and runaway-effect mutation remain separate
unported producers. The mode command cycles NORMAL->BATTLE->GOHOME->NORMAL;
target/type clear only on the last two transitions. Unknown modes still call
ChgMode and increment key as the source switch does. ChgMode captures per-recipient
mode/color and DWORD recovery clocks, then host key increments, then base
AICOMPLETE/repeat behavior. No command-only success response was invented.

## Safety differences and remaining dependencies

MAKEINT64 uses unsigned bit construction followed by C++20 bit_cast into signed64,
avoiding the original signed-shift overflow while retaining stored bits/order.
Missing-target LeaveAggro recursion is iterative; it preserves callbacks and the
discarded inner result without unbounded C++ stack growth. Existing per-turn
work/event-depth bounds still apply. Synchronous event boundaries re-find the
initiating generation and spawn/member ownership before subsequent accesses;
Remove cannot leave a borrowed iterator/reference usable after object erasure.
Initialize/Event also stop their remaining binding iteration after a generation
change, and Complete does not calculate delay/repeat on a replacement object.
Removed initiating objects do not emit base completion on a replacement lifetime.
These are deliberate memory/resource safety repairs, not changed aggro rules.

Populated admission and actual-player AI guards are unchanged. These conditional
paths do not establish playable combat or a complete actor event graph. PC
Enter/Leave/channel/MoveGhost still need source event integration and two-sided
loot inventory lock lifetime. Source MONITEMLIST/TAKE creates those locks; no
zero-valued placeholder lock was installed. Combat SetAggro, full monster effects/
CheckAttack, general player disguise/replica state, host movement/attack/skill Use,
remaining awake/dead commands, non-PC targets, non-fresh Regen/death/loot, dynamic
spawns, battles and party instances remain open.

## Deferred acceptance and precise next step

Future Phase B scenarios include signed high-bit object keys, wrapping existing
aggro without host/country replacement, strict equal-value ties, raw versus aid
country, absent-key/current-target LEAVELB precedence, GOHOME, same-target no-event,
chains of absent neighbors and discarded recursive TRUE, real PC/monster index
lookup, missing new host reset/re-entry order, hostile/allied/same host behavior,
old-target SelectSkill order, source leader/party/duplicate order, dead versus
disappeared followers, reentrant vector removal and initiating generation reuse,
current country/effect changes and stale accepted claims, unknown disguise refusal,
CHGMODE host/status/condition RNG order, all mode transitions, unknown mode, target
clearing, tick wrap and per-recipient mode/color/host output. None was executed.

Next complete native source maintained-skill status/AI metadata and CheckAttack,
plus general GetCountry/replica disguise authority; then host movement/attack/skill
and remaining awake/dead commands. Integrate combat SetAggro with actual damage/
heal/skill producers and PC events with real MONITEMLIST/TAKE lock lifetimes.
Reuse these aggro/CHGHOST/follower/mode paths and keep player guards until connected
coexistence exists. Inventory328 unique IDs/13 columns; DB map75 rows/6 columns.
Counts are navigation metadata, not completion metrics. Next migration083; full
Phase A is incomplete and Phase B/C remain deferred.
