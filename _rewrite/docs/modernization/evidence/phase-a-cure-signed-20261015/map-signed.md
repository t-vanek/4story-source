# Map signed numeric deltas in positive-template living CURE

Baseline `e2e4784`, 2026-10-15. **IMPLEMENTED / INTEGRATED_UNVERIFIED**.
Only source, declaration, production-caller, SQL-text and diff review was
performed. No build, test, SQL execution, migration/import, extraction, client,
server, service or runtime acceptance ran. No Map-agent commit or push. No new
schema, grant, publication or CMake entry; written071/072 remain unexecuted
prerequisites and next migration remains073.

## Template classification and signed source CURE

Original `Server/TMapSvr/TSkillTemp.cpp:31–34` defines IsNegative as the inverse
of template `bPositive % 2`. The complete IsDamageType at118 requires an
SDT_ABILITY row with damage attributes/execs. A numerically negative HP/MP CURE
delta does not change either test: the admitted odd-bPositive, all-SA_ONCE
SDT_CURE own-PC template remains outside the negative-template damage path.
No negative foreign-target erase, damage OnDamage, new battle-mode transition
or monster damage/aggro producer is implied by the sign alone.

Source GetValue/Calculate (`TSkillTemp.cpp:62–105`) can produce signed results
through Calc0..3, DECREASE or the DWORD combine-bit projection. In PerformSkill
(`TObjBase.cpp:3553–3568`), each real HP/MP row computes signed INT
`value + value * (rand()%16) / 100` and adds the result to its DWORD resource.
C++ integer division truncates toward zero for negative values. For example,
value-1 with roll15 produces -1, while value-7 with roll15 produces -8. These
are arithmetic explanations, not executed tests or seeded gameplay values.

The source performs no death/clamp between CURE rows. Defend applies the cap
after all rows at1038–1048, then zero-MP cleanup and original output, and only
later reaches its final death branch at1265. Existing supported graph proof
keeps maxHP/maxMP independent of intermediate HP/MP: all outgoing rows are
CURE without ABILITY, posture execs7/8/9/16 are not max-stat effects, progression
and800/802 markers are stat-neutral, and unported guild/recall/companion graphs
are unavailable. A later HP row can therefore restore intermediate HP0 before
the source death boundary.

## Native checked signed delta and variance

The internal helper is renamed `native_cure::Delta`; both direct production
consumers use the new name. Signed GetValue, actual Calc2 metadata and checked
pow/INT conversion, source DWORD product/subtraction, `bit_cast<int32_t>`
projection, DECREASE INT_MIN refusal and actual default branches are unchanged.
Unknown combine still evaluates checked GetValue first. The generic skill
engine remains unchanged.

After combine, the result must lie in the symmetric interval
`[-INT32_MAX/15, +INT32_MAX/15]` (±143,165,576). Direct comparison avoids an
unsafe absolute-value operation on INT_MIN. This bound proves every original
signed `value * roll` product fits INT, and its divided term plus value also
fits INT. Values beyond the limit are refused; they are not clamped or
converted to a smaller effect.

FINISH now computes variance in signed int64 with signed multiplication and
division by100. It does not reinterpret a negative value as uint64 before the
variance calculation and does not use floor division. Only the completed
signed sample is converted to uint32, as source DWORD addition requires. The
existing wide addition followed by low32 projection then retains exact source
resource bits. Negative sample underflow may wrap to a high resource followed
by the source final cap; the native path preserves that operation rather than
inventing a different negative-effect rule or pre-row saturation.

The existing living boundary remains: each target's entire ordered row list
runs, then HP/MP are capped and final HP must be>0 before zero-MP cleanup,
staging that target's output or writing finish/core/checkpoint state. Intermediate
HP0 is permitted; final HP0 refuses the complete transaction atomically because
this origin's full OnDie/last-hit lifecycle is still outside the capability.
Repeated target occurrences retain that check per Defend call. Final MP0 uses
the existing zero-MP effect cleanup. Nothing adds a new refund/replay policy for
an ACT whose item debit already committed; staged packets remain unexposed on
failure and uncertain writes are not replayed.

## Actual production consumers and retained authority

ITEMUSE type25 initiation and ACT acceptance already refresh actual current
statistics and use `PreflightActItemCure`. Normal preflight now admits signed
final Delta within the same safe range at current trigger rank and maximum,
instead of refusing a valid negative result before action/item hooks or debit.
The actual post-acceptance rank/max/roll may differ at FINISH; unsupported
completion still fails through the existing authority path.

Every admitted ordinary/loop/ACT native FINISH row uses the same Delta helper.
Actual MISS and empty-target completion still skip GetValue/combine/RNG;
anticipated mode2 or trigger critical255 preflight still checks only actual802
capability before numeric work. Default zero rows still perform their real
independent RNG and completion when PerformSkill runs. Required metadata and
current actor/effect capability proof are unchanged.

Source trigger current rank and baseline critical, outgoing power/type,
original ACK/DEFEND/conditional HPMP packet fields, source current recovery
clock, item/reuse/erase ordering and live party vitals remain unchanged. No
damage entry or alternative skill classification is inserted for a negative
numeric CURE. Current owner/account/session/connection/epoch, physical-cell,
catalog/072 skill context, inventory/checkpoint/graph, accepted origin/actual
route and one-use finish receipt authority still fences each mutation. Signed
INTEGER identity and positive BIGINT receipt-key domains remain distinct.
Current local-party and actor/recipient projection checks still guard awaited
output. No wire field, ordinary cost/cooldown or SQL membership claim is added.

## Static review and deferred acceptance

Read original template classification, complete GetValue/Calculate, CURE row
arithmetic and post-row Defend clamp/death ordering. Reviewed helper rename,
symmetric comparison, both direct call sites, signed variance types, uint32
sample conversion and per-target living check placement. No historical evidence,
legacy source, client, protocol, schema or backup was edited. Static source/type
review is not compilation, execution or original-client acceptance.

Phase B must exercise real original-client and PostgreSQL authority with:

- Actual signed final deltas through all Calc/combine and default branches,
  current trigger rank/max changes and preflight before action/item debit.
- Signed roll truncation for small negative values, ±variance limits and
  just-outside refusal, including INT_MIN negation/invalid Calc2 refusal.
- Lower living resources, DWORD underflow followed by cap, mixed signed HP/MP
  rows, intermediate HP0 restored later, final HP0 atomic refusal per target
  and final MP0 source cleanup/order.
- Positive-template classification and original zero-damage DEFEND body despite
  negative numeric delta, truthful conditional bars/party vitals and no newly
  fabricated damage/aggro/mode behavior.
- MISS/empty no numeric work, real zero/default-row RNG, exact current packet
  audiences, duplicate/stale finish, write-unknown failure, save/transfer and
  retained item accounting without refund or replay.

Even-bPositive negative templates, general damage/foreign-target combat,
final-death completion, unsafe signed variance, hardcoded IDs128/229/3604,
linked/repeated effects and richer802 remain outside this capability. Source
CRT/libm exact truncation boundaries, original Map admission/bootstrap and
native ordinal persistence versus original DB-only save projection remain
separately OPEN. No execution-verified full parity is claimed.
