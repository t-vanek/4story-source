# Native ACT trigger rank and scaled cures — 2026-10-15

Baseline: local `main`, `5da1bf0`. Status: **IMPLEMENTED /
INTEGRATED_UNVERIFIED**. This increment extends the existing bounded positive
own-PC ACTITEMS cure, including supported basic parties. It does not complete
Phase A or certify the original executable. Local Git only; no push.

## Implemented contract

The source FINISH looks up the outgoing skill's current trigger object. An
absent object gives rank0/critical0. A present object, including rank0, gives
its actual rank and the actor's baseline critical selected by the trigger's
attack type: MAGIC3 uses magic critical; every other type uses physical.
Kind21 reads critical before installing an instance skill. Copying the
trigger's ordinary cast profile would incorrectly include its instance-only
modifiers. The outgoing pure CURE retains its own attack type, baseline powers
and attack level. Missing current metadata for a present trigger is unavailable.

The original ACT acknowledgement stays rank1, critical0 and zero powers.
The trigger does not incur ordinary cast HP/MP costs, reuse, charge or effects.
The actual item remains the acceptance authority, with the existing source
activity/erase/deadline/debit order and exactly one successful receipt.
FINISH refreshes and derives the trigger again; the earlier ACK does not freeze
its rank or statistics. Existing receipt, route, owner, epoch, catalog, current
actor/recipient and party-output fences remain in place.

Actual INCREASE HP/MP rows now preserve Calc0 (constant), Calc1
`value + (rank - 1) * increment` and Calc3
`value - (rank - 1) * increment`. Rank0 uses signed integer promotion, with no
substitute rank1. Each normal target occurrence uses the existing source row
variance and final resource clamp, within the checked DWORD addition domain.
Both item initiation and ACT preflight negative normal magnitudes before hooks
or debit. Anticipated GOHOME2 or critical255 MISS checks actual802 capability
and skips cure arithmetic. Empty completion and MISS also skip Calculate and
cure randomness at FINISH. A later change to an unsupported normal magnitude
uses the existing refusal/close handling; it does not refund or replay an
already accepted item action.

## Changed code and evidence

- [ACT profile and preflight](../../../../../Server/TMapSvrAsio/services/postgresql_map_actitems.cpp):
  actual trigger lookup, baseline critical, Calc0/1/3 and shared producer preflight.
- [Item initiation](../../../../../Server/TMapSvrAsio/services/postgresql_map_item_use.cpp)
  and [private declaration](../../../../../Server/TMapSvrAsio/services/postgresql_map_service.h):
  preflight before ACTION; no new public service API or compilation unit.
- [Developer contract](map-trigger.md) and [independent source review](analysis.md):
  original complete FINISH/ACT branches, rank0 load chain, critical/instance
  ordering, integer bounds, all three profile callers and two preflight callers.
- [Coverage inventory](../../legacy-modern-coverage.csv): active contracts
  updated and six focused rows added. Whole-server responsibility coverage
  remains `NOT_ANALYZED`.

## Static verification and deferred acceptance

Verification is limited to original source, declarations/call sites, SQL text,
packet structure, Markdown links, coverage CSV structure and Git diffs.
Static review found no remaining blocker in this bounded scope. The coverage
file has178 data rows,12 columns, unique IDs and LF endings; its whole-server
`NOT_ANALYZED` row is unchanged. All14 added local Markdown link targets exist,
and `git diff --check` is clean.
No build, test, SQL, migration, import, backup extraction, service, server,
client or container was run. No actual deployed item/skill pair was certified.
Original sources, client, protocol, backups, applied001–043, existing written
044–072, historical evidence and private audits are unchanged. No new schema,
grant or publisher change; written044–072 remain unapplied; next073.

Executable acceptance must cover absent/present0/presentN and self/different
triggers; physical/magic/NONE selection and instance-only modifiers; rank/stat
changes between acceptance and FINISH; Calc0/1/3 bounds, zero and negative
values; critical255/GOHOME2 with missing/supported/richer802; empty and duplicate
FINISH; real item reuse/debit, current party output, recovery/handoff and
unknown commit/output without replay. The original client remains unchanged
and unverified for this increment.

Calc2 growth/floating conversion, other combine modes, negative or wrapped
effects, outgoing128/229/3604, other targets/groups, richer effects,
guild/companion/recall contexts and general combat remain OPEN. This work does
not claim source DB-only effect-save parity or full Map bootstrap acceptance.
