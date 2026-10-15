# Native ACT exponential cure values — 2026-10-15

Baseline: local `main`, `8ed424c`. **IMPLEMENTED / INTEGRATED_UNVERIFIED**.
This increment adds actual Calc2 to the existing positive own-PC action-item
cure path, including its supported basic-party context. Full modernization and
actual original-client compatibility remain unverified. Local Git only; no push.

## Actual data and numeric contract

Source `TSkillTemp.cpp:62–107` defines Calc2 as
`int(value * pow(growth, exponent) / 100)`, where rank0 gives exponent0 and
other ranks give `start + (rank - 1) * next`. The current learned trigger
supplies rank; the outgoing cure supplies start/next. `TMapSvr.cpp:2655–2730`
stamps each skill with formula FTYPE_1ST34. Its SQL double coefficient is read
into source FLOAT (`DBAccess.h:81–105`) before use. No trigger coefficient,
guessed growth or default success replaces those actual values.

The actual profile reader preserves Calc2 rows and loads the outgoing start/
next plus formula34 only when needed. It requires an actual row and checks its
finite FLOAT range before converting the PostgreSQL double to FLOAT. Existing
applied017 views expose all three charts through the pinned complete character
publication; applied002 preserves the SQL double type. Existing native SELECT
grants and catalog/claim checks suffice. There is no new migration, publication,
grant, wire field or receipt field.

The preceding RefreshEquipment already reads formula34 while rebuilding actor
statistics. Its common formula reader now also checks finite FLOAT range for
both X/Y rates before narrowing, so an invalid rate cannot reach an earlier
unchecked cast before the ACT-specific reader. This intentionally tightens
native formula admission for every existing reader of those rates; it does not
change the formulas or clamp/substitute their values.

The new shared `native_cure::Magnitude` evaluates the FLOAT base as double,
then the WORD product and division by100. It checks power/product/scaled
finiteness and INT32 range before the integer cast. Conversion truncates toward
zero, including a negative fraction becoming0. It accepts only nonnegative
integer cure values within the source variance domain. A negative coefficient
is not arbitrarily forbidden if the actual integer exponent produces an
admissible result. Zero-value multiplication is not short-circuited around an
otherwise invalid power.

Source `TObjBase.cpp:3556–3565` multiplies signed INT `nValue` by `rand()%16`.
Checking only the final integer conversion would miss that overflow. The
shared helper therefore requires magnitude at most `INT32_MAX / 15`, or
143165576, before drawing randomness. This is a bounded capability rule for
every possible source roll; it refuses source-undefined arithmetic rather than
assigning it invented unsigned behavior. Existing per-row variance, checked
DWORD resource addition and final clamp remain. Calc0/1/3 remain admitted.

## Integrated producer and completion paths

Both existing item initiation and ACT preflight callers use the same checked
helper for normal cures before ACTION/activity/erase/reuse/debit. Anticipated
GOHOME2 or actual critical255 MISS only checks actual802 capability. The shared
profile reader loads metadata without evaluating the exponential magnitude.

FINISH derives the actual current rank and outgoing profile again and checks
INCREASE magnitude inside its non-MISS target loop, before randomness. MISS and
empty completion never evaluate pow, Calculate or cure variance. Actual802
capability/lifecycle, equipment cleanup and one-use receipt behavior remain.
Ordinary/loop INCREASE cures also use the shared safety bound at FINISH; their
other combine modes retain their existing generic engine. The generic
`skill_effect.h` implementation is unchanged. A later unsupported rank/context
closes/refuses the existing transaction without refunding or replaying an
already accepted item action.

ACT wire rank1/critical0/zero powers, actual item debit, current trigger critical
selection, checkpoint/fullgraph atomicity, current owner/session/epoch/route and
actor/recipient/party output fences are unchanged. No trigger cast, ordinary
cost, charge or reuse is introduced.

## Evidence and deferred acceptance

- [Developer contract and call sites](map-exponential.md).
- [Independent original-source, data and API audit](analysis.md).
- [Checked cure helper](../../../../../Server/TMapSvrAsio/services/native_cure.h),
  [ACT producer](../../../../../Server/TMapSvrAsio/services/postgresql_map_actitems.cpp)
  and [shared FINISH](../../../../../Server/TMapSvrAsio/services/postgresql_map_checkpoint.cpp).
- [Upstream formula reader](../../../../../Server/TMapSvrAsio/services/postgresql_map_load.cpp):
  checked X/Y FLOAT conversion during the existing actor-stat refresh.
- [Coverage inventory](../../legacy-modern-coverage.csv), with the whole-server
  `ALL.RESPONSIBILITIES` row still `NOT_ANALYZED`.

Only static source, declaration/caller, SQL-text, packet, CSV, Markdown-link and
Git-diff inspection was performed. The coverage CSV has184 data rows,12 columns,
unique IDs and LF endings; the whole-server row is unchanged. All15 added local
Markdown targets exist, and `git diff --check` is clean.
No build, test, SQL, migration, import,
backup extraction, service, server, client or container ran. Written044–072
remain unapplied; next073. Original sources/client/protocol/backups, applied
migrations, older evidence and private audits remain unchanged.

Phase B must exercise actual published Calc2 item/skill pairs, missing and
present0/presentN triggers, source FLOAT rounding, start/next versus trigger
metadata, truncation boundaries, negative/zero bases, underflow/nonfinite
powers, INT and variance bounds, upstream X/Y metadata refusal, pre-debit
refusal, current-rank changes,
MISS/empty/actual802, ordinary/loop INCREASE, mixed row order, real debit/reuse,
crash/recovery/handoff and unknown output without replay. The exact legacy CRT
implementation is unavailable in this repository: bit-level CRT/libm and
near-integer rounding parity remain explicit executable acceptance gates.

Other combine modes, negative/wrapped effects, outgoing128/229/3604, other
targets/groups, richer effects, guild/companion/recall graphs, general combat,
source DB-only effect-save parity and full Map admission remain OPEN.
