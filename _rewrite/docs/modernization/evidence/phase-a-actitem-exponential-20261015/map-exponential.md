# Map action-item exponential cure

Baseline `8ed424c`, 2026-10-15. **IMPLEMENTED / INTEGRATED_UNVERIFIED**.
Only source, declaration, production-caller, SQL-text and diff review was
performed. No build, test, SQL execution, migration/import, extraction, client,
server, service or runtime acceptance ran. No Map-agent commit or push. No new
schema, wire field, public API or CMake entry; written071/072 remain unexecuted
prerequisites and next migration remains073.

## Original numeric and metadata contract

`CTSkillTemp::GetValue` (`Server/TMapSvr/TSkillTemp.cpp:62–76`) defines Calc2 as
the INT conversion of `value * pow(f1stRateX, exponent) / 100`. Rank0 selects
exponent0. Otherwise the exponent is the outgoing template's `bStartLevel +
(rank - 1) * bNextLevel`; all three source fields are BYTE, so its maximum is
65,025. This is not the ACT acknowledgement's presentation rank1, and growth
metadata does not come from the learned trigger template.

Original `TMapSvr.cpp:2655–2674` reads `TFORMULACHART` and retains FTYPE_1ST
`fRateX`. At2705/2707 it copies outgoing `bLevel`/`bNextLevel`, and at2730 stamps
the coefficient into each skill template. FTYPE_1ST is34. Source
`DBAccess.h:81–104` stores the coefficient in C++ FLOAT, while the recovered SQL
float53 column and its existing PostgreSQL publication use double precision.
The native reader therefore requires the actual published row, checks finite
and FLOAT range before narrowing, then retains that source FLOAT value.
No default growth value is substituted.

The earlier `RefreshEquipment` dependency calls `DeriveStats` and its
`postgresql_map_load.cpp::ReadFormula` before the ACT-specific reader. That
shared reader now checks both actual double `fRateX` and `fRateY` for finite
FLOAT range before either cast. This explicitly tightens every existing
`ReadFormula` caller, including non-ACT statistics, so an upstream unrepresentable
coefficient cannot be narrowed before the later ACT check. Representable source
FLOAT rounding, including underflow to zero, remains unchanged; no coefficient
is clamped or defaulted. This dependency protection does not evaluate Calc2
`pow` on MISS or empty FINISH.

`Calculate` (`TSkillTemp.cpp:77–107`) with SVI_INCREASE returns the computed
magnitude. `PerformSkill` (`TObjBase.cpp:3555–3566`) adds each HP/MP row's
independent `nValue + nValue * (rand()%16) / 100`; its multiplication is signed
INT. Calc0/1/3 WORD/BYTE inputs already fit that product, whereas a valid Calc2
INT conversion alone could exceed the source's variance multiplication range.
The original resource addition is DWORD and happens before Defend's cap.

## Checked native cure calculation

The new `services/native_cure.h::Magnitude` is scoped to positive
SVI_INCREASE cure magnitudes. It does not modify the generic skill engine.
For Calc2 it promotes the actual stored FLOAT to double, uses the exact rank0
exponent rule and computes `pow`, then the WORD product and division by100.
All three intermediate results must be finite, and the final raw result must
be within INT32 bounds before the cast. Conversion truncates toward zero.
Negative coefficients are not arbitrarily forbidden: an even exponent can
produce a positive value; a negative fraction such as -0.5 converts to0 just
as source does. Negative integral results remain outside the supported
positive-cure capability. Zero and floating-point underflow to zero are valid.

After conversion, every admitted magnitude must be nonnegative and at most
`INT32_MAX / 15` (143,165,576), making all possible original variance rolls
representable. That conservative boundary refuses a value whose small sampled
roll might happen to fit but whose other rolls would invoke signed overflow.
It neither saturates nor invents defined behavior for original undefined
floating-to-INT or signed multiplication cases.

Calc0/1/3 retain their existing `skill_effect::GetValue` expressions and now
share the same cure-only post-conversion check. The generic engine and the
shared FINISH's existing ordinary non-INCREASE combine path remain unchanged;
those combine modes are not admitted for ACT. The actual sampled positive
variance and each DWORD resource addition still use the existing FINISH check
before mutation/commit. There is no full heal or hidden overflow clamp.
The same SVI_INCREASE FINISH helper also hardens existing ordinary/loop cure
completion: it refuses unsupported growth conversion or signed variance range
before RNG. This is an explicit shared native cure boundary, not a claim that
all generic ordinary combine/formula behavior has been reimplemented.

## Connected production path and authority

`ReadActItemSkill` admits Calc0..3 only on the existing pure positive own-PC
SA_ONCE SDT_CURE HP/MP, SVI_INCREASE profile. It preserves actual value,
increment and calc fields. If any outgoing row uses Calc2, it reads the actual
pinned formula34 and outgoing `bLevel`/`bNextLevel`. Missing, NULL, nonfinite or
non-FLOAT-representable metadata makes this capability unavailable; it is not
converted to an empty effect or a guessed coefficient. The existing
`CheckCatalogs`/verified character publication covers formula/skill/data rows;
the separate072 guild-skill context proof remains required. No SQL publication
or runtime grant changes are needed.

Both real ITEMUSE type25 initiation and ACT acceptance already refresh current
equipment/statistics and call `PreflightActItemCure`. Normal preflight now uses
the shared checked helper at the actual current trigger rank before action or
item hooks/debit. Anticipated MISS from actual mode2 or trigger critical255
checks only the actual802 capability and returns before numeric evaluation.
Reading required metadata is distinct from evaluating `pow`.

At accepted FINISH, the existing fenced transaction refreshes the actual actor
and rederives current trigger rank/critical. For actual non-MISS target
occurrences, SVI_INCREASE calls the same checked helper before RNG or resource
addition. Thus preflight cannot validate one exponential formula while FINISH
uses an unchecked INT cast. MISS and empty target completion never invoke the
helper, Calculate or cure RNG, and retain their existing cleanup/receipt flow.
The normal magnitude may change after acceptance because source samples the
trigger rank at FINISH; a later unsupported result is refused without a new
success, refund or replay policy.

The original rank1/critical0/zero-power ACT acknowledgement and actual-rank
84-byte DEFEND are unchanged. Outgoing skill type still selects its powers and
attack level; trigger type selects baseline critical independently. Source
item consumption, strict delay, activity/erase ordering, actual physical-cell
proof and current owner/account/session/connection/epoch/inventory/checkpoint
authority remain intact. Accepted origin, actual route and one-use finish
receipt remain unchanged. No receipt field, cooldown/cost or transfer DTO was
added. Signed INTEGER identities and positive BIGINT receipt keys retain their
separate domains. Native current-party and actor/recipient fences still guard
ordered output; uncertain writes are not replayed.

## Deferred acceptance and open boundaries

Statically reviewed the new helper's types/includes and all production calls:
two preflight callers via their shared member and the actual SVI_INCREASE
FINISH loop. Formula reader and source field types, rank/current-stat metadata,
MISS/empty branches and existing DWORD addition were read directly. Static
review is not evidence of compilation, database execution or client behavior.
Source CRT `pow` versus the target platform's `std::pow` can differ at final
rounding/truncation boundaries. Exact cross-runtime numeric equivalence remains
OPEN for original-client/runtime acceptance; expression and type review alone
does not prove identical libm results.

Phase B must run the original client through real initiation→ACT→FINISH with
actual pinned data and cover:

- Missing trigger and present rank0 use exponent0, while rank1 and later ranks
  use outgoing start/next fields, even when trigger growth fields differ.
- Formula SQL-double→FLOAT rounding, finite positive/zero/negative coefficients,
  odd/even exponent signs, fractional truncation including negative-to-zero,
  underflow, `pow`/product overflow, INT boundary and variance-domain refusal.
- Upstream `RefreshEquipment`/`ReadFormula` rejection of nonfinite or out-of-FLOAT
  `fRateX` and `fRateY`, including non-ACT callers, before any narrowing cast.
- Ordered HP/MP rows, resource caps and actual DWORD-addition refusal, current
  rank/stat changes after ACT and independent per-row variance.
- The same numerically invalid-but-readable profile through actual MISS or
  empty FINISH without cure evaluation, actual missing/supported/richer802,
  duplicate/stale finish and unknown commit handling.
- Item/delay accounting, exact wire rank/critical/powers, current audiences,
  party vitals, save/reconnect and transfer authority without ordinary costs.

Hardcoded IDs128/229/3604, other combine modes, negative/non-cure/foreign-target
effects, linked/repeated effects, richer802 and unported guild/companion/recall
contexts retain their existing OPEN boundaries. General ordinary formulas and
other combine modes were not ported by this increment. Original Map bootstrap,
and native exact ordinal persistence versus source DB-only save projection,
remain separately OPEN. No new execution-verified parity is claimed.
