# Native action items and verified skill context — 2026-10-15

Baseline: local `main` at `a3fa3ef`. The original client and protocol are unchanged.
This increment connects the actual ITEMUSE → ACTION → ACTITEMSUSE → SKILLUSE →
FINISHSKILL caller for a bounded positive own-PC HP/MP cure. It does not complete
general action-item combat or the full modernization.

Gameplay is **INTEGRATED_UNVERIFIED**. New SQL, offline publication and grants are
**IMPLEMENTED_UNVERIFIED**. Review was static source/API/SQL-text/packet/diff/CSV
inspection only, with Python AST syntax inspection without importing the tool.
No build, test, SQL execution, migration, import, extraction, service, server,
client, container or runtime acceptance ran. Local Git only; no push/deployment.
Applied001–043, earlier written044–070, original source/client/backups and older
evidence are unchanged. New071/072 are written, not applied; next073. Private
audit output and extracts remain outside Git.

## Source and implementation evidence

- [Complete original server/client chain and review](analysis.md).
- [Map implementation, packet contract and bounds](map-actitems.md).
- [Native ACT implementation](../../../../../Server/TMapSvrAsio/services/postgresql_map_actitems.cpp).
- [Initialization and actual item debit](../../../../../Server/TMapSvrAsio/services/postgresql_map_item_use.cpp).
- [Quest transaction, accepted origins and finish](../../../../../Server/TMapSvrAsio/services/postgresql_map_checkpoint.cpp).
- [Origin migration071](../../../../../database/postgresql/071-native-actitems-origin.sql).
- [Skill-context migration072](../../../../../database/postgresql/072-native-skill-context.sql).
- [Offline skill-context publisher](../../../../../tools/database/activate_skill_context.py).
- [Restricted Map view grants](../../../../../deploy/sql/skill-context-runtime-grants.sql).

The complete original functions, their callers and client producers establish
separate initiation, acceptance and completion contracts. Type25 initialization
emits ACTION17 only: no item debit, rearm, ITEMUSE success or use-item quest
credit. The complete original client can emit repeated initialization through
its kind-switch fallthrough; no body-based deduplication was invented.

ACT has an exact36-byte request. Source ignores its supplied route/ground and
uses actual actor context. Native accepts only the current own-PC/self request
with finite coordinates in ordinary PvE. Riding, actual map/previous-action,
physical Map ownership, live resources and supported graph checks precede
mutation. After source activity and applicable per-instance attack/zero-MP
erasure, strict item reuse requires deadline < now, including delay0. The owner's
ITEMUSE success8 precedes the actual one-item availability/debit check. Late
SPEEDYUSE/NEEDITEM retains source preceding mutations without authorizing finish.

Successful debit and accepted-cast receipt commit atomically with the existing
account/claim/catalog/quest/core/effect/graph/checkpoint fences. Actual first
source bag/slot selection is retained; an unsupported first stack is refused,
not skipped. Success SKILLUSE67 has wire rank1 and zero powers/costs; failure
SKILLUSE62 has no targets and canSelect0. Actual equipment-special uses the
published bIsSpecial field. Nearby success has source3×3 cells and distance<=64.

FINISHSKILL uses its existing opcode22007 and fixed34+5N body, including DWORD
BOOLs. Correlation uses accepted origin, actual ACT route, ground and ordered
target subsequence from this login. An empty filtered list consumes the one-use
receipt without an invented heal. Actual effect rank0 comes from the absence of
a learned trigger, independently of acceptance rank1. The admitted actual chart
has kind21 and only positive flat instant HP/MP CURE rows. Existing FINISH
evaluates each row with source randomness, clamps to actual maxima and emits
DEFEND84 then conditional HPMP21. Whole-source review proves this all-CURE
profile's zero aggro returns before any monster mutation; no NPC absence or
arbitrary radius whitelist is required. General negative Defend remains OPEN.

## Written schema and offline publication

071 adds nullable origin and authoritative ACT channel/map to existing accepted
casts. Historical NULL origins retain their known ordinary/loop interpretation
without backfill. New ordinary21172, loop21362 and ACT22006 writers record origin
explicitly. ACT requires rank1, request36, ACK67, no ordinary charge and actual
route. The Map constructor requires these columns. Existing table-wide grants
cover them; no old receipt or migration is rewritten.

Original FINISH consults TGUILDSKILLCHART independently of actor guild0 or
skill kind21. 072 supplies a separate complete registry, publication pointer
and history. Its release requires a verified archival run, current verified
character/routing releases with matching metadata and both pinned backups,
exact checkpoints, actual guild-row count and non-NULL source keys/type. No
default chart, row, guild membership or absence is fabricated.

`activate_skill_context.py` requires the complete pinned historical manifest.
Under the shared offline Map/World owner barrier and table/advisory locks it
checks actual archive value/optional byte hashes, current releases and import
checkpoints, then atomically publishes and verifies the exact release tuple.
Runtime trusts restricted raw values after this offline hash check; runtime
views recheck identity/count/checkpoints, not a full row hash on each cast.
NULL original key/type semantics remain unsupported. No automatic retry follows
an unknown COMMIT outcome. Private manifest/rows stay outside Git.

The separate role grant permits only schema usage and SELECT of two skill-context
views. Missing/stale publication or actual guild-skill membership refuses the
affected operation. **Ordinary FINISHSKILL also now requires this publication.**
071/072, prerequisite migrations, publisher and grants are later Phase B steps;
none was run. [Deployment prerequisites](../../../../../deploy/README.md).

## Private clocks and current output authority

Relative item remaining0 cannot distinguish ACT equality from expiration. A
private absolute-deadline map is reconstructed at hydration, seeded before
ordinary-cast relative validation and replaced on both real rearm paths. It
never enters wire, SQL, hashes or transfer state. ACT refuses source DWORD-wrap
cases it cannot reproduce; it does not silently substitute unrestricted64-bit
behavior. Expired/zero groups retain only the existing process-local policy.

Initiation/ACT and shared FINISH recheck current actor and recipient identity,
Ready role/epoch, socket, SQL authority and actual grid after awaited work.
Nearby FINISH also rechecks64 distance. Party vitals follow the owner's HPMP
queue attempt. A queue attempt is not an application delivery acknowledgement;
stale/busy recipients and unknown send outcomes are not replayed.

## Deferred acceptance and remaining scope

Phase B must validate an actual published supported item/skill pair through the
unchanged client animation chain, repeated initialization, exact packet fields,
rank1 versus0, randomized rows/clamping, true last-stack deletion/decrement,
before/equal/after reuse and delay0, ordinary-cast interleaving, late failure
durability, multiple800 erasures, empty/duplicate/altered finish, mixed origins,
missing/changed context and stale ownership around awaits. Reconnect, crash,
fresh/full-graph checkpoint and handoff acceptance remain unexecuted. Static
review does not establish that a currently deployed chart contains an admitted
pair or that the original executable has accepted it.

Party/guild/tactics contexts, learned triggers, other targets, negative attacks,
linked/repeated/maintained effects, special modes, DWORD wrap, full source SQL
row-order equivalence, permissive source failed-ACT used records and repeat
finishes, full bootstrap/AOI/monster AI/combat/SoulHP and original DB-only
maintained-save filters/collapse remain OPEN. Native one-use FIFO receipts are
an explicit security/recovery policy. Identical outstanding original requests
have no nonce, so an old identical finish cannot be uniquely distinguished from
a later one. Scope changes after debit can make finish unavailable; no fake heal
or replay compensates for this boundary. Phase A is still incomplete.
