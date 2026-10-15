# Map native own-cure MISS and Avoid802

Baseline `391f499`, 2026-10-15. **IMPLEMENTED / INTEGRATED_UNVERIFIED**.
Only source, API, caller, packet, SQL-text and diff review was performed. No
build, test, SQL execution, migration/import, extraction, client, server, service
or runtime acceptance ran. No Map-agent commit or push. No schema change; the
existing written, unapplied071/072 prerequisites remain and next is073.

## Exact original completion

`CTObjBase::GetAtkHitType` (`Server/TMapSvr/TObjBase.cpp:2266–2348`) returns
HT_MISS0 for criticalBYTE255 or actual GOHOME2 before the premium, kind21 and
guaranteed-hit tests. The prior native self-cure implementation explicitly
refused these cases. This increment implements the source MISS branch for the
already admitted own-PC positive SA_ONCE HP/MP cure capability, with ordinary,
loop and ACT accepted origins. It does not broaden target/effect selection.

The whole `CTPlayer::Defend` wrapper (`TPlayer.cpp:1884–1970`) still records the
activity tick on MISS. Base Defend (`TObjBase.cpp:767–1306`) skips PerformSkill,
does not calculate or roll a cure, and sets PERFORM_MISS. It clamps against
actual maxima, runs the existing CheckEternalBuff cleanup, sends the original
MISS DEFEND to nearby players, then calls ForceMaintain(TAVOID_SKILL802) at1258.
Source TAVOID is WORD802 (`NetCode.h:37`), not TBLOCK801. Party input remains
parsed-but-unused for these non-damage rows.

The original cure MISS DEFEND body is84bytes: own host/type1, actual critical,
HT_MISS0, actual skill/rank/powers/positions, maintainFALSE, duration0,
performFALSE, and no damage rows. It is **not** a successful cure or a short
failure response. Original client `OnCS_DEFEND_ACK`
(`Client/TClient/CSHandler.cpp:2806–3302`) reads those fields and gates skill
function application on the perform flag. Its separate hit switch at2973 can
enqueue a MISS presentation even with an empty damage list. Actual hosting,
target/UI and rendering behavior still require original-client acceptance.

Source computes `bHPMP` before the later zero-MP erasure. Under the admitted
actual-resource bound, MISS itself produces no cure-derived final HPMP. A
notifying cleanup can retain its own END, conditional HPMP and private STAT;
these must not cause a second synthetic final MISS HPMP.

`ForceMaintain` (`TObjBase.cpp:4397–4494`) returns without an effect when
FindTSkill802 is absent. When maintained insertion is possible, it uses actual
rank-one duration and metadata, self attacker/host in the durable row, then
sends a separate84-byte maintained DEFEND to source neighboring3×3 cells.
Its wire host/type are0/0, attack level1, critical0, HT_NORMAL1, rank1 and
performTRUE; the stored effect's attack level is0. That distinct forced reply
follows the original cure MISS. Source CheckEquipSkill runs after finish even
when its filtered target list is empty.

## Actual template boundary and lifecycle

`ReadProgressEffects` now reads800 and802 independently from the configured
published character chart. Private exists/supported flags distinguish absence
from an existing unsupported definition. Nothing is seeded or inferred from a
skill ID. The supported802 shape is actual positive, nonempty SA_BUFF/
SDT_STATUS/STATUS_NONE0 data, with no aura/posture/hide/dismount/weapon or
duration-increment branch. Actual priority, duration, static, bIsuse and
bEraseAct0..3 are retained. A richer existing802 makes a nonempty MISS
completion unavailable before its mutation; it is never converted to a fake
marker.

Exec0 never matches source HaveSkillData, so802 appends independently instead
of erasing same-ID rows. The existing ordered positive collision primitive
still applies to other admitted status definitions. Retained800/802 clocks and
identities are not reset when a new row is appended. New802 birth/sample/charge
time uses the actual single FINISH observation (`defend_now`), not the last
quest event. A post-append refresh proves the actual statistics remain present
and maxima/resources unchanged; no extra STAT or bars are invented for it.

The complete supported lifecycle includes:

- Finite802-only actors select the real native progression tick. Expiry removes
  each row with the existing END/STAT output; eternal0 does not time-expire.
- bIsuse plus zero template duration selects CheckEternalBuff removal at its
  actual existing ordinary-cast/ACT/Defend callers. The newly forced802 is not
  immediately erased in the same MISS: that occurrence's cleanup already ran.
  A later occurrence can remove it. Periodic zero-MP cleanup is not invented.
