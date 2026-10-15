# Native positive cure combine modes — 2026-10-15

Baseline: local `main`, `09317c7`. **IMPLEMENTED / INTEGRATED_UNVERIFIED**.
This increment extends actual own-PC action-item HP/MP cures to the five known
combine modes inside a checked positive arithmetic domain. Supported basic
parties retain their existing authority. Full modernization and original-client
compatibility remain unverified. Local Git only; no push.

## Source value, combine and base

Original `TSkillTemp.cpp:62–107` computes signed GetValue first, then combines
it with a DWORD base. Pure HP/MP CURE uses current effective GetMaxHP/GetMaxMP
(`TObjBase.cpp:2011–2035,3556–3565`), not the current damaged bars. The signed
value and final cure delta must therefore be separate: DECREASE of a negative
Calc3 value can yield a valid positive cure. Actual Calc0..3, trigger rank0,
formula34 FLOAT metadata and checked exponential conversion remain in use.

| Mode | Actual source calculation within the supported domain |
| --- | --- |
| INCREASE1 | Return signed GetValue. |
| DECREASE2 | Negate GetValue; reject INT_MIN before the negation. |
| MULTIPLY3 | Convert the factor to DWORD, multiply by the maximum, then subtract the maximum. |
| DIVIDE4 | Replace a nonpositive divisor with1, divide the maximum, then subtract the maximum. |
| PRECENT5 | Multiply DWORD maximum and converted DWORD factor first, divide by double100, truncate to INT, then subtract the maximum. |

Signed-to-DWORD factor conversion is implemented, including negative factors.
It is distinct from arithmetic wrap. A uint64 product must fit DWORD before
narrowing, and a projected total must be at least the base before subtraction.
The final delta must be0..INT32_MAX/15 (143165576), so every original signed
variance product is representable. No arbitrary signed-base multiplication or
floating percentage product replaces the source order. With base1000, percent
150 produces delta500; it does not produce a heal1500. With base1, signed
factor-1 converts to DWORD_MAX and yields percent delta42949671 without a
product wrap. These are algebraic illustrations, not executed acceptance cases.

DIVIDE's nonpositive-divisor clamp and base0 can produce a real zero delta.
Negative final deltas, wrapped product/subtraction and unsupported modes are
refused. Base0 does not bypass GetValue or unsafe Calc2 evaluation. Zero rows
still draw their normal per-row variance at FINISH. The existing sampled-roll
DWORD resource-addition guard and clamp after all rows remain; preflight does
not promise that future resource additions fit before later state changes.

## Actual production integration

`ReadActItemSkill` preserves actual bInc1..5 instead of forcing INCREASE.
Both item-initiation and ACT preflight use current rank and current maxHP/maxMP
before ACTION/activity/erase/reuse/debit. Pure admitted CURE, automatic posture
and marker rows do not change these maxima during row evaluation or cleanup.
FINISH refreshes and derives the current state again; it does not freeze rank
or maxima in the earlier rank1 ACT acknowledgement.

All normal FINISH rows now use the same checked helper, including existing
ordinary/loop cures. Their previous generic unchecked Calculate fallback is
removed. Unknown Calc or combine modes are explicitly unavailable for normal
completion, instead of inheriting the source default no-op. This is a documented
native capability boundary; full source wrap/default behavior remains OPEN.
The generic skill engine and isolated prototype callers are unchanged.

MISS and empty completion perform no GetValue, combine or cure RNG. Actual802
capability/lifecycle, equipment cleanup, one-use receipts, current owner/session/
epoch/route/catalog/state proof, atomic checkpoint/fullgraph persistence and
actor/recipient/party output fences remain. Post-acceptance unsupported changes
refuse the existing transaction without refund or replay. No trigger cast,
ordinary cost, wire field, new SQL schema, grant or publisher is introduced.

## Evidence and deferred acceptance

- [Developer contract](map-combine.md) and [independent source audit](analysis.md).
- [Checked value/combine helper](../../../../../Server/TMapSvrAsio/services/native_cure.h),
  [ACT producer](../../../../../Server/TMapSvrAsio/services/postgresql_map_actitems.cpp)
  and [shared FINISH](../../../../../Server/TMapSvrAsio/services/postgresql_map_checkpoint.cpp).
- [Coverage inventory](../../legacy-modern-coverage.csv); whole-server
  `ALL.RESPONSIBILITIES` remains `NOT_ANALYZED`.

Verification is static source, type/API/caller, SQL-text, packet/client-consumer,
CSV, Markdown-link and Git-diff review only. The CSV has190 data rows,12 columns,
unique IDs and LF endings; the whole-server row is unchanged. All14 added local
Markdown targets exist, and `git diff --check` is clean. No build, test, SQL, migration,
import, backup extraction, service, server, client or container ran. Original
sources/client/protocol/backups, applied001–043, existing written044–072, older
evidence and private audits are unchanged. Written044–072 remain unapplied;
next073. No actual published item/skill pair was certified.

Phase B must exercise actual Calc0..3 × combine1..5 data, signed intermediates,
INT_MIN, high-bit maxima/factors, percentage delta/truncation and integer-product
order, DIVIDE/base0, nonwrap versus wrap boundaries, variance and sampled resource
overflow, current-rank/max changes, normal preflight before debit, ordinary/loop
hardening and unknown modes. It must also cover MISS/empty/actual802, real item
reuse/debit, original DEFEND/HPMP order and audiences, party vitals, recovery/
handoff and uncertain commit/output without replay. Client consumers at
`CSHandler.cpp:2806,3492` still read the existing DEFEND and actual HP/MP fields;
source inspection does not prove original-executable acceptance.

Final negative damage/death, DWORD product/subtraction/resource wrap, unknown
default no-ops, outgoing128/229/3604, other targets/groups, richer effects,
guild/companion/recall graphs, general combat, source DB-only effect-save parity,
CRT/libm boundaries and full Map admission remain OPEN.
