# Map ordinary native own-CURE final death

Baseline `1a9fa82`, 2026-10-15. **IMPLEMENTED / INTEGRATED_UNVERIFIED**.
Only source, declaration, production-caller, SQL-text and diff review was
performed. No build, test, SQL execution, migration/import, extraction, client,
server, service or runtime acceptance ran. No Map-agent commit or push. No new
schema, grant, publication or public API; written070/071/072 are unexecuted
prerequisites for their respective paths and next migration remains073.

## Original final-HP0 Defend contract

Signed numeric CURE still belongs to the admitted odd-bPositive, all-SA_ONCE
SDT_CURE HP/MP own-PC template. It does not turn into IsNegative or IsDamageType.
Source `Server/TMapSvr/TObjBase.cpp:3553–3568` performs all signed-variance
DWORD resource additions without an intermediate death check. Defend clamps
only after all rows at1038–1048 and captures its HP/MP-change condition there,
before CheckEternalBuff around1108.

Its original send at1156–1204 uses HT_LASTHIT4 when final HP is0. Conditional
HPMP reports current HP0 and wire MP0 even if actual internal MP has not yet
been cleared. The death branch at1265 then checks active duel, assigns temporary
HP=GetMaxHP and calls OnDie. In this capability, the native transient absence
and graph/map proofs exclude every intervening branch that could observe or
publish that temporary maximum; no invented healing packet/state is persisted.
`CTPlayer::OnDie` (`TPlayer.cpp:1971–2149`) reaches the base OnDie for the
supported ordinary self context. The base sets dead action/status, HP/MP0,
clears transform attack and calls `ReleaseMaintain()` with its default
notifyFALSE before neighbor DIE (`TObjBase.cpp:2379–2483`).

Silent ReleaseMaintain removes nonstatic rows and retains static rows. It does
not call notifying per-row EraseMaintainSkill or grant expiration benefits.
CheckEternalBuff's earlier legitimate notifying END/conditional bars/private
STAT remains distinct from silent death cleanup. No DROP packet or DROP's
recovery delay belongs to Defend death.

Original FINISH filters an already dead target unless CanDefendAtDie permits
the skill (`CSHandler.cpp` target loop around20535). The complete
`TSkillTemp.cpp:505–519` permits only CURE REVIVAL or AI TEMPT, neither present
in these all-HP/MP rows. Subsequent repeated self-target occurrences after death
must skip clocks/RNG/cleanup/DEFEND, rather than throw or revive it accidentally.
The outer FINISH still calls `CheckEquipSkill` at20662. That method
(`TPlayer.cpp:3964–4003`) has no dead guard and can notify removal of an
incompatible own posture after DIE.

## Native-only authority and actual absence proof

This is limited to the current native-owned PostgreSQL ordinary PvE path.
`main.cpp:238–242` rejects PostgreSQL startup unless mode is PvE, workers exist
and a World peer is configured. Actual OnFinishSkill now checks PvE explicitly
too. The public player-service signature is unchanged; no trusted boolean is
invented from a client field.

The active duel/GodBall/self/transform proof is about these current native
objects, not arbitrary source-server interoperability:

- Source CTPlayer constructor initializes active duel and GodBall to0 and its
  live self-object collection empty (`TPlayer.cpp:115/125/170`). Full server
  ENTER/RELEASE do not serialize those active fields. Duel records and score
  sets are historical data and are not required to be empty.
- Native fresh load creates a new snapshot/payload from app_world children;
  actual native transfer/promotion uses the complete supported graph contract.
  Native CS/MW dispatch has no duel/GodBall/self activation producer. Unsupported
  native DEFEND cannot fall into prototype combat; native prototype AI is
  excluded. Existing casts/actions/item paths do not activate TRANS/recall
  state outside their actual capabilities.
- Omitted transform fields alone would not prove no transformation: source
  maintained buffs can restore TRANS. Actual SupportedPostures/validated
  progression markers and all-CURE outgoing capability separately exclude that
  buff-driven branch. Any future activation producer must extend this actual
  state/guard contract before reusing this death capability.
- ANGEL comes from GetCompanion(active slot)→GetPetItemUse/GetPetItemKind
  (`TPlayer.cpp:5626–5663`), not the account's inactive TPET mount list. Native
  app_world admission does not import companion/item records; full-graph
  companions and companion items are retained and explicitly refused when
  nonempty. Actual fresh recall rows are already unavailable, and the native
  PostgreSQL process has no SociCompanionService attachment.

The shared `RequireNativeOrdinaryDeathState` explicitly requires actual payload,
statistics/effects/recovery, hydrated cluster, SupportedPostures, no guild,
riding, tactics, arena or tournament participation, no actual recalls and no
full-graph companions/items/recalls/recall-buffs/local ID. Maps500..532 remain a
conservative special-map exclusion, including lounge500. This does not pretend
to persist a new duel/ANGEL/transform field or infer active state from history.

