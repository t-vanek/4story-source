# Map native positive cure combine modes

Baseline `09317c7`, 2026-10-15. **IMPLEMENTED / INTEGRATED_UNVERIFIED**.
Only source, declaration, production-caller, SQL-text and diff review was
performed. No build, test, SQL execution, migration/import, extraction, client,
server, service or runtime acceptance ran. No Map-agent commit or push. No new
schema, grant, publication, public API or CMake entry; written071/072 remain
unexecuted prerequisites and next migration remains073.

## Source arithmetic before positive-cure admission

`Server/TMapSvr/TSkillTemp.cpp:62–107` first computes signed INT `GetValue`
using actual Calc0..3 and current trigger rank, then combines that result with
the supplied DWORD base. For HP/MP CURE, `TObjBase.cpp:3555–3566` supplies actual
`GetMaxHP()`/`GetMaxMP()`; the base is not the current damaged resource. It then
adds the resulting delta plus each row's independent signed-INT variance.
The source SVI enum starts at1 (`NetCode.h:1521`, native `domain/skill_data.h`).

The new `native_cure::SignedValue` retains the already checked exponential
FLOAT/double/INT conversion and existing Calc0/1/3 expressions. It permits signed
negative values before combine. A positive-cure guard at this earlier stage
would wrongly refuse a DECREASE of a negative Calc3 or rank0 Calc1 value.
Rank0 remains actual0, without BYTE underflow or ACK-rank1 substitution.

`native_cure::Magnitude` now takes the actual maximum as its fourth argument
and implements all five source combine modes:

| SVI | Source operation and supported boundary |
| --- | --- |
| INCREASE1 | Final signed delta is GetValue. |
| DECREASE2 | Negate signed GetValue; INT_MIN is refused before source negation would overflow. |
| MULTIPLY3 | Convert factor to DWORD, multiply DWORD base by that factor, subtract base. Product and subtraction must not wrap. |
| DIVIDE4 | Replace signed divisor≤0 with1, perform unsigned base/divisor, subtract base. Subtraction must not wrap. |
| PRECENT5 | Convert factor to DWORD and multiply in DWORD domain before division by double100; truncate that total to INT, then source mixed signed/unsigned subtraction of base. Product and subtraction must not wrap. |

Signed→DWORD conversion is source-defined and implemented, including negative
factors. It is distinct from multiplication/subtraction wrap, which remains
OPEN. The helper computes a uint64 product first and requires it≤UINT32_MAX,
then requires the projected total≥base before subtraction. The maximum uint32
product fits uint64, so these checks themselves cannot overflow. For percent,
the checked DWORD product is divided by100.0 only afterwards; it is not
`double(base) * percentage`. The explicit finite/INT range guard precedes the
percent INT cast. The source's final mixed subtraction is reproduced in its
nonwrapping positive domain.

For illustration of the source expressions, base1000 and factor150 in PRECENT
give a total1500 and delta500, not a heal1500. A signed factor-1 converts to
UINT32_MAX: with base1 its product does not wrap and its percent delta is
42,949,671. This is a supported conversion case, not an arithmetic-wrap policy.
Base0 gives0 in unsigned combines even for a negative factor, but only after
GetValue has really run. A DIVIDE divisor≤0 becomes1 and gives delta0. A
positive divisor above1 with a positive base usually makes the source unsigned
subtraction wrap; that path is explicitly unavailable, not rewritten as a
zero heal.

Only the final combined delta must be in0..INT32_MAX/15. This proves every
original `delta * (rand()%16)` signed product is representable before RNG.
It does not reject a larger intermediate signed GetValue that a valid combine
reduces into this final range. Source INT_MIN negation, nonfinite/out-of-range
Calc2 conversion, wrapped product/subtraction and unknown combine modes are
refused without a clamp or fabricated output. The existing actual sampled
variance, DWORD resource-addition guard and later resource cap remain intact.
Zero-valued rows still take their independent RNG draw in real FINISH.
SignedValue explicitly accepts Calc0..3 only; unknown Calc values are refused
instead of reaching the generic source GetValue default0.

## Actual connected production callers

`ReadActItemSkill` now admits actual bInc1..5 and preserves the field in every
supported positive own-PC SA_ONCE HP/MP CURE row. Existing Calc0..3, template,
current-stat/graph, metadata and pinned skill-context gates remain. Unknown
combine values and unsupported effects are unavailable before ACT mutation.

