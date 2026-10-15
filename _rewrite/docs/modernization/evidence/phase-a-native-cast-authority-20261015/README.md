# Native cast ownership and source CURE exceptions

2026-10-15, local `main`, baseline `5f81b9f`.
**IMPLEMENTED / INTEGRATED_UNVERIFIED**. The overall Phase A implementation
remains incomplete. This increment binds a waiting request to its original
session tenure and implements the admitted pure-CURE branches of source skill
exceptions128/229/3604.

Owner restrictions remain in force: local Git only; no push/deployment, builds,
tests, SQL execution, migration/import, backup extraction, server/client/service,
container or runtime verification. Only source/API/SQL-text/packet/diff and
CSV/Markdown metadata inspection was performed. Original server/client/Lib/
protocol, backups, older evidence and migrations001–072 remain unchanged.
Written044–072 remain unapplied; the next migration remains073. Private audit
output stays outside this change. No new wire field, public API, schema,
publication, grant, learned row, effect row or successful result is seeded.

## Request ownership across a wait

The actual dispatcher already filters ReadyReplica requests to its supported
MOVE/SKILLEND allowlist. A normal incoming replica cast was not an unrestricted
prototype path before this increment.

The additional boundary is the wait for a gameplay lease. BeginGameplay checks
Ready/checkpoint state; it does not bind a waiting packet to the captured role
and epoch. A complete transfer can demote the old primary to ReadyReplica and
clear its payload while the old request is suspended. Fresh replicas have
unknown zero bars, while demoted replicas can retain stale prior bars. Neither
payload absence nor those bars authorize prototype gameplay under a new tenure.

Dispatch now compares char/user/key/channel/connection/authority_epoch/role and
Ready/open state during wait retries and after acquiring the gameplay operation,
before selecting a handler. A changed tenure drops the old request while RAII
releases its acquired operation. Same-tenure requests retain the existing wait
behavior and replica allowlist. Existing PostgreSQL writers still independently
check durable authority; this change also prevents earlier ACK/cache/cost work
from being selected under the wrong captured tenure. It is not a claim that
those writers previously accepted stale durable mutations.

## Actual source exception context

For128/229, original FINISH tests the target for maintained129/230, then removes
all matching partner rows if present. Current supported native maintained
profiles cannot contain those richer effects. The shared private resolver
requires the actual supported vector and checks partner absence explicitly.
The absent-partner source branch now proceeds without the blanket skill-ID
refusal. Present129/230 and its real notifying/stat-dependent erasure remain
unsupported; no fake END/STAT is emitted for an absent effect.

For3604, original FINISH looks up the session player's actual learned3603 and
uses its BYTE rank for Defend. A present rank0 object is valid. The native
resolver performs that current ID lookup without borrowing3603 costs, reuse,
critical, attack profile or abilities. Only a copied effect profile's rank
changes: outgoing3604 calculation/growth and the existing DEFEND rank byte use
it; critical and outgoing powers/type/attack level retain their original source.
The ACT wire ACK remains rank1 and does not freeze the later effect rank.

The resolver runs in both real item preflight callers and inside each eligible
living FINISH target before activity or effect work. A nonempty MISS still
requires source exception context, then skips numeric calculation/RNG and uses
the resolved DEFEND rank. Empty or already-dead filtered target occurrences do
not look up3603 or inspect partner state through this resolver. Empty completion
keeps its existing equipment/receipt epilogue.

Missing3603 on a nonempty target remains unavailable. The original returns from
the whole FINISH before terminal CheckEquipSkill; the native path refuses the
unsupported transaction rather than inventing a rank, a successful empty result
or an equivalent equipment-only completion. Item preflight refuses this context
before hooks/debit. A later unsupported FINISH does not refund or replay a prior
ACT debit. This bounded refusal is a documented capability policy, not full
source early-return parity.

The all-SA_ONCE positive-template HP/MP profile is still mandatory. Source3604
also has an IsDamageType/ABILITY percentage-damage branch; this increment does
not admit it by changing an ID guard. Actual published eligibility for any
specific item/skill pair still requires later acceptance.

## Production integration and evidence

- [Source, client and ownership audit](analysis.md) traces source main-PC cast
  selection, SayToAll producers, actual demotion/wait boundaries, exception
  order, secondary vitals limitations and remaining work.
- [Map implementation review](map-casts.md) records the four production files,
  private resolver, real preflight/FINISH callers and unchanged authority.
- Dispatch remains the actual incoming request boundary. Init/ACT preflight
  still precedes existing item hooks/debit; ordinary/loop/ACT FINISH retains
  current account/owner/session/epoch/cell/catalog/skill-context, inventory,
  accepted origin/route and one-use receipt/checkpoint/fullgraph authority.
- Signed variance, DWORD modulo, actual defaults, MISS802, zero-MP cleanup,
  bounded ordinary death, equipment epilogue and current awaited party/recipient
  output fences remain. No decoder, packet width or audience is added.

Static review covered the complete production diff, declarations/callers,
exception profile copies, current source rank and default semantics, RAII lease
placement, source/client packet fields and unchanged SQL/persistence paths.
Metadata checks cover 218 unique CSV data rows, 12 columns, LF, eight new
contracts, unchanged `ALL.RESPONSIBILITIES=NOT_ANALYZED`, added relative Markdown
targets and whitespace. None is compilation or executable acceptance.

## Remaining work and subsequent acceptance

Full secondary HP/MP/death cache convergence remains OPEN. Original own-PC ENTER
does not carry bars; client HPMP/DIE handlers do not update another Map process's
cache. Current checkpoint revision is not a monotonic sequence for every
gameplay mutation. No new polling protocol, client report or replica vitals
authority is invented here. Direct-handler native-owner guards remain separate
hardening; source recall ACTION exceptions and target-owner NPC/PvP DEFEND still
need their real contracts.

Present129/230 effects, missing3603 nonempty source-return parity, richer3604
damage, foreign/negative combat, linked/repeated effects, full admission/AOI,
AI/rewards, special death graphs and original DB-only effect-save projection
remain OPEN. Full original functionality has not been completed.

Phase B must exercise real client/PG checkpoint waits and complete handoff,
epoch/role/connection replacement, stale operation release, unchanged initial
replica filtering, actual128/229 item/template eligibility, partner absence and
refusal,3603 present rank0/rankN/removal, differing trigger/effect ranks, unchanged
CP/powers/ACK, MISS/empty/dead targets, signed/default/exponential values,
item accounting, ordered death/effect/party output and unknown commits without
replay. Preserve source protocol and compare actual client behavior.

No services or tests were run. Full functionality and compatibility with a real
original client remain unverified.
