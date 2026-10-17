# Native monster presentation and cell output — Phase A, unverified

Baseline `7e1339b`, local `main`, 2026-10-17. Full source ADDMON wire/color are
**IMPLEMENTED_UNVERIFIED**. Fresh-slot state, normal monster entry/Remove and
bounded output consumers are **INTEGRATED_UNVERIFIED** within the existing
actor-isolated scope. Populated processes still cannot admit players; the new
recipient branches are connected structurally, not enabled/validated gameplay.
PC-to-monster events, host selection/skills/requests, effect-aware maxima and
full player/monster coexistence remain **IMPLEMENTING**. Full Phase A is incomplete.

No build, test, SQL, import, migration, service, client, container, runtime check,
push or deployment ran. Only source/interface/CMake/diff and inventory metadata
inspection. Private extracts and `_rewrite/docs/audits/` remain outside Git.
No migration was added.044–081 remain written/unapplied; next free number082.

## Source/data authority

The inspected original server and corresponding available client files still
have last modifying commit `c77f79017d7940c1500ba36a5dbf8bda6fefb67c`; they were
not edited. TVERSION0x2918 is source evidence only. Matching executable/assets/
region profile and client compatibility remain **UNVERIFIED**. This increment
follows ordinary non-BOW/non-BR OT_MON behavior, not recall/self/companion modes.

Actual local backup files were rehashed without executing/restoring their data:

| Archive | SHA256 |
|---|---|
| TGAME_RAGEZONE.bak | `2cff1635d523322d3de8ddafaeacc3335581dc338dca901239ac80288f51d259` |
| TGLOBAL_RAGEZONE.bak | `fca545bcb90bcc72aa4f3514a6177c2b88d2b0bf71bf847b0128f090a20877fb` |

079 supplies real source templates/attribute-family vitals,080 actual own AI
presence,081 the original channel/owner graph. Their complete private manifests
and actual original query-order captures are still absent from the checked
publication paths. No public schema metadata was treated as row proof. No
historical rows, legacy snapshots, backup, existing migration or grant was changed.
This is source RAM/protocol work; no new SQL Server dependency was introduced.

## Concrete source to modern paths

| Original file / symbol | Modern implementation / actual consumer |
|---|---|
| `CSSender.cpp913::SendCS_ADDMON_ACK`; `Client/TClient/CSHandler.cpp1771::OnCS_ADDMON_ACK` | `native_monster_presentation.cpp::EncodeNativeAddMonAck`; actual native `PublishNativeCellNotices` ADDMON branch |
| `TMonster.cpp54::GetColor`; `TObjBase.cpp4962::GetWarCountry`; source effective `GetPartyID` | `NativeMonsterColor`; actual recipient snapshot's country/aid and SOLO-effective party, with full connection/role/epoch fencing |
| `TObjBase.cpp7::CTObjBase`; `TMonster.cpp7::CTMonster`; `TSkill.cpp3/100::CTSkill/GetRemainTick` | Native slot pitch/key/mouse/neutral aid and owned raw maintained fields; full record encoder with source DWORD clocks |
| `TMonster.cpp1700/1708::GetMaxHP/GetMaxMP`; `TObjBase.cpp1409::CalcAbilityValue`; `TAICmdRegen.cpp192` | Existing actual signed-Win32 fresh maxima producer now establishes explicit `maxima_ready`; real HP/MP/template level feed `ProjectNativeMonsterAppearance` |
| `TMap.cpp973::EnterMAP(CTMonster)`; `TCell.cpp::EnterMonster` normal branch | `EnterNativeMonster`; actual Regen's required EnterMap callback stages immutable projection/recipients, then the same executor emits one AT_ENTER |
| `TMap.cpp1115::LeaveMAP(CTMonster)`; `TCell.cpp550::LeaveMonster`; `TAICmdRemove.cpp::ExecAI` | `LeaveNativeMonster`; new required LeaveMap consumer stages normal DELMON(TRUE) before source cancellation/vector/slot erase |
| Source neighbor cell map traversal and owned output before later AI mutation | Shared `NativeNeighborCells`; `StageNativeCellNotices/TakeNativeCellNotices`; actual native AI IO turn after ACK, then existing Asio `PostPacket`/channel/drain |