Both real ITEMUSE type25 initiation and ACT acceptance refresh current actual
equipment/statistics and call `PreflightActItemCure`. For anticipated normal
completion it passes each row's current maxHP or maxMP and current trigger rank
to the same checked helper before action/item hooks or debit. A predicted MISS
from mode2 or actual trigger critical255 instead checks actual802 capability
and returns without GetValue/combine/RNG. Required metadata validation still
runs independently of numeric evaluation.

Shared native FINISH now uses that helper for **every admitted combine**, with
no generic unchecked Calculate fallback. This also explicitly tightens existing
ordinary/loop positive cure completion: it rejects source wrapped arithmetic
and unknown Calc>3/combine0/>5 instead of accepting an unchecked result or
source's default no-op. The generic `skill_effect.h` engine and prototype callers are
unchanged. This increment does not claim all source wrap/default behavior.

FINISH refreshes current actor/rank/stat authority, correlates one actual
accepted receipt, and evaluates rows only for non-MISS actual targets. Each
row uses the current maximum, and the admitted all-CURE rows do not change
maxima within PerformSkill. Later source cleanup can update context before a
subsequent target occurrence. Empty completion and actual MISS perform no
numeric evaluation and retain their current receipt/cleanup/802 semantics.
Preflight cannot freeze a later rank or maximum: a post-acceptance change that
enters an unsupported arithmetic domain is refused at FINISH, without a refund,
replacement success or replay policy.

The current supported graph also bounds max-stat stability: admitted posture
ability execs7/8/9/16 affect PAP/PDP/LAP/MDP, and admitted progression/800/802
status rows do not change CON/MEN/MAXHP/MAXMP. Their attack/zero-MP cleanup does
not create an unproved maximum-resource drift in this cure capability.

Actual outgoing type/powers versus trigger baseline critical, source item and
delay accounting, recovery clocks, attack/zero-MP erase ordering, original wire
ACK/DEFEND bodies and output audiences remain unchanged. All mutations still
commit through the existing current owner/account/session/connection/epoch,
physical-cell, inventory/checkpoint/graph and one-use accepted/finish authority.
Signed INTEGER identity and positive BIGINT receipt-key domains remain distinct.
Current local-party and actor/recipient fences still guard output. No new
ordinary cost, cooldown, wire field, membership assertion or successful combat
dependency was added, and uncertain writes are not replayed.

## Static review and deferred acceptance

Read source GetValue/Calculate/PerformSkill and native field/enum types. Reviewed
the signed helper split, all multiplication/division/subtraction/cast guards,
both preflight paths through their shared member and the actual FINISH loop.
The fourth-argument maximum is supplied by current native state, never input
coordinates or party metadata. Static review is not compilation or acceptance.

Phase B must exercise the original client and real PostgreSQL authority with:

- Actual Calc0..3 × SVI1..5 rows, including negative Calc3 and rank0 Calc1
  values that become positive DECREASE deltas, and INT_MIN negation refusal.
- Multiplier identity/zero, DIVIDE≤0/1 and base0, positive-divisor subtraction
  refusal, and exact no-wrap versus product/subtraction-wrap boundaries.
- PRECENT100/150, fractional truncation, negative factor DWORD conversion and
  base0, proving DWORD multiplication precedes division and subtraction.
- Final0 and variance limits, intermediate values above the final variance
  bound, checked exponential extremes, each ordered row's independent RNG and
  actual resource-addition overflow refusal without saturation.
- Current rank/max changes after ACT, normal preflight before debit, the same
  invalid numeric profile through actual MISS/empty without computation,
  missing/supported/richer802, duplicate finish and unknown-commit recovery.
- Item/delay accounting, exact current outputs, live party vitals and
  checkpoint/transfer persistence without a second mutation or replay.

Arithmetic DWORD product/subtraction/resource wrap, negative final effects,
unknown combine default no-ops, hardcoded IDs128/229/3604, foreign/linked/repeat
effects and richer802 retain explicit OPEN boundaries. Source CRT/libm versus
target `std::pow` exact rounding/truncation equivalence, original Map bootstrap
and native ordinal persistence versus source DB-only save projection remain
separately OPEN. No execution-verified full parity is claimed.
