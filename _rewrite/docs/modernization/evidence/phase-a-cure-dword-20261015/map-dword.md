# Map cure DWORD arithmetic and actual default rows

Baseline `cde2ded`, 2026-10-15. **IMPLEMENTED / INTEGRATED_UNVERIFIED**.
Only source, declaration, production-caller, SQL-text and diff review was
performed. No build, test, SQL execution, migration/import, extraction, client,
server, service or runtime acceptance ran. No Map-agent commit or push. No new
schema, grant, publication, API signature or CMake entry; written071/072 remain
unexecuted prerequisites and next migration remains073.

## Original DWORD32 and Windows INT projection

The complete `Server/TMapSvr/TSkillTemp.cpp:62–105` performs signed INT
GetValue before selecting any combine mode. In Calculate, a DWORD base mixed
with signed INT GetValue promotes the factor to unsigned32. MULTIPLY performs
DWORD product and subtraction modulo2^32. DIVIDE changes a signed divisor≤0
to1, then performs unsigned quotient and subtraction modulo2^32. PRECENT first
computes the DWORD product modulo2^32, then promotes that product to double
for division by100.0 and truncates to INT; its mixed INT-minus-DWORD subtraction
is again modulo2^32 before assignment to signed INT.

Original `TMapSvr.vcxproj` declares Win32/v141/MachineX86. Windows DWORD is
unsigned32 and INT is signed32; the original MSVC unsigned-to-INT assignment
preserves low32 bits with the high bit as sign. The modern existing
`services/skill_targets.h` already expresses this convention with uint32 and
`std::bit_cast<int32_t>`. Linux `long` is not a substitute for this ABI.

`native_cure::Magnitude` now preserves these exact product/subtraction bits.
It multiplies two uint32 values in uint64, whose full product always fits,
then explicitly projects the low32 bits. A uint32 subtraction is projected
through `std::bit_cast<int32_t>`. Signed factor→DWORD conversion remains
source-defined for negative GetValue too. No unbounded signed multiplication,
implementation-defined unsigned-to-signed cast or saturation is introduced.

PRECENT still divides the actual modulo product by double100 and checks the
INT conversion before performing its source mixed subtraction. That product
is at most UINT32_MAX, so its divided total is at most42,949,672.95 and fits
INT; the explicit finite/range proof remains in code. It is neither a double
base-times-factor product nor a direct percentage heal without subtracting
the base. DIVIDE keeps its source nonpositive-divisor replacement before the
unsigned quotient. DECREASE still refuses INT_MIN negation, whose signed
overflow is outside the supported source domain.

Only a final signed delta in0..INT32_MAX/15 is admitted. A modulo result whose
Windows signed projection is negative remains outside positive own-PC CURE;
large positive results still fail the original all-roll signed variance bound.
The actual sampled resource addition preserves DWORD modulo as described below.
This does not port negative combat or final-death completion, and never replaces
the source operation with saturation or a pre-row resource cap.

## Living resource addition and all-row death boundary

The complete source `PerformSkill` (`TObjBase.cpp:3077–3929`, HP/MP branches
at3553–3568) adds each sampled signed-positive cure/variance to DWORD HP/MP.
That resource addition is modulo2^32. There is no death, clamp or zero-MP cleanup
between these rows. Base Defend clamps and observes changes at1038–1048 only
after all rows, runs CheckEternalBuff around1108, emits DEFEND/conditional HPMP
and reaches its death branch only at1265. Separate target occurrences invoke
separate Defend calls; a later occurrence does not postpone an earlier death.

Native FINISH now adds each actual sampled delta through uint64 and explicitly
retains low32 resource bits. The positive-delta/variance bound makes this wide
sum safe. After every target occurrence's complete ordered row list, it applies
the existing source cap and requires final HP>0 before zero-MP cleanup, staging
that occurrence's output or writing core/finish/checkpoint state. Intermediate
HP0 is permitted when a later HP row restores it. Checking per row would reject
a legitimate source living completion; checking only after all target
occurrences would bypass the source's intervening death branch.

Final MP0 is legitimate and reaches the existing source zero-MP effect cleanup
in its original order. The admitted all-CURE/stat-neutral posture/progression
graph leaves maxima independent of these intermediate resource values and does
not add a hidden mid-row effect/death producer. Lower but living HP/MP after
wrap is committed and reported truthfully, without a fake full heal or refund.
Final post-cap HP0 is explicitly unavailable because this FINISH origin's full
source OnDie/last-hit lifecycle is not ported. Its transaction fails atomically;
an already committed ACT item debit is not refunded or replayed. All previously
staged packets from this transaction remain unexposed before a successful commit.

## Source default GetValue and Calculate branches

The same full source GetValue returns actual0 after its switch for an unknown
Calc BYTE. Calculate initializes `nValue=0`, always evaluates GetValue first,
then leaves nValue0 for an unknown combine BYTE. These are real branches on a
nonempty CURE row, not a synthesized effect replacing absent or unsupported
skill data.