The former Enter/LeaveNativeBootstrapMonster functions are extended/renamed in
place, not retained as a parallel implementation. PC cell traversal now uses
that same neighbor helper. Neither source slots nor these projections enter
the development MonsterInstance registry. Its encoder remains explicitly marked
as a prototype with incomplete fields and is excluded from native entry.

## Packet and state contracts

ADDMON explicitly writes all51 base bytes: DWORD instance, WORD actual template,
BYTE **template** level, max/current HP and MP, FLOAT XYZ, WORD pitch/direction,
BYTE mouse/key/action/mode/new-member/country/color, DWORD region and BYTE count.
Mutable slot level is not substituted for `m_pMON->m_bLevel`. HP/MP come from the
actual source slot's derivation; absence of a template, attribute, generation or
completed maxima producer refuses projection instead of producing zero bars.
Source constructor TKDIR_N is4; pitch starts0; aid/country neutral is3. Spawn
country and direction still come from actual source InitMonster/Regen data.

Each maintained record writes all51 bytes in source order: skill, rank, remaining,
attacker/type, host/type, hit, attack level, attacker level, four powers, selection,
attack country and XYZ. Raw start/duration are process-local DWORD values.
`start==0` returns remaining0; otherwise unsigned `tick-start` wraps and expiry
compares against duration. Expired records stay in the transmitted prefix with
remaining0. Duration0 is not converted to a fabricated finite expiry. Neither
the player's monotonic64-bit sampled-ms model nor persistence expiry filtering
is substituted for CTSkill::GetRemainTick.

Original SendCS_ADDMON_ACK narrows the vector size to BYTE and loops only over
that prefix:255 emits255,256 emits0,257 emits1. The encoder preserves that safe,
observable narrowing rather than sending a wrapped count followed by all rows.
Projection retains at most that prefix; immutable snapshots do not retain an
unbounded full source vector. Header/generation/tick are internal metadata and
are not serialized as new client fields. No native C++ object layout is streamed.

**Live effect producer remains missing.** The owned raw fields and complete wire
codec do not implement effect application or derived stat recalculation. Current
live `ProjectNativeMonsterAppearance` explicitly rejects nonempty maintained
state; current `maxima_ready` is established only by actual fresh empty-state
Regen and invalidated by template resolution. Non-fresh Regen also refuses such
state. This protects against displaying effects with old unmodified maxima and
is not classified as completed maintained-monster gameplay. The standalone wire
format's full effect branch is implemented/unverified; its actual nonempty live
producer remains an implementation dependency.

GetColor preserves the original branch order: same war country -> ALLI0;
non-neutral monster -> ENEMY3; GOHOME -> PIECE2; matching private/party keeper ->
ENEMY3; other nonzero keeper -> NO_REWARD1; BATTLE -> ENEMY3; own FIRSTATK AI ->
ENEMY3; otherwise PIECE2. War country uses aid unless neutral3. Recipient party
uses source effective SOLO semantics, not merely the stored raw party field.
The own template AI pointer is resolved from the actual080 type presence.
Missing own AI refuses only the final dereference branch; earlier branches still
work. DEFAULT_AI fallback belongs to OnEvent and is deliberately not invented
for GetColor. Refusal replaces the source null dereference, not a game rule.

## Cells, ordering, lifetime and limits

Normal monster entry inserts real cell membership even if its logical channel
is disabled, as the source normal branch does. It walks neighbor cells in z/x
order and each cell's DWORD player order. Every recipient is tested against the
monster's **origin** cell IsMain(recipient logical channel), with new-member TRUE.
Projection is lazy: if no recipient needs a packet, no missing template/AI is
dereferenced just to manufacture a descriptor. EnterMAP affiliation is recorded
even without an origin cell; the executor still emits its one AT_ENTER afterwards.

Normal LeaveMonster sends DELMON to every neighbor player without a main/channel
filter. It has none of the recall/self/companion aggro side effects. Remove stages
exit-map TRUE before exact-generation AI cancellation and vector/slot deletion;
even an anonymous never-entered slot follows source LeaveMAP's neighbor behavior.
Owning map association is separate from the entered marker, as in prior work.

