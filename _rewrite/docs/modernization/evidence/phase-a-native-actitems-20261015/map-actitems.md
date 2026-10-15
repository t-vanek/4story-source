# Map native action-item initiation, acceptance and completion

Baseline `a3fa3ef`, 2026-10-15. **IMPLEMENTED / INTEGRATED_UNVERIFIED**.
Only source, declaration, caller, packet, SQL-text and diff review was performed.
No build, test, SQL execution, migration, import, extraction, service, client,
server or runtime acceptance ran. No Map-agent commit or push. Root owns the
written, unapplied071/072 SQL, publisher and grants; the next migration is073.

## Original request and client chain

`OnCS_ACTITEMSUSE_REQ` (`Server/TMapSvr/CSHandler.cpp:19875–20196`) parses the
same **36-byte** body emitted by `Client/TClient/CSSenderAll.cpp:1041`:
DWORD attacker, BYTE attacker type/channel, WORD map/skill, BYTE action,
DWORD action/animation, three FLOATs, DWORD defender, BYTE defender type.
The client says it to all Map connections. Source requires actual player Map
presence and an available main attacker, then checks riding, actual skill/map,
negative peace/tournament/arena, attack and previous-action eligibility.
Channel/map/position request fields are parsed but not used as authority.

The original ITEMUSE `IT_ACTITEMS=25` branch at9264 precedes that request. It
only emits **ACTION ACK17bytes** with action255, action/animationDWORDFFFFFFFF
and the item's actual skill. Its `bUsed=TRUE` is outside the later IT_USE-only
common block at9698. It does not arm reuse, consume, send ITEMUSE success or
run the USEITEM quest trigger. Client action/skill handling later emits ACT and
its animation completion sends the existing `CS_FINISHSKILL_ACK` at
`TClientGame.cpp:28801`; `CSSender.cpp:2980` writes its BOOLs as four bytes.
There is no new opcode or Ready/progression acknowledgement.

The complete client UseItem caller (`TClientGame.cpp:20276–20991`) can fall
through from the type25 branch to its kind switch and emit another ITEMUSE.
This source quirk is retained as an acceptance case, without request-body
deduplication. Its action-skill eligibility path does not require a learned row
or ordinary HP/MP skill costs, so the bounded unlearned-trigger flow has a real
original-client caller.

ACT uses FindTSkill, without requiring a learned skill, skill reuse or HP/MP
costs. For own PC, its source order is activity clock; conditional battle mode;
attack/zero-MP effect erasure; used-skill registration; strict item-group reuse;
ITEMUSE success; HaveItem/UseItem one; SKILLUSE success. A cooldown failure or
missing item occurs after the earlier activity/erase state changes. NEEDITEM
also follows an already armed cooldown and ITEMUSE success. These late failures
must not roll back those source effects or grant an accepted finish.

`HaveItem`/`UseItem` (`TPlayer.cpp:5776/5798`) scan ascending BYTE inventory and
slot maps, including equipment; ACT has no wrap/power filter. The admitted
native debit selects that same first item. An equipped, zero-count or unverified
first candidate is explicitly unsupported, never silently skipped for another
stack. `DebitNativeItem` emits the actual DEL/UPDATE only, without MOVEITEM ACK
or a USEITEM quest event. No consumable flag exempts ACT's one-item debit.

Success SKILLUSE has **67bytes**, one target, source wire rank1, zero powers,
zero attack level, actual level/countries/equipment-special, canSelect0 and
actual attacker position. Errors keep the original full **62-byte** body with
zero targets and sender defaults, including canSelect0. ITEMUSE ACK is8bytes. FINISHSKILL is34bytes
plus5 per surviving target; the client can legitimately filter its sole target
away before finishing.

## Bounded actual effect and authority

`services/postgresql_map_actitems.cpp::ReadActItemSkill` reads the configured
pinned character chart independently of learned templates. Supported actors are
actual living own PCs in ordinary native PvE, partyless and without guild,
tactics, riding, arena/tournament or companion/recall/local graph dependencies.
Actual statistics and local ENTERCHAR clocks must exist. GOHOME2, which makes
the source hit test miss, is refused. IDs128/229/3604 retain explicit refusal
because FINISH has additional hardcoded erase/rank behavior for them.

Initiation, ACT and shared FINISH also verify the actual current position's
published physical Map owner. Client route/position bytes cannot supply that
proof. Refreshed statistics may not silently clamp preexisting resource values.

Supported templates are actual positive kind21, client-run, guaranteed-hit,
single-target, instant SA_ONCE HP/MP cures with flat Calc0/INCREASE data, no
maintained/duration/charge/repeat/linked/target-active/posture/dismount branch.
NULL repeat metadata is unavailable, rather than guessed zero. The actual
trigger must have no learned row: source FINISH therefore uses rank0 and
critical0. ACT's fixed wire rank1 is not reused as an effect rank. The source
type/attribute scan selects attack type, while pure CURE has no IsLongAttack
ability row and uses actual melee AP. Values, powers and maxima come from the
actual published chart and refreshed supported statistics, never synthetic bars.
The explicit source-key row order follows the existing native convention; the
original query lacks ORDER BY, so full original row-order equivalence remains
an acceptance boundary.