- `EraseNativeActionEffects` now covers actual800/802 BEA_ATTACK metadata and
  retains one event/snapshot per removal. ACTION, ordinary SKILLUSE and ACT use
  their existing hooks. Both erase directions exempt actual postures; attack
  alone also skips source IsRecall (type2 with exec other than SER_SKILL12).
  The shared SQL predicate checks actual rows, preserving SER_SKILL's source
  exclusion. Ordinary SKILLUSE has no ACTION `action_supported` veto, so the
  helper must enforce this itself; no recall gameplay is added. LOOP retains
  its source exception. Positive own Defend
  never enters the other-actor negative BEA_DEFEND branch; that flag is metadata
  for a wider, still-unported producer.
- Own SKILLEND keeps the original first attacker/type/skill match. Expiry and
  applicable attack/zero-MP paths may remove several rows in vector order.
- Source silent death cleanup retains actual static rows and removes nonstatic
  rows. Fresh/full-graph corpse hydration with a nonstatic800/802 is explicitly
  unavailable rather than silently altering durable membership. The source
  RecallRevivalErase exception remains800-only;802 does not inherit it.

802 is admitted to the existing supported-stat and equipment-posture graph only
after those actual pure-marker checks. General status/ability802 is OPEN. This
does not give802 the hardcoded800 negative-defense immunity or implement general
negative damage/recall behavior.

## Production transaction, packets and persistence

`FinishSkill` preserves current owner/account/session/connection/epoch,
catalog/072 skill-context, physical-cell owner, inventory/graph/checkpoint and
accepted-cast/one-use finish authority. Critical255 and GOHOME are actual current
hit inputs; they no longer imply fabricated heal or blanket rejection. Empty
target completion performs no hit test, heal or802 force, and keeps its receipt
consumption/equipment cleanup. ACT profile reading no longer imposes mode2;
real ITEMUSE initiation and ACT acceptance preflight an anticipated nonempty
MISS against the actual802 capability before action/debit. Shared FINISH's802
guard applies only to an actual nonempty MISS, so later empty ACT completion
does not need Avoid.

For each accepted occurrence: record activity/current event tick; skip cure and
random draw on MISS; retain applicable cleanup; append the original MISS reply;
then force802 with actual duration. Quest force packets map explicitly to
Finish owner/neighbors audiences; an unexpected World/other audience is refused,
never cast between unrelated enum values. Exact ordered replies and effect/core
state commit together before cache and output. Existing current local-party,
actor/recipient claim/Ready identity and grid/nearby guards apply around awaits.
Busy/stale recipients may miss output without replay. Party vitals follow only
actual own HPMP queue attempts; no heal-only HPMP means no synthetic MISS
MANSTAT. Legitimate cleanup HPMP retains its existing coupling.

Normalized033 ordinal rows and complete transfer/checkpoint vectors preserve
independent802 entries without a new schema or wire field. Sampling, recovery,
load and handoff use the existing effect representation. Source RELEASEMAIN
similarly retains vector multiplicity, while original database saving applies
its separate filters and first-eligible per-skill projection. That earlier
native exact-checkpoint versus original DB-save distinction remains OPEN.
Client maintained visuals remain keyed by skill ID and can disappear on one END
while another server row survives; no invented refresh masks this source quirk.

## Deferred Phase B and OPEN

Exercise ordinary/loop/ACT finishes with current GOHOME2 and critical255, alone
and in basic parties; actual802 absence, supported finite/eternal/static/
nonstatic/zero-MP/erase flags and richer refusal; exact two ordered84-byte
replies with distinct hit/perform/host/attack-level fields; no cure/random/heal
HPMP on MISS; truthful cleanup END/STAT/bars; newly forced zero-MP lifetime;
multiple accepted target occurrences and duplicate802 clocks;802-only idle
expiry; first cancel versus all flagged attack removal, actual recall/SER_SKILL and
posture exemptions and LOOP exception;
death/revival/load; normalized/full-graph checkpoint, crash recovery and handoff;
empty ordinary/loop/ACT finishes;255-row rollback; owner loss, current party,
recipient movement/close/handoff across awaits, and unknown COMMIT/output without
retry. These acceptance cases are written, not executed.

Richer802, general miss/block/damage, arbitrary targets, group effects, negative
Defend, full recall/AI/SoulHP, special modes, original DB-save projection parity,
full bootstrap/AOI and original-executable acceptance remain OPEN. Source wire
has no finish nonce, so the existing FIFO ambiguity among identical outstanding
operations remains. No new deployment, migration application or runtime parity
claim is made.
