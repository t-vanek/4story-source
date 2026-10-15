# Map action-item cure with a learned trigger

Baseline `5da1bf0`, 2026-10-15. **IMPLEMENTED / INTEGRATED_UNVERIFIED**.
Only source, declaration, production-caller, SQL-text and diff review was
performed. No build, test, SQL execution, migration/import, extraction, client,
server, service or runtime acceptance ran. No Map-agent commit or push. No new
schema, wire field or migration; written071/072 remain prerequisites and the
next migration remains073.

## Source rank, critical and instance ordering

Original `OnCS_ACTITEMSUSE_REQ` (`Server/TMapSvr/CSHandler.cpp:19875–20196`)
accepts the item skill without requiring that skill or its trigger to be
learned. It applies its existing item/reuse/activity hooks. The successful
`CS_SKILLUSE_ACK` still carries rank1, critical0 and zero attack powers. These
presentation fields do not determine the later effect rank or critical.

The whole source `OnCS_FINISHSKILL_ACK` starts at20197. Its kind21 branch checks
the original used-skill entry, and the attack projection at20380 looks up
`pTemp->m_wTriggerID` through `FindTSkill`. The actual trigger object's current
`m_bLevel` becomes the effect rank at20394, and critical is sampled at20395;
absence gives rank0/critical0. The lookup does not
test whether that level is nonzero. Source DB skill query
(`DBAccess.h:1965–1991`), load sender (`SSHandler.cpp:3644–3673`) and receiver
(`SSHandler.cpp:4878–4923`) preserve a present rank0 object.
`TObjBase.cpp:2486–2493` returns the stored pointer by ID only.

`GetCritical` (`TObjBase.cpp:2255–2265`) reads the **trigger template's**
`GetAttackType`: SAT_MAGIC3 selects actual baseline magic critical; every
other type, including NONE0, selects physical critical. For kind21, this is
computed before installing an instance skill. It must not use the hydrated
trigger `attack_profile.critical`, whose separate ordinary-cast projection can
include that trigger's instance abilities.

The later PC power projection uses the **outgoing item skill's** attack type
and `IsLongAttack`, independently of the trigger's critical type. Only a
self-trigger installs that trigger as the instance while reading powers. Every
row in the admitted outgoing profile is positive SA_ONCE SDT_CURE HP/MP, so it
has no SDT_ABILITY and cannot alter those powers or attack level. Its
`IsLongAttack` is false (`TSkillTemp.cpp:197–209`); a CURE row with LONG attr
does not become an outgoing ranged ability. A different trigger's abilities
are never applied as this cure's instance. Source `GetAttackType`
(`TSkillTemp.cpp:172–195`) still supplies the outgoing physical/magic choice.
For outgoing attack level, physical is selected only for PHYSIC1; NONE0 uses
magic level. This is distinct from the trigger critical's magic3-only rule.

## Actual rank-dependent cure values

Source `TSkillTemp.cpp:62–107` evaluates WORD value/increment and BYTE rank
through promoted signed INT arithmetic. The admitted positive SVI_INCREASE
HP/MP rows now retain their actual `bCalc` and `wValueInc` for these forms:

| bCalc | Source GetValue at the current trigger rank r |
| --- | --- |
| 0 | value |
| 1 | value + (r - 1) × increment |
| 3 | value - (r - 1) × increment |

Rank0 is not replaced by1 or wrapped to BYTE255. Calc1 at rank0 is
value-minus-increment; Calc3 at rank0 is value-plus-increment. With source
WORD/BYTE inputs these forms fit INT32: maximum positive is16,711,425 and the
largest positive variance product by15 is250,671,375. The existing shared
`skill_effect::GetValue`/`Calculate` already preserves these expressions.
Source `PerformSkill` at `TObjBase.cpp:3555–3566` adds each HP/MP row's value
plus its independent0..15-percent variance, before Defend's resource cap.

The new shared `PreflightActItemCure` is called only by actual type25 initiation
and ACT acceptance, after current profile derivation and before action/item
hooks or debit. An anticipated MISS checks only actual802 capability and skips
numeric cure validation, just as source skips Calculate. An anticipated normal
cure requires every current-rank value to be nonnegative; negative values are
explicitly unsupported rather than clamped to0 or converted to rank1. The
profile reader itself has no magnitude guard, so later MISS or empty FINISH is
not refused for a calculation it does not perform. Existing FINISH Calculate
and negative-result checks run at actual current rank only for non-MISS target
occurrences. Resource-addition DWORD overflow remains independently refused.
Calc2 growth/floating-point semantics and other increment modes remain OPEN;
the ordinary skill engine is unchanged.

