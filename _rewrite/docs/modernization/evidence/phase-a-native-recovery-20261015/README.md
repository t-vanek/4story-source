# Native resource recovery and mode — 2026-10-15

Baseline: local `main` at `1fe7404`. Status: **INTEGRATED_UNVERIFIED**, Phase A.
Only static source, caller, SQL-text, packet and diff review is authorized.
No build, tests, SQL execution, migration/import, backup extraction, service,
server, client, container or runtime acceptance is performed. Local Git only;
no push or deployment. Applied001–043 and written/unapplied044–069 remain
unchanged; next070. Private extracts and `_rewrite/docs/audits/` stay outside Git.

## Original producer and resource contract

`Server/TMapSvr/SSHandler.cpp:48/370–381` runs the player timer only for actors
with a map and primary ownership. `TPlayer.cpp:955–962` orders maintained-skill
expiry and automatic EXP, Recover, ResetAftermath, then the battle-mode timeout.
`CTPlayer::Recover:3547` simply calls `CTObjBase::Recover:1322–1407`.

Recover requires a map, a living actor and no actual STOPRECOVER maintained
status (`TObjBase.cpp:4088`). HP recovers only in MT_NORMAL0; MP recovers in
every source BYTE mode. Independent DWORD clocks are compared using
`tick >= clock + RECOVER_TIME3000`. A deficit receives exactly one actual
GetHPR/GetMPR increment, followed by the original maximum check. Its clock
becomes the current tick. There is no catch-up loop and no update to a full
bar's clock. A zero increment still sets the change flag, advances that bar's
clock and emits HPMP. Recovery itself sends no CHARSTAT packet.

`TObjBase.cpp:1558/1589/1611/2226–2239` derives actual recovery rates:

| Resource | Formula | Primary input | Powered item magic / ability modifier |
| --- | --- | --- | --- |
| HP | FTYPE_HPR9: init + DWORD(CON * FLOAT rateX) | Actual CON, including aftermath and supported effects | MTYPE_HPR32 |
| MP | FTYPE_MPR20: init + DWORD(MEN * FLOAT rateX) | Actual MEN, including aftermath and supported effects | MTYPE_MPR33 |

`CalcItemAbility:1624/1753` uses actual powered equipment. `CalcAbilityValue`
applies supported active and learned passive modifiers to these values. Rates
are not percentages of maximum HP/MP, nor the WORD primary values printed in
the statistics packet. Source DWORD addition may wrap; numeric handling must
preserve supported representable values and explicitly identify any refused edge.

## Client mode request and existing action dependencies

`CSHandler.cpp:1220–1231` requires a mapped primary, reads one BYTE and calls
ChgMode even when the value is unchanged. It does not restrict this parser to
NORMAL/BATTLE or require the actor alive. `TObjBase.cpp:295–340` updates the
actual mode and last-attack tick; BATTLE1 also sets both recovery clocks to
`tick + RECOVER_INIT5000`. Other mode values retain the recovery clocks.
The battle timeout uses strict `lastAttack + 5000 < tick` and runs after
Recover/ResetAftermath, so returning to normal cannot enable HP recovery earlier
in that same pass.

Ordinary `OnCS_SKILLUSE_REQ:2429` updates the PC attack tick at2864 and switches
a normal negative caster into battle at2916. Its attack/effect erasure occurs
later at2971–2972. `OnCS_LOOPSKILL_REQ:3040–3376` has none of these mode,
last-attack or effect-erasure hooks. ACTION does not refresh the attack tick.
`CTPlayer::Defend:1882/1938` does refresh it, including a positive self cure;
the existing native item-status producer already records its event tick.
The separate ACTITEMSUSE producer and complete native negative PC damage
remain independent OPEN responsibilities.

| Message | Original body | Independent client evidence |
| --- | --- | --- |
| CS_CHGMODE_REQ | one BYTE mode | CSSender.cpp116 |
| CS_CHGMODE_ACK | DWORD actor + BYTE type + BYTE mode,6bytes | CSHandler.cpp1952 |
| CS_HPMP_ACK | DWORD actor + BYTE type + four DWORD bars,21bytes | CSHandler.cpp3492 |

`CSSender.cpp:1315` also publishes PARTYMANSTAT only for the owner's own HPMP
and effective nonzero party. Each intermediate resource packet must retain its
own displayed bars and cause no per-spectator duplicate party publication.
No new opcode, level byte, synthetic stat packet or recovery-success code is needed.

## Local object clocks and transfer

