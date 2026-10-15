# Signed numeric CURE on a living own-PC target

2026-10-15, local `main`, baseline `e2e4784`. Production status:
**IMPLEMENTED / INTEGRATED_UNVERIFIED**. The overall Phase A implementation
remains incomplete. This evidence covers signed numeric deltas within the
existing odd-bPositive pure HP/MP CURE profile, not negative-template combat
or final-HP0 Defend death.

Owner restrictions remain in force: local Git only, no push/deployment, build,
test, SQL execution, migration/import, backup extraction, service/server/client,
container or runtime verification. Only source/API/SQL-text/packet/diff and
CSV/Markdown metadata inspection was performed. Historical acceptance is not
evidence for this code. Applied001–043 and written/unapplied044–072 are unchanged;
no new schema, publication or grant is required; the next migration remains073.
Private extracts and pre-existing audit output remain outside this change.

## Implemented behavior

The existing producer preflight and real FINISH arithmetic now admit signed
final row deltas within ±143165576, the symmetric all-roll source INT domain.
The internal helper is named `native_cure::Delta`; both direct consumers were
updated. Checked Calc2 conversion, source DWORD product/subtraction, signed-bit
projection, DECREASE INT_MIN refusal and actual BYTE-mode defaults remain.

FINISH calculates `n + n * roll / 100` with signed arithmetic and division
toward zero. It then converts that completed sample to DWORD and adds modulo
2^32. Underflow can produce a high DWORD that later caps to the maximum; the
implementation does not promise that every negative sample lowers the final
bar. Each real row, including a zero/default result, keeps its independent RNG.
Actual MISS and empty completion still skip numeric evaluation and RNG.

Rows run in order, then HP/MP are capped once per target. Intermediate HP0 may
be restored by a later row; final HP must remain positive for each complete
Defend occurrence. Final MP0 uses existing source-ordered effect cleanup.
Final HP0 still refuses the entire FINISH transaction before staged output or
finish/core/checkpoint writes escape. A prior committed ACT debit is neither
refunded nor replayed by this extension.

Numerical sign does not change the template's IsNegative/IsDamageType results.
The supported odd-bPositive pure CURE keeps the existing zero-damage DEFEND,
conditional HPMP and party-vitals path. It gains no damage/aggro producer or
negative-template battle transition. Current account/owner/session/epoch,
catalog, route, cell, accepted-receipt, inventory/checkpoint/fullgraph and awaited
party/recipient authority remain in the real production path.

## Implementation and source evidence

- [Map implementation review](map-signed.md): shared checked Delta, signed
  variance, DWORD addition, living guard, actual producer/FINISH consumers and
  retained transaction/output authority.
- [Source and client audit](analysis.md): complete original classification,
  PerformSkill/Defend behavior, client DEFEND/HPMP consumers and the separately
  unimplemented Defend-death dependency graph.
- Production changes are limited to
  `Server/TMapSvrAsio/services/native_cure.h`,
  `services/postgresql_map_actitems.cpp` and
  `services/postgresql_map_checkpoint.cpp`.
- Current Map README, seven global status/deployment documents and the
  [coverage CSV](../../legacy-modern-coverage.csv) reflect the new boundary.
  The CSV has 203 data rows and 12 columns, with six new signed-CURE contracts;
  `ALL.RESPONSIBILITIES` remains `NOT_ANALYZED` and unchanged.

Static review traced dispatch to OnFinishSkill, native FINISH selection and its
two direct Delta consumers, including both upstream item preflight callers.
The complete diff retains current-rank/maxima selection, signed operation order,
the all-row cap/death boundary, MISS/empty bypass, actual zero/default-row RNG,
existing core/receipt widths and unchanged packet fields/audiences. Source-wide
native Map search found no stale `native_cure::Magnitude` call. CSV shape, unique
IDs, LF, preservation of the overall responsibility row, added relative Markdown
targets and whitespace were checked as metadata. None of these inspections is
compilation or runtime acceptance.

## Remaining work and subsequent acceptance

Final-HP0 completion needs LASTHIT DEFEND, conditional wire HPMP0/0, and the
actual DuelLose/OnDie graph with distinct nearby/neighbor audiences. Active
duel, ANGEL/companion, GodBall/local/arena/tournament, recall/self/transform and
retained-static effect context require actual authority or proven source absence.
Historical duel rows do not prove no active duel. The DROP helper's ACK and
recovery+5000 rules belong to that producer and cannot be called unchanged here.
The [analysis](analysis.md) gives the concrete follow-up contract; no death code
is implemented by this increment.

Negative-template and foreign-target combat, hardcoded128/229/3604,
linked/repeated/richer effects, broader admission/bootstrap and full original
functionality remain OPEN. Exact CRT/libm boundaries also require acceptance.

Phase B must use actual PostgreSQL and the unchanged original client to cover:

- Signed final results across Calc/combine/default modes, current-rank/maxima
  changes and preflight before action/item debit.
- Division around zero, both variance limits and just-outside refusal,
  DECREASE INT_MIN, invalid Calc2 and real zero-row RNG.
- Mixed signed ordered rows, underflow followed by cap, restored intermediate
  HP0, per-target final-HP0 atomic refusal, final MP0 cleanup and packet order.
- Original zero-damage DEFEND and truthful lower/wrapped bars, party fanout,
  clocks, save/transfer, stale/duplicate finish and uncertain-commit handling.
- MISS/empty bypass, existing item accounting without refund/replay, and current
  actor/recipient/party authority during transfer and awaited publication.

No services or tests were run. Full functionality and compatibility with a real
original client remain unverified.
