# Native environmental self damage and ordinary death — 2026-10-15

Baseline: local `main` at `d86d407`. Gameplay is **INTEGRATED_UNVERIFIED**;
new070, its publisher and grants are **IMPLEMENTED_UNVERIFIED**. Phase A remains
incomplete. Only static source/caller/API/SQL-text/packet/diff review was performed.
No build, test, SQL execution, migration application, import, backup extraction,
server, service, client, container or runtime acceptance ran. Local Git only;
no push or deployment. Applied001–043 and written/unapplied044–069 remain
unchanged. New070 is written only; next071. Private extracts and the existing
`_rewrite/docs/audits/` remain outside Git.

## Source and production integration

[Complete source analysis](analysis.md) and [Map implementation review](map-death.md)
record the source/client callers and implementation bounds. The original
`Server/TMapSvr/CSHandler.cpp:10608–10742` request is produced by actual client
fall, drowning and lava paths. It carries DWORD target, BYTE type, DWORD damage,
BYTE channel and WORD map (12bytes). Source ignores the trailing channel/map;
the actual primary claim, physical map and own-PC target establish authority.
Native parsing additionally requires exact EOF. Other target branches and source
anti-hack operator text remain outside this supported slice.

The real dispatcher now routes DROPDAMAGE through `OnNativeQuestReq` and
`ChangeQuest` to `DropNativeDamage`. Existing current-session/owner/Ready claim,
inventory/head/checkpoint and transaction fences guard the complete mutation.
Actual supported statistics, effects, companion/recall/local state and physical
Map ownership are revalidated. A lethal request additionally requires the explicit
source-data publication described below. Unsupported state is rejected before any
committed resource change. The existing ledger/core/effects/transfer checkpoint
commit precedes cache, presence and client publication; unknown results never
trigger a write retry. No new client field or operation schema is invented.

| Event | Supported source behavior |
| --- | --- |
| Damage0 or nonlethal own PC | Bounded HP subtraction; both recovery clocks become DWORD(now+5000); original requested damage is reported |
| Lethal own PC | Same damage/recovery hooks, then HP0/MP0/dead state and silent removal of actual nonstatic effects |
| Client output | DROP9bytes → HPMP21bytes with pre-death MP → DIE5bytes; no additional zero-MP HPMP |
| Effects | Actual static templates survive; nonstatic800 is removed silently; no synthetic SKILLEND/CHARSTAT |
| Later ENTER | Source action TA_DEAD7 for the stored dead actor; `bStartAct` remains the tutorial field |

`CTPlayer::OnDie` (`TPlayer.cpp:1971–2141`) and base `OnDie`
(`TObjBase.cpp:2379–2484`) were reviewed in full. The default
`ReleaseMaintain(BYTE bNotify=FALSE)` is decisive: this branch does not call the
notifying effect eraser. Supported damage does not invoke Defend, protection800
immunity or SoulHP; does not update lastAttack/mode/aftermath; and creates no XP,
currency, durability, loot, PvP killer or quest reward. These are source behavior,
not omitted fabricated penalties. Client DIE invokes its own silent nonstatic
cleanup; its MP intentionally remains the preceding HPMP value until a later
source message changes it.

Current actor and recipient socket/Ready/identity/epoch/presence are checked around
awaited publication proofs and before queueing. Another recipient's gameplay
operation is acquired nonblockingly; busy/stale recipients can miss an event.
These delivery guards and commit-before-output are modern authority boundaries.
They do not prove source admission/bootstrap, delivery acknowledgement or full
roster reconciliation.

## Source map-context publication — written070 only

The original death branch may update a castle/local score, drop GodBall or use a
companion Angel escape. Empty unknown state cannot stand in for these graphs.
Supported actors exclude actual active companion/recall, guild/local, tournament,
arena, riding and tactics contexts; source tournament/lounge maps500–532 are
outside scope. Inactive account-owned pet inventory is distinct from an active
companion. Native activation/pickup and other richer graph producers remain OPEN.

Routing014 publishes only its original four chart families and cannot prove the
death contexts absent. New [070-native-death-context.sql](../../../../../database/postgresql/070-native-death-context.sql)
adds an explicit publication pointer/history and three read-only `death_compat`
views. They require a verified full archival run, exact metadata/backup identity
against the current verified routing release, complete table value/text-byte
checkpoints and actual raw row counts. Typed views preserve source SMALLINT/WORD
bits and all original columns. No safe-map whitelist, table row or gameplay value
is seeded. Runtime does not recalculate every value or text-byte hash.

The [offline publisher](../../../../../tools/database/activate_death_context.py)
requires the complete historical model set and both pinned backup fingerprints.
Within one transaction it locks out active Map/World owners, locks source and
publication inputs, verifies the actual archived row values and original text
bytes against the manifest/checkpoints, matches current routing, and atomically
publishes the complete pointer/history. Unknown COMMIT outcomes require later
reconciliation without automatic retry. This tool was read, not executed.
[Separate Map grants](../../../../../deploy/sql/death-context-runtime-grants.sql)
permit SELECT on the three views only; existing native grants are still required.

One SQL statement checks release availability and both map absence queries before
lethal mutation. Any published TBATTLEZONECHART row for the actual WORD map, any
global battle-zone key0, or any TGODBALLCHART row for that map refuses this bounded
death. The global key0 matters because actual `FindLocal(0)` has no zero shortcut,
and every mission/castle/SkyGarden/occupation registry insertion requires its
corresponding battle-zone key. This conservative exclusion avoids guessing mutable
castle battle state. Missing, stale or count-inconsistent publication is unavailable,
never an empty-map proof. Value/text-byte drift is checked by offline activation;
runtime source inputs must retain their existing restricted write authority.

## Prototype isolation and remaining work

The existing `RunMonsterAi` prototype previously ran when native chart loading
also supplied a monster chart. It directly mutated cached HP with zero defense and
had no native transaction/claim authority. Native startup now excludes this
prototype. The helper also excludes payload-bearing or hydrated native actors
before selection, under mutation lock and before each send. Its legacy behavior
remains confined to nonnative actors. Real native monster AI/damage is OPEN.

Retained static800 correctly survives death, but existing native revival rejects
an already-present800: source replacement of this retained effect remains OPEN.
Only the ordinary cycle with absent/nonstatic800 can use the already bounded
same-cell revival. Full negative Defend/skill/monster/PvP damage, SoulHP, Angel,
recall/self-object cleanup, castle/local/GodBall scoring, special BOW/BR/arena/
tournament deaths, full movement/AOI/replica lifecycle and source bootstrap remain
OPEN. No complete modernization or real-client acceptance is claimed.

## Deferred Phase B acceptance

Verify exact12/9/21/5byte request/replies, malformed tails, primary-only SayToAll,
damage0/exactlethal/overkill/already-dead, both reset clocks without mode/lastAttack
change, old-MP HPMP ordering, durable MP0, silent static/nonstatic persistence and
dead ENTER. Exercise recovery → damage → lethal → checkpoint/logout/handoff →
supported revival, party vitals, current actor/neighbor loss across awaits and
unknown commit/send outcomes without replay. Verify absent/drifted/replaced070
publication, full manifest/value/text-byte mismatch refusal, actual map/key0
exclusions, owner barriers, least-privilege grants and native exclusion from the
prototype at selection/mutation/fanout. These are a future acceptance backlog,
not executed tests or a claim of runtime success.