The constructor starts both recovery clocks at0 (`TObjBase.cpp:20`). The complete
fresh-entry path ends at `OnMW_ENTERCHAR_REQ:1447/1622`, which sets both to
the current tick plus5000. Constructor defaults alone do not describe successful
entry: the first ordinary eligibility is therefore at entry +5000 +3000.
An actual later ENTERCHAR also resets these two clocks.

Full RELEASEMAIN/ENTERSVR does not carry recovery clocks or the last-attack tick.
`ClearMain:222` and the common load-to-existing-replica path do not reset them.
The target retains its own local object state; it must not import the remote
source clock or rebase on every hydration/checkpoint. Native partial replicas
drop their inventory payload, so retention must survive that payload removal.
The matching source mode is also local: CHARDATA/ENTERCHAR convey mode at entry,
but ordinary handoff to an already-ready replica need not repeat those messages
(`TWorldSvr.cpp:3839–3974`). Retaining target mode may differ from the source's
latest mode and is the original behavior, not justification for a new wire field.

## Integration and later acceptance

The real native primary timer now uses `NativeRecoveryWork` to select deficits
and battle-only timeout even without another status, aftermath, expiring item or
entitlement. `QuestMapFacts::ordinary_recovery` selects this PvE simulation in
opcode0; `AdvanceProgressEffects` calls recovery between maintained expiry and
aftermath, then performs automatic normal mode. `AdvanceRecovery` rederives the
current actual statistics and refuses unsupported context or unexpected resource
clamping. Published formulas9/20, real powered item magic32/33 and learned
passive ability rows32/33 supply optional rates; zero remains a real rate.

`CS_CHGMODE_REQ` enters the dispatcher and existing ChangeQuest transaction as
an exact one-byte request. Normal-mode revival shares `ChangeNativeMode` without
resetting HP/MP deadlines. Ordinary accepted SKILLUSE, positive self-FINISH and
item-status Defend update the real local activity clock; LOOP and ACTION retain
their original distinctions. Ordinary cast mode output follows item replies and
precedes effect-end/cast replies. Resources, supported effects, operation head
and core/graph checkpoint commit together before cached/client publication.
Unknown transaction outcomes retain close/recovery handling without write replay.

The primary entry descriptor is handled only in Loaded under its projection
operation; Ready repair uses the existing identity ACK. The serialized World
receive path completes initial projections before processing the descriptor.
Replica SQL completion rechecks its full Pending session identity before storing
the clock. Promotion captures target-local mode/clocks after outstanding local
operations drain; payload demotion and ordinary checkpoints retain them.
Capture and HydrateTransfer explicitly remove clocks from the typed transfer
state, and fresh/recovered sessions initialize through actual ENTERCHAR.

New HPR/MPR formula composition and ability results are limited to supported
nonnegative INT32 arithmetic; current-resource addition must fit DWORD before
the source maximum cap. Wrapped numeric branches remain explicit refusals.
The supported stat/effect graph excludes richer guild/local/companion/recall
dependencies rather than substituting invented zero bonuses. This is an
ordinary PvE primary implementation, not complete special-mode/secondary combat.

Recovery/mode output checks actual channel/map/socket and source3×3-cell presence,
current actor/recipient account/key/connection/epoch/role/Ready identity and the
live native owner/claim/name proof after SQL awaits. It reuses the actor's held
operation and takes only nonblocking operations on other recipients. Busy,
stale, moved or closed recipients can miss output; there is no application ACK,
uncertain-output replay or claimed complete reconciliation. The cast-mode event
uses this same boundary through `native_projection.h`.

[Map implementation](map-recovery.md) and [independent source review](analysis.md)
record the final supported graph, concrete entry/cast/finish/timer producers and
their static integration review. Full combat/death/SoulHP, unsupported maintained
graphs, companions/recalls/guild/local simulation and mode variants stay OPEN.

Phase B must separately verify actual entry +7999/+8000 boundaries, independent
HP/MP clocks, NORMAL/BATTLE/other BYTE values, repeated same-mode requests,
zero rate and full bars, actual formula/item/passive/aftermath contributions,
representable addition and overflow handling, and no offline catch-up. Verify
expiry → recovery HPMP → aftermath replies → auto-normal ordering, ordinary
SKILLUSE/positive self-Defend tick hooks and LOOP preservation. Exercise fresh,
repeated entry, promotion/demotion with retained local clocks/mode, checkpoint,
disconnect/crash recovery, stale owner/key/epoch, rejected/unknown COMMIT and
packet fanout/party MANSTAT. Full functionality and original-client compatibility
remain unverified; no historical tests cover this changed implementation.