`ReadActItemSkill` now preserves actual Calc/combine BYTE metadata without the
previous0..3/1..5 whitelist. Every other effect/actor boundary remains: actual
nonempty positive own-PC SA_ONCE SDT_CURE HP/MP rows, current supported graph,
skill/template checks, hardcoded exclusions, pinned catalog and072 skill-context
proof. NULL fields still fail their actual numeric reads. Empty or richer skill
data is not transformed into a zero row.

`SignedValue` handles actual Calc0/1/2/3 and returns0 for the source unknown-Calc
branch. The selected combine still runs on that0, so the final result need not
be zero: MULTIPLY/PRECENT may produce source modulo subtraction bits. For an
unknown combine, Magnitude still calls SignedValue first and only then returns
its initialized0. An invalid Calc2 therefore cannot bypass its checked pow/INT
conversion merely by pairing it with unknown combine metadata.

The ACT-specific growth query remains conditional on an actual Calc2 row and
uses actual outgoing metadata. An unknown Calc is not interpreted as Calc2.
Existing upstream `ReadFormula` dependency validation remains unchanged.
Normal FINISH still evaluates every actual row and takes its independent RNG
draw even when the final delta is0, then emits its real original DEFEND and
performs source cleanup/one-use receipt completion. Conditional HPMP remains
conditional on actual changed resources. No fake success packet, empty-target
shortcut or row omission was introduced for defaults.

## Unchanged production authority and consumers

The helper signature and existing three route integrations are unchanged:
ITEMUSE type25 initiation and ACT acceptance call the shared preflight against
current actual maxHP/maxMP, and all admitted native ordinary/loop/ACT FINISH
rows call the same helper at the current trigger rank and maxima. Anticipated
MISS checks only actual802 capability before numeric work. Actual MISS and
empty FINISH skip GetValue/combine/RNG as before. Required metadata proof is
separate from arithmetic evaluation.

Source current trigger rank/baseline critical, outgoing type/powers, original
ACT ACK/FINISH DEFEND shapes, item/reuse/activity/erase ordering, per-row output
and party vitals remain unchanged. Exact owner/account/session/connection/epoch,
physical-cell, catalog, inventory/checkpoint/graph and one-use accepted/finish
authority still fences each committed mutation. Existing current local-party
and actor/recipient projection guards remain around awaited output. Positive
BIGINT receipt keys and signed INTEGER identity domains are unchanged. No new
ordinary cost, cooldown, wire field or membership claim is added. Post-acceptance
arithmetic scope failure has no new refund/replay policy; uncertain writes are
not replayed.

## Static review and deferred acceptance

Read full original GetValue/Calculate and existing native bit-projection pattern,
reviewed the helper's unsigned conversions/intermediate widths/bit_cast and
final guard, and checked actual profile metadata preservation plus all existing
preflight/FINISH consumers. No generic `skill_effect.h` engine edit or historical
evidence rewrite was made. This static source/type review is not compilation or
execution verification.
Read the complete source row loop and its post-loop clamp/output/death boundary;
reviewed the new addition and living check placement in the actual FINISH loop.

Phase B must verify real original-client and PostgreSQL routes, subject to
actual supported actor/stat/publication authority:

- MULTIPLY product wrap, unsigned subtraction and signed-bit projection.
  Source arithmetic vectors include base0x80000001/value3→delta2 and
  base0xfffffff0/value0→delta16; these are source math examples, not evidence
  that an actual published actor can currently reach those maxima.
- DIVIDE nonpositive/1/large divisors, quotient-before-modulo subtraction;
  source base0xfffffff0/value65535 gives final delta65552.
- PRECENT modulo product before double division/INT truncation and mixed
  subtraction, including negative signed factors, zero base, high-bit final
  negatives and final variance-bound refusals.
- Unknown Calc with each known combine, unknown combine with valid Calc0..3,
  invalid Calc2 plus unknown combine, actual zero/default rows retaining RNG,
  DEFEND, conditional bars, cleanup and receipt completion.
- Current rank/max changes after ACT, normal preflight before debit, the same
  arithmetic profile through actual MISS/empty without math, duplicate finish,
  write-unknown failure, save/transfer and current output/party audiences.
- Resource modulo yielding lower but living HP/MP, intermediate HP0 restored by
  a later HP row, final post-cap HP0 refusal, MP0 source cleanup and repeated
  target occurrences respecting the per-Defend living check. Validate actual
  packet order, retained item debit on unsupported completion and whole finish
  transaction atomicity without exposing staged packets.

Negative final effects, signed INT_MIN negation, unsafe final variance,
final-death completion, richer/non-cure/foreign/linked/repeated effects,
hardcoded IDs128/229/3604 and richer802 remain outside this positive capability.
Source CRT/libm exact truncation boundaries, original Map bootstrap and native
ordinal checkpoint versus source DB-only save projection remain separately
OPEN. No new execution-verified full parity is claimed.
