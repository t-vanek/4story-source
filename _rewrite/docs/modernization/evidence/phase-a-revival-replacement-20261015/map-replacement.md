# Map maintained-status collisions and retained800

Baseline `d5377dd`, 2026-10-15. Status: **IMPLEMENTED /
INTEGRATED_UNVERIFIED**. This increment received source, declaration, caller,
packet, SQL-text and diff review only. No build, test, SQL execution, migration,
import, extraction, server, client, service or runtime acceptance ran. No commit
or push was made by the Map agent. No schema change is needed.

## Source contract

The task initially called this replacement. The actual supported800 template
requires append. `CTPlayer::Revival` (`Server/TMapSvr/TPlayer.cpp:3610–3832`)
emits the existing aftermath/mode/revival/resource sequence, then calls
`ForceMaintain800`. The complete chain is `TObjBase.cpp:4397` →
`MaintainSkill:3964` → `UpdateBuffSkill:2953` →
`CTSkillTemp::HaveSkillData` (`TSkillTemp.cpp:36`) →
`PushMaintainSkill:4614`.

Positive data collisions require equal type, attribute and nonzero incoming
exec. Equal skill IDs alone do not collide. The admitted800 definition contains
only SA_BUFF/SDT_STATUS/STATUS_NONE0 rows, so an existing static800 is kept and
another800 is appended. The old row retains its identity, vector position and
timer; the new row receives the actual rank-one duration and new clock. Source
`CTSkill::SetEndTick` (`TSkill.cpp:132`) makes duration0 eternal. There is no
old-row END/STAT, clock refresh or deduplication in this branch.

For other currently admitted pure progression templates, the source comparison
is ordered per existing row:

| Existing matching positive row | Decision |
| --- | --- |
| Lower priority, or equal priority with lower actual value | Notifying erase |
| Higher priority, or equal priority with higher actual value | Keep and record collision |
| Equal priority/value and incoming self-PC force | Erase; admitted templates have no aura-radius exception |
| Posture, different type/attribute/exec, or incoming exec0 | Keep independently |

Source `CTSkill::GetValue:119–130` uses the first matching data value. The
admitted non800 definitions have one rank-one SA_BUFF/SDT_STATUS row with Calc0
and no value increment. Incoming forces are self-PC. The final source expression
permits append if any existing row was erased, even when another stronger row
survives. A collision with no erasure blocks append and produces no DEFEND.
There is no global strongest-wins normalization.

Actual status42 is shared by905/921/922/939;919 is64. This corrects the common
primitive's former distinct-status assumption. Its production force callers
remain800/820/903/923/918. Probability-item use retains its separate same-ID and
any-lucky-potion refusal; this increment does not create a new probability-item
flow or admit arbitrary buffs.

Original client DEFEND (`Client/TClient/CSHandler.cpp:2806/3115–3190`) reuses one
`FindMaintain(skillID)` visual and updates its timer. The server may keep several
independent800 rows. Client SKILLEND at6763 removes that keyed visual even if
another server row survives. No invented DEFEND refresh conceals this source
behavior.

## Native production implementation

`domain/maintained_effect.h::ProgressEffectTemplate` and
`services/postgresql_map_load.cpp::ReadProgressEffects` retain actual published
attribute with priority/status/value. This is private derived metadata, absent
from player persistence, transfer fields and client packets. Existing catalog
pins and richer800 rejection remain intact.

`services/postgresql_map_during.cpp::ForceProgressEffect` implements the ordered
positive rule. Incoming metadata is copied before notifying erasure can refresh
the payload and template vector. Each old-row lookup occurs against the current
payload. Absent templates retain the existing source no-effect branch; present
unsupported definitions remain unavailable. The255-row representability bound
refuses an overflowing transaction rather than wrapping a BYTE count.