Source FINISH (`CSHandler.cpp:20447`) consults the complete guild-skill registry
even for a guildless actor. Migration072 and the explicit offline publisher
expose `skill_context_compat.catalog_release` and `TGUILDSKILLCHART`. A single
consumer query requires configured character/routing manifests and a verified
complete release, then proves the actual WORD skill absent. Missing publication
cannot establish absence. The same guard now applies to ordinary native FINISH.
`bIsSpecial` is read from actual equipped item templates for the ACT reply.

No NPC state is fabricated for the positive cure. Whole-source review proves
the relevant aggro call is a no-op: pure CURE fails IsDamageType
(`TSkillTemp.cpp:118`), so Base Defend's `nDamageHP/MP` remain0; PerformSkill takes
them by value. Post-Defend cure aggro is0 and `CTMonster::SetAggro:141` returns
before mutation. No bRadius whitelist or invented nearby-NPC absence is used.
Negative damage and real monster aggro remain OPEN.

## Atomic production paths and completion

Native dispatch connects ACT to `OnNativeQuestReq` and `ChangeQuest`. The source
IT_ACTITEMS initializer retains its common ITEMUSE deal/store/arena, ownership,
wrap/riding/level/delay checks, then validates the supported continuation before
emitting ACTION. It does not run the IT_USE common effects. Native ACT checks
exact own attacker/target and finite ignored coordinates as explicit modern
hardening; source allowed a wider sender/attacker/target relationship.

The existing transaction locks current owner/account/session claim, checks pinned
catalogs and durable inventory/graph/checkpoint heads, then applies actual
clock/erase/reuse/item mutations and ordered responses. Late SPEEDYUSE/NEEDITEM
changes commit through the same quest ledger. Successful ACT additionally appends
an accepted-cast receipt with origin22006 and actual local map/channel. New
ordinary and loop writers record21172/21362; historical NULL origins retain their
known learned/loop interpretation. Migration071 records this distinction without
backfilling fake ACT history. Its columns are required at Map boot.

ACT's request route fields stay ignored. FINISH correlation uses the separate
actual route, source ACK ground/targets, exact login and an outstanding one-use
receipt. It never classifies ACT from request length or borrows ordinary skill
cost/rank/charge. The accepted receipt's rank1 documents the actual wire ACK;
the finish independently rebuilds the actual rank0 effect. An empty filtered
target list consumes the receipt without an invented heal. Otherwise the
existing atomic FINISH applies actual ordered cure rows/randomness, clamps to
actual maxima, updates the Defend activity clock and sends DEFEND followed by
HPMP only if resources changed. Applicable zero-MP and equipment-posture cleanup
retain their original ordering. All output follows commit.

Initiation and ACT output use current actor/recipient SQL authority and actual
presence around awaits. Neighbors remain source3×3 cells; ACTION/SKILLUSE success
uses actual64-radius nearby recipients. FINISH now uses the same existing
nonblocking projection helper, with a post-await64-radius check for nearby
packets. Own operations are reused; other recipients never wait for a second
actor while holding the caller. Party vitals follow only the owner's HPMP queue
attempt. Queueing is not an application delivery ACK. No transaction or packet
replay is introduced for unknown COMMIT, stale recipients or failed output.

## Local item-delay observations

Source ITEMUSE permits equality (`deadline > now` rejects), whereas ACT rejects
equality (`deadline < now` alone permits). Relative remaining0 could not prove
which case applies. `ItemCooldownState` now retains private absolute observations,
seeded from actual hydration and preserved through sampling, including expired
RAM entries. Both actual rearm sites replace the deadline. The ordinary-cast
relative validator initializes observations on the real clock before temporarily
rebasing sampled time to zero. Capture/SQL/codec still contain only group and
remaining; no process clock is added to original wire or persistence.

Direct DWORD deadline comparison across wrap is explicitly unsupported in ACT:
new delays crossing the current DWORD period, or retained deadlines from another
period, are refused atomically. This is a bounded range guard, not full DWORD
timer parity. Zero delay still arms the source in-memory group and rejects
another ACT in that same tick. Expired/zero entries continue to be omitted from
the existing durable/transfer projection.

## Deferred acceptance and OPEN

Phase B must exercise the real original-client ITEMUSE→ACTION→ACT→SKILLUSE→FINISH
chain with actual pinned supported definitions; absent/richer/NULL templates;
missing/changed072 release and guild membership; exact17/36/62/67/8/39/84/21-byte
shapes; true0 effect rank versus wire1; equality/zero-delay/rearm/wrap refusal;
init WRAPPING refusal versus ACT's unchanged lack of a wrap filter, and
equipped first-stack refusal; late NEEDITEM and SPEEDYUSE durable
ordering; multiple800 erasures; empty filtered finish; single-use receipt and
identical outstanding-request ambiguity; normalized/full-graph checkpoint and
handoff; actual mode/presence/owner changes around awaits; missing071 boot and
unknown COMMIT/output without replay. No acceptance case has been executed.

General action-item damage, other targets, monsters, linked/repeated/maintained
effects, learned triggers, party/corps/group effects, special modes, broader
equipment mutations, complete AI/negative Defend/SoulHP and full source
bootstrap/AOI remain OPEN. A scope change after accepted consumption can make
finish unavailable; no fake heal or item replay compensates for it. Existing
FIFO matching cannot distinguish an old identical finish from a later identical
outstanding operation because the original wire has no nonce. Earlier source
DB-only maintained save-filter/deduplication boundaries remain unchanged.