## Native production path

`ReadActItemSkill` removes only the blanket learned-trigger refusal. After the
existing actor, full-graph, pure-cure, pinned catalog and072 skill-context gates:

- Missing actual trigger row retains rank0 and critical0.
- A present row, including rank0, requires its current hydrated template and
  attack profile. Rank, actor level and aftermath must agree with that current
  projection. Missing or stale metadata is unavailable; it is not treated as an
  unlearned trigger or a zero critical.
- The row supplies current rank. The trigger profile supplies only attack-type
  metadata; critical comes from current baseline `payload.statistics` with the
  exact magic3/otherwise-physical selection.
- Outgoing power/type/levels remain the existing own-PC CURE projection. No
  trigger MP/HP cost, cooldown, charge, item requirement, action capability or
  ordinary-cast eligibility is consulted or charged.

All three production callers use this same projection: type25 ITEMUSE
initiation, ACT acceptance and accepted ACT FINISH. Each already refreshes
actual equipment/statistics under the existing fenced transaction. Init and ACT
use `PreflightActItemCure` for an anticipated MISS from **mode2 or the selected
trigger critical255**, or for actual nonnegative normal magnitudes, before
ACTION/debit. Missing802 keeps the
source no-effect branch; an existing richer definition remains unavailable.

FINISH derives the trigger again from the current refreshed state. Rank and
critical are not frozen in the rank1 ACT acknowledgement or a new receipt
field. A legitimate rank/equipment/effect change between acceptance and FINISH
therefore uses its current source projection. If that change enters an
unsupported scope, FINISH is refused through the existing close/uncertain-write
handling; no refund, replacement success or replay is fabricated. Empty target
completion still skips Hit/Defend/802 and preserves receipt consumption and
equipment cleanup.

The accepted origin, actual ACT route, exact request/acknowledgement bytes,
one-use FINISH receipt, current owner/account/session/connection/epoch, catalog
and physical-cell proof remain unchanged. Atomic item/effect/core/checkpoint
writes retain signed INTEGER identity versus positive BIGINT receipt-key
domains. Existing recovery clocks, source erase ordering, effect ordinals and
unknown-commit handling remain unchanged. Existing native current-party and
actor/recipient projection fences still guard output; request party WORD
grants no authority.

For a nonempty occurrence, ordinary cure or actual MISS uses the derived rank
and critical in the original84-byte DEFEND. Cure magnitude uses the admitted
actual Calc0/1/3 value plus the existing source per-row variance; no percentage,
negative or arbitrary trigger effect is inferred.
MISS retains no heal/random draw, source cleanup, hit0/perform0 DEFEND and then
actual supported802 force, including its existing independent lifecycle.

## Static review and deferred acceptance

Read all three `ReadActItemSkill` callers, actual `SkillAttackProfile` and
statistics declarations, baseline-before-instance derivation, whole source ACT
and FINISH, source lookup/critical/type/instance helpers and existing packet
encoder. The new private static preflight has two production callers and does
not add a public service API or CMake entry. Static diff checks are not runtime
verification.

Phase B must exercise the original client through real ITEMUSE→ACT→FINISH:

- Missing trigger versus present rank0; same trigger and different trigger;
  learned ranks above1 with ACK rank1 but actual DEFEND rank.
- Magical trigger with physical outgoing cure, physical/NONE trigger with
  magical outgoing cure, and a trigger whose instance-adjusted critical differs
  from baseline. Only the trigger's attack-type metadata selects critical.
- Legitimate trigger-rank/stat changes after ACT but before FINISH, including
  critical255 and a transition to unsupported actual802. GOHOME2 and missing,
  supported pure-marker or richer802 retain their distinct outcomes.
- Empty accepted-target completion, duplicate/stale FINISH, owner/claim loss,
  write-unknown failure, item/delay accounting, output audiences and live party
  vitals without ordinary costs or a second accepted receipt.
- Calc1 and Calc3 at ranks0/1/above1, ordered HP/MP rows with real increments,
  zero values, negative normal preflight before ACTION/debit, and the same
  negative-valued profile through MISS or empty FINISH without Calculate/RNG.

Hardcoded outgoing IDs128/229/3604, Calc2/percent or non-cure effects, foreign
targets, linked/repeated effects, guild/recall/companion contexts and general
combat remain OPEN under their existing capability guards. Native ordinal
effect persistence versus original DB-only first-eligible save projection and
original Map admission/bootstrap remain separately OPEN. No new successful
coverage is claimed for these dependencies.