One immutable shared appearance is captured per monster entry before subsequent
AT_ENTER execution; notices retain source ID and process generation after erasure.
An ADD then immediate REMOVE yields FIFO ADD/DEL history. Delivery must not drop
the earlier ADD merely because the current slot is absent or the DWORD ID now
belongs to another generation. The original client ignores duplicate ADDMON IDs,
so preserving DEL/ADD order across reuse matters. These captured events do not
provide authority for new gameplay against an erased object.

The same native AI mutex protects stage/take. The existing single IO game
executor consumes the outbox immediately after a delivered ACK, without awaiting
network capacity. Current recipient identity/snapshot is rechecked and owned
packet bodies reach existing PostPacket/strand/channel/framing/encryption/drain.
Ordinary PC cell output continues through that same publisher. Generation is
checked against its captured appearance, not a replacement slot. New outbox
limits are65536 notices and conservative64MiB, charged per recipient even for
shared appearances. Overflow throws and the existing AI failure path closes/
drains; no prefix is returned as successful output. Existing per-peer bounded
PostPacket retirement remains in force. Stop/owner/World/signal retirement clears
queued cell output with AI work. No output/job persistence table was invented.

The populated-process admission barrier remains latched over surviving slots or
jobs. Source-empty processes retain the previously integrated PC lifecycle.
Initialize and nonstale ACK now additionally refuse any actual player population
before actor-free command execution; stale object/generation/host-key ACKs still
return first without conditions/RNG. These are protection boundaries, not hosted
AI completion. Current supported constructor/AI runs therefore produce zero client
monster recipients. Conditional recipient paths are implemented and connected
but have not been enabled, exercised or accepted with a player/monster population.

## Deferred Phase B scenarios — none executed

1. Compare source/client field-by-field decoding of all51 base bytes and51-byte
   effects. Use different template/mutable levels, real nonzero MP, pitch/key/
   mouse/action/mode/region and all nondefault effect metadata. Confirm native
   MonsterInstance prototype descriptors never appear on this path.
2. Exercise tick0, start0, duration0, exact expiry, already expired and DWORD wrap;
   effect counts0/1/255/256/257 and duplicate skills preserve original order and
   prefix semantics. No expiry filtering or accidental unlimited/finite swap.
3. Check every GetColor precedence collision, effective SOLO party, neutral/aid
   country and own AI presence versus missing own AI with available DEFAULT_AI.
   Confirm only the original unsafe dereference branch is refused.
4. Compare normal entry origin-main/channel predicates, missing/disabled origin,
   two logical channels, replica observers, z/x/DWORD recipient order and one
   AT_ENTER. Compare Remove DELMON(TRUE) without a main filter, including slots
   not previously entered and source anonymous slots.
5. Capture ADD/DELETE/ID reuse, later source mutation, close and role/epoch changes.
   Prove immutable lifetime ownership and FIFO order despite erased slots; verify
   client duplicate-ID behavior and all actual framing/encryption/output queues.
6. Exercise outbox count/byte/per-peer capacity and authority retirement/failure.
   Confirm bounded memory, failure/drain instead of successful truncation, and no
   output after retirement. Existing external-owner replacement race acceptance
   remains deferred; the new RAM outbox is not an atomic DB owner transaction.
7. Confirm nonempty live effects and absent/unready maxima refuse before false
   bars; do not label raw wire fixtures as an implemented effect/stat producer.
   Preserve actor-admission refusal until PC/monster host/event dependencies exist.

## Exact next handoff

Extend current player EnterPlayer/LeavePlayer/channel movement with actual
monster ADD/DEL and source AT_ENTER/LeaveAggro/AT_LEAVE sequencing. Supply real PC
CanHost/MoveGhost/move-clock state; source TAICmdSetHost selection, NotifyHost and
SelectSkill; CS_MONHOST_ACK and actual host-produced requests such as
CS_MONMOVE_REQ; concrete awake/dead AI and their state/metadata. There is no
CS_MONHOST_REQ in the inspected source; do not invent a matching request opcode.
Nonempty maintained effects need real mutation/stat producers before removing
their projection refusal. Preserve the process admission barrier until those
producers and consumers support actual player/monster coexistence. Continue
non-fresh Regen/death/loot, dynamic/quest/battle and party-instance lifetime.
Next migration082; Phase B/C execution remains deferred. Inventory now has305
unique IDs/13 columns; that record count is not a completion metric.
