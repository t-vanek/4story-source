# Maintained-status collisions and retained revival800 — 2026-10-15

Baseline: local `main` at `d5377dd`. Status: **INTEGRATED_UNVERIFIED**.
Phase A remains incomplete. Only static source/caller/API/SQL-text/packet/diff
review was performed. No build, test, SQL execution, migration/import, backup
extraction, server, service, client, container or runtime acceptance ran. Local
Git only; no push or deployment. Applied001–043 and written/unapplied044–070
remain unchanged; no new migration, next071. Private extracts and the existing
`_rewrite/docs/audits/` remain outside Git.

[Source analysis](analysis.md) and [Map implementation review](map-replacement.md)
record the complete caller and client contracts. The evidence directory retains
the initial task name; source review established append rather than replacement
for the currently admitted800 template.

## Actual source collision contract

`CTPlayer::Revival` (`Server/TMapSvr/TPlayer.cpp:3610–3832`) emits ordinary
revival and resource replies, then invokes `ForceMaintain800`. Its full chain is
`TObjBase.cpp:4397` → `MaintainSkill:3964` → `UpdateBuffSkill:2953` →
`CTSkillTemp::HaveSkillData` (`TSkillTemp.cpp:36`) → `PushMaintainSkill:4614`.
Positive collisions depend on matching data type, attribute and a **nonzero**
incoming exec. Equal skill IDs alone do not establish a collision.

The actual supported800 shape contains only SA_BUFF/SDT_STATUS/STATUS_NONE0
rows. Exec0 never matches. Retained static800 therefore survives death and a
subsequent ordinary revival appends another800 with its own actual template
duration and new local clock. Old row identity/order/remaining time is retained.
No old800 is erased, no SKILLEND/CHARSTAT is invented, and no existing clock is
reset. An absent template retains the source no-effect branch; an existing richer
unsupported template still makes revival unavailable before commit. The original
same-cell, true-dead, ordinary-mode, actual-stat and ownership bounds remain.

Other currently admitted pure progression statuses use a single rank-one,
Calc0 data row. Published attribute, priority and actual WORD value feed the
shared native collision rule; definitions remain private derived metadata,
absent from player SQL/transfer/wire fields. Source `CTSkill::GetValue:119–130`
uses the actual first data value rather than the separate bInc calculation.

| Matching positive row | Source action |
| --- | --- |
| Existing lower priority, or equal priority with lower value | Notifying erase, retain vector order |
| Existing higher priority, or equal priority with higher value | Keep row and record collision |
| Equal priority/value with the incoming self-PC force | Erase old row; admitted templates have no aura-radius exception |
| Posture, different type/attribute/exec, or incoming exec0 | Keep independently; no collision |

The final source decision is also retained: any successful erasure permits the
new append even if another stronger matching row survived. If all matching rows
block insertion and none was erased, there is no new DEFEND. This is not a global
strongest-wins normalization. Probability905/921/922/939 share actual status42;
the common primitive can compare these admitted definitions without claiming that
its current producers force those IDs. Actual probability-item use has a separate
same-ID/any-lucky-potion CheckItemBuff refusal, which remains unchanged. Status919
is64 and remains distinct. No arbitrary buff or unsupported skill data is admitted
by this change.

## Production transaction and removal paths

`ForceProgressEffect` now uses the source ordered comparison for existing native
revival, EXP entitlement, level-benefit and soul-lottery producers. It copies
incoming metadata before a notifying erase can refresh/replace the payload.
Actual erasures still emit neighbor SKILLEND7bytes followed by private CHARSTAT;
the admitted pure shapes do not change resource maxima. Accepted force appends
the original84-byte DEFEND body with source wire host0/type0, while its durable
row retains the actual self host/attacker. There is no new opcode or body field.
The existing255-row representability bound refuses overflow before commit.

Revival's unconditional existing800 refusal is removed only after applying that
real collision rule. Existing quest-operation/core/effects/checkpoint/graph
authority commits the complete result before cache/output. Current owner,
account/key/Ready claim, receipt and graph validation remain intact. No unknown
COMMIT or output retry is introduced. Lethal environmental damage still requires
the preceding explicit070 map-context publication; this increment applies none
of its SQL or grants.

Multiple800 rows also require correct removal cardinality. ACTION and ordinary
SKILLUSE attack erasure now retain one ended event and intermediate snapshot per
erased row rather than collapsing every removal into one synthetic event. LOOP
keeps its existing source exception. Existing periodic expiry handles each
independent clock; zero-MP hooks keep their original applicability. Explicit
own-PC SKILLEND removes the first attacker/type/skill match, while attack erasure
may remove all matching flagged rows. Native cancellation and the per-row
ACTION/SKILLUSE effect output now reuse
current actor/recipient SQL authority, exact Ready identity and live grid presence
around awaited sends. Other recipients acquire only nonblocking operations;
busy/stale recipients may miss output without replay. Party bars publish only
after the owner's actual queue attempt. This does not establish an application
delivery acknowledgement or repair the whole ACTION ACK/cast/finish projection.

## Client presentation and persistence distinction

Original client DEFEND (`Client/TClient/CSHandler.cpp:2806/3115–3190`) updates
one `FindMaintain(skillID)` display and timer, even when the source server has
several independent800 rows. SKILLEND at6763 erases that keyed visual. Expiry of
the first server row may therefore remove the visual while another server row
survives. The implementation preserves this source behavior; it sends no invented
refresh DEFEND to conceal it. Actual original-client behavior remains unexecuted.

Native033 stores ordered rows with PK(world,char,ordinal), not skill-ID
uniqueness. `MaintainJson`, fresh load, exact checkpoint, `Capture`/transfer codec
and hydration preserve duplicates and their independent remaining times. This
continues the existing native exact-state authority policy through save/recovery
and handoff. Source RELEASEMAIN similarly serializes its vector, without treating
same-ID rows as one skill or adding private clocks to the wire.

Original database saving has a **different** projection: source
`SSSender.cpp:1110` applies store/local/expiry filters in vector order, and
`SSHandler.cpp:7131` calls TSaveSkillMaintain for each eligible row. The tracked
backup-matched `_rewrite/docs/schema/procs/TGAME_RAGEZONE/TSaveSkillMaintain.sql`
uses INSERT-IF-NOT-EXISTS(char,skill), after TSaveChar clears the prior collection.
Original TSKILLMAINTAINTABLE has PK(char,skill); the first eligible duplicate wins
that database projection. Existing reconstruction evidence records the module
as IDENTICAL to its backup. Full native parity for these DB-only save filters,
first-eligible collapse and subsequent fresh load remains **OPEN**, distinct from
live/checkpoint/transfer multiplicity. No guessed deduplication was applied to
every live transaction or historical migration. This difference is tracked rather
than presented as complete original persistence compatibility.

## Deferred Phase B acceptance

Verify actual pinned absent/pure/richer800 data; retained static finite/eternal800
through death and repeated ordinary revival; independent old/new timers and
exact output with no invented END; type/attribute/priority/value, equal self/foreign
rows, mixed weaker/stronger vector-order behavior and fully blocked no-DEFEND;
per-row ACTION/SKILLUSE/zero-MP/expiry removal versus first-match cancellation;
LOOP exception;255-row refusal with atomic rollback; ordered normalized/full-graph
checkpoint/recovery/handoff; real keyed-client visual behavior; current actor,
replica and busy/stale/closed neighbor across awaited cancellation proofs; owner
loss and unknown COMMIT/send outcomes without replay. General buffs/negative
Defend/SoulHP, richer800, source save-only persistence parity, special modes,
full bootstrap/AOI and complete client acceptance remain OPEN.