`services/postgresql_map_revival.cpp::ReviveNative` removes its unconditional
existing800 refusal after that shared rule is corrected. Retained static800 now
passes through the existing same-cell, actual dead/statistics, ordinary-mode,
owner/account/current Ready-claim and checkpoint/graph authority. An accepted
append emits the existing **84-byte DEFEND body** from
`services/client_senders.cpp::EncodeForcedStatusDefend`: source wire host0/type0,
while the durable effect keeps its actual self-PC attacker and host. No opcode,
field or synthetic success is introduced.

Every effect removal remains observable separately. `EraseRevivalByAction`
records an intermediate snapshot after each erased800; native ACTION and
ordinary SKILLUSE no longer collapse several removals into one END/STAT event.
The ordinary cast hook remains `!loop`, preserving the source LOOP exception.
Existing independent periodic expiry and applicable zero-MP hooks continue to
iterate each row. Explicit own-PC SKILLEND retains the first attacker/type/skill
match and the original no-match END; no bCanCancel or wire-host gate is invented.

`handlers/maintained_effect.cpp` routes native cancellation END, conditional
HPMP and private STAT through the existing `DeliverNativeGridProjection`.
Its per-row ACTION effect output and the matching output in `handlers/skill.cpp`
use the same helper. It reuses the actor operation, acquires other recipient
operations without waiting, verifies actual SQL authority and exact Ready
identity, then rechecks actor/recipient current sessions and live3×3-cell
presence after awaits before enqueue. Busy or stale recipients can miss output;
there is no replay. Party HPMP publishes only after the owner's queue attempt,
which does not prove application delivery. ACTION ACK's64-radius audience and
the whole FINISH projection path are unchanged.

Source notifying erasure sends END7bytes, conditional HPMP only if maxima change,
then private STAT (`TObjBase.cpp:2621–2750`). These admitted pure statuses do not
alter maxima. Successful transactions commit all effect rows, core, receipt and
graph state before cache publication and output. Existing unknown-COMMIT
poison/close behavior and no-replay handling remain unchanged.

## Persistence boundary

Native migration033 stores rows by `(world_id,char_id,ordinal)`. `MaintainJson`,
ordered fresh load, checkpoint snapshots and full `Capture`/codec/hydration
preserve duplicate IDs and independent remaining times. The existing native
authority policy therefore retains these exact rows through final save, crash
recovery and relogin. Source RELEASEMAIN (`SSSender.cpp:2197–2239`) likewise
serializes eligible vector rows without ID deduplication. Private clocks never
enter the original transfer bytes.

Original database saving deliberately projects a different collection.
`SSSender.cpp:1110` filters store/local/expired rows in vector order;
`SSHandler.cpp:7131` invokes TSaveSkillMaintain for each eligible row. The tracked
backup-matched `_rewrite/docs/schema/procs/TGAME_RAGEZONE/TSaveSkillMaintain.sql`
inserts only when `(char,skill)` is absent, after `TSaveChar.sql:85` clears the
old collection. Original TSKILLMAINTAINTABLE has a `(char,skill)` primary key.
Thus the **first eligible duplicate** wins that DB projection. Full parity for
these DB-only filters, collapse and subsequent fresh loading remains **OPEN**,
separate from source live/transfer multiplicity and the modern exact ordinal
checkpoint policy. No guessed newest-row selection, general live deduplication
or historical migration edit was applied.

## Deferred acceptance and remaining scope

Phase B must cover retained static finite/eternal800 through death and repeated
ordinary revival; old/new independent clocks; no invented old-row END; actual
84-byte DEFEND; priority/value/attribute and equal self/foreign cases; mixed
weaker/stronger vector order and fully blocked insertion; first-match cancel
versus multi-row attack/zero-MP/expiry; LOOP;255-row rollback; ordered normalized
and full-graph checkpoint/recovery/handoff; real keyed-client visuals; current
actor/replica/neighbor changes across SQL awaits; owner loss and uncertain
COMMIT/output without replay. These are unexecuted acceptance tasks.

Richer800 ability/status definitions, general or negative buffs, complete
negative Defend/SoulHP/recall effects, special-mode/secondary revival, full source
bootstrap/AOI, whole ACTION/cast/FINISH output and original database save-only
projection parity remain OPEN. This increment does not close those contracts.