`RequireNativeDeathMapContext` reuses the exact070 publication/absence query in
one SQL statement: configured routing-manifest verified release, no battle-zone
row on the map, no battle-zone ID0 that could supply FindLocal0, and no GodBall
row on the map. Missing/drifted publication or an unsupported row refuses death
before its cleanup/output/write. It does not replace current owner/account/
ready claim, physical-cell, route, inventory, catalog and checkpoint authority;
both callers already hold those proofs. Migration070 and its complete pinned
publication remain required even when no special registry row would be found.

## Shared state refactor and distinct DROP behavior

`ApplyNativeOrdinaryDeath` is a private state operation without packets, clocks
or replay policy: HP/MP0 and bDead1, silent nonstatic effect removal, then actual
stat/effect derivation. Static postures and supported static progression/800/802
rows remain. There is no early return based on bDead: RefreshEquipment during
earlier zero-MP cleanup can derive bDead from HP0 without having run OnDie.
The caller's captured final-HP0 decision controls this transition.

DropNativeDamage now uses the shared actor/map proofs and state operation at
its existing positions. Request decode/self-PC/physical-owner/actual-stat
checks remain; nonlethal behavior is unchanged. Lethal DROP still stages
DROP9B, then HPMP21B with old internal MP, applies death and stages neighbor
DIE5B. It still sets both recovery clocks to the current sample+5000. This
distinct wrapper is not called from FINISH and cannot inject DROP packets or
delay into CURE death.

## Real FINISH transaction and source-ordered output

Each actual target first skips a prior dead result. Living occurrences retain
current activity sampling, signed cure/combine/default arithmetic, independent
row RNG and low32 resource addition. After all rows and cap, FINISH captures
both lethal and conditional-HPMP flags before zero-MP cleanup and, for lethal
results, requires the shared actual actor/map proofs. No intermediate HP0 check
prevents a later row from restoring it.

Existing CheckEternalBuff output precedes LASTHIT DEFEND84B to source nearby
players. Conditional HPMP21B uses wire0/0 at final HP0. Silent ordinary death
then runs and DIE5B uses neighboring3×3 cells. Final CheckEquipSkill remains
outside the target loop and may append its original END/private STAT after DIE.
The encoder derives LASTHIT from current HP, not the possibly pre-derived bDead
flag. No extra generic dead-bars ACK or silent-erasure notification is added.

Dead state, current effects, exact ordered responses/audiences and the one-use
FINISH receipt commit with core/checkpoint/full-graph authority before cache
publication and network output. A failure rolls the complete transaction back;
already committed ACT item accounting has no new refund policy. Exact current
local party and actor/recipient session/key/connection/epoch/presence projections
still fence awaited output. Their non-invitation claim proof allows dead actor
projection; actual own HPMP queue attempts retain source party-vitals coupling.
Uncertain writes are not replayed. Signed INTEGER identities and positive BIGINT
receipt keys remain distinct.

## Static review and deferred acceptance

Read source Defend/OnDie/ReleaseMaintain, CanDefendAtDie and CheckEquipSkill,
native startup/handler runtime gates, shared private APIs, actual transaction
call positions, encoder and current projection authority. No public API, legacy,
client, Lib, SQL schema, backup or historical evidence was changed. Static
source/type review is not compilation or original-client/runtime acceptance.

Phase B must verify real client and PostgreSQL routes with actual authority:

- Signed/modulo rows reaching final HP0, intermediate HP0 restored before cap,
  MP0 versus old-MP nonzero, exact LASTHIT/conditional wire0/0/DIE order and
  preserved source audiences, including a neighbor beyond nearby64.
- Missing/drifted070 publication and local/castle/GodBall/graph exclusions
  refusing the whole finish without staged outputs or a replacement success.
- Static and nonstatic800/802 cleanup, zero-MP notifying cleanup before death,
  derived bDead before Apply, repeated dead-target filtering and final posture
  equipment cleanup after DIE. Historical duel records must remain allowed.
- Unchanged nonlethal/lethal DROP bytes/old MP/recovery clocks, corpse save/
  reload/checkpoint/full transfer, ordinary revival and static marker lifetime.
- Current owner/claim loss, receipt race/replay, unknown commit, busy/stale
  recipients and live party bars without a second durable mutation/refund.

Active duel resolution, ANGEL rescue, GodBall/local/castle, arena/tournament,
guild/recall/companion/self/transform branches, non-PvE modes, negative-template
or foreign/general combat and richer802 remain OPEN. This is not general
source/legacy interop; secondary Map death-state cache convergence also remains
OPEN and is not supplied by client grid output. Exact CRT/libm truncation boundaries, original Map
admission/bootstrap and native ordinal persistence versus source DB-only save
projection remain separately OPEN. No execution-verified full parity is claimed.
