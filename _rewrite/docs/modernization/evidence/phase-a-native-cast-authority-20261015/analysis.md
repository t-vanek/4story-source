# Native cast authority and source FINISH exceptions — source review

Date: 2026-10-15. Starting revision: `5f81b9f`, local `main`.
Status: **IMPLEMENTED / INTEGRATED_UNVERIFIED** for the bounded four-file change reviewed below. Evidence class: static source and implementation review; no executable or live proof.

This review began by tracing secondary Map HP/MP/death convergence. That trace did not establish a source protocol for general secondary PC vitals replication. The coordinated implementation scope is therefore the existing cast authority boundary and the bounded own-PC pure CURE FINISH exceptions for skills 128, 229 and 3604. Secondary vitals convergence remains OPEN; it is not replaced with an invented report or peer opcode.

No build, test, SQL query, migration/import, extraction, service, server or client was run. No commit or push was made by the analyst. Only this new `analysis.md` is analyst-owned. Private `docs/audits/` content was not read.

## 1. Original secondary state is deliberately partial

`Server/TMapSvr/SSHandler.cpp:1447–1631`, the complete `OnMW_ENTERCHAR_REQ`, reads identity, map/position, guild/tactics/party, level/country/class, mode/riding, chat ban, soulmate, recalls and comment. Its PC descriptor contains neither HP/MP/maxima nor dead/action flags. The DWORD maxima/bars at 1537–1548 belong to each recall descriptor, not the PC. At the end the local recovery clocks are set to `m_dwTick + RECOVER_INIT`; they are not received over the wire.

World `OnMW_CHARDATA_ACK` (`Server/TWorldSvr/SSHandler.cpp:847` onward) does receive primary maxima/bars and updates World character status, but its later ENTER descriptor is the partial descriptor above. Reading World status is not evidence that ENTER transports those bars to every Map.

The native implementation preserves that distinction in `Server/TMapSvrAsio/handlers_native_entry.cpp:100–177`: a freshly pending replica starts with a new `CharSnapshot`, unknown four bars set to zero, no `CharacterPayload`, the actual World cluster descriptor and local entry recovery clocks. A **demoted** source differs: `handlers/session.cpp:147–167` confirms the outgoing transfer, changes to Ready replica and resets only `payload`; its old bars remain in the snapshot. Those stale retained bars are not a fresh resource projection. Primary descriptors are accepted only during Loaded, under a projection operation. Fresh unknown replica HP0 is not proof that source `OS_DEAD`/native `bDead` is true; neither kind of partial state may become a client CHARINFO or a full-graph mutation input.

### Actual client consumers and subsequent messages

* `Client/TClient/CSHandler.cpp:3492–3550`, complete `OnCS_HPMP_ACK`, reads `DWORD object, BYTE type, DWORD maxHP, HP, maxMP, MP` (21 bytes), changes the client object and UI, and conditionally sends **Relay** `CS_CORPSHP_REQ(squad, own character, maxHP, HP)`. It sends no Map HP/MP report and no MP in that corps message. See `Client/TClient/TRelaySender.cpp:307`.
* `OnCS_DIE_ACK` at 3552–3770 changes the client object/death UI. `CTClientObjBase::OnDIE` (`TClientObjBase.cpp:7481`) and `NotifyObjDie` (`TClientGame.cpp:21973–21980`) clear client state/target; the latter is not a network producer.
* Map `OnCS_CORPSHP_REQ` (`Server/TMapSvr/CSHandler.cpp:8384–8411`) forwards corps information; `OnMW_CORPSHP_REQ` (`SSHandler.cpp:11908–11923`) forwards the UI packet to the client. Neither writes PC runtime bars. `OnMW_PARTYMANSTAT_REQ` (`SSHandler.cpp:8033–8071`) likewise produces client party UI, not a secondary actor writer.
* `SendCS_MOVE_REQ` (`Client/TClient/CSSenderAll.cpp:69–99`) sends the source 26-byte movement body to all Map connections: map, XYZ, pitch/direction, mouse/key/action/ghost and speed. It has no resource fields. Original Map `OnCS_MOVE_REQ` at 439–667 updates movement/action/ghost and treats its own local HP0 specially; it does not reconstruct committed HP/MP from a report.
* `SendCS_REVIVAL_REQ` (`CSSenderAll.cpp:22–37`) sends XYZ/type (13 bytes) to all Maps. Complete Map `OnCS_REVIVAL_REQ` (`CSHandler.cpp:1067–1098`) sets wake/stand and moves every local connection, then calls resource/cost/effect `CTPlayer::Revival` **only for main**. This is a real secondary wake/movement producer, not an HP/MP replication packet. Native dispatch currently does not admit replica REVIVAL; that separate feature remains OPEN.

`CS_HPMP_ACK=0x52A2`, `CS_DIE_ACK=0x52A5`, `CS_REVIVAL_REQ=0x52A6`; the executable declarations and historical declarations agree in `Lib/Own/TProtocol/include/{MessageId.h,CSProtocol.h}`.

The source HP/MP copies around `SSHandler.cpp:5905` and 6103 are inside mode-specific saved-player restoration in `OnDM_LOADCHAR_ACK`, not a general secondary update handler. `OnMW_GETBLOOD_REQ` at 12551 explicitly requires a main player before touching resources. Neither is an interchangeable ordinary replica synchronization supplier.

### Authority needed for any future modern extension

Native `postgresql_map_replica.cpp::LockReplica` binds target server/token/connection, pinned manifests and exact primary server/token/connection/epoch to current unlocked `TCURRENTUSER` and the live primary runtime owner. `ConfirmReplicaLevel` separately proves a committed checkpoint/level; it is not a resource projection API. Committed core HP/MP and derived maxima are different inputs: core bars are stored, maxima require the actual supported stat/equipment/effect graph, which a partial replica does not have.

A future read-only PG projection must have an explicit known/unknown contract, exact primary and replica generation proofs, serialized local publication and a checkpoint/version fence. Client bars, action/ghost flags, party UI, delivery completion and zero placeholders cannot supply that proof. It must preserve local entry recovery clocks and must not run recovery/effects on a replica. During transfer, `handlers_main_transfer.cpp:27–45,86–124` changes phase, drains outstanding operations and only then captures target cluster/clocks and publishes the committed incoming core. An awaited projection must not overlay that promotion, a newer primary generation, an out-of-order commit, death or revival. This increment adds no such modern protocol or writer and claims no secondary vitals closure.

## 2. Client fanout and original caster authority

`CTClientGame::SayToAll` (`Client/TClient/TClientGame.cpp:12272–12284`) copies a request to every logically valid Map connection. These actual producers use it:

| Producer | Source body | Original own-PC caster selection |
| --- | --- | --- |
| `SendCS_ACTION_REQ`, `CSSenderAll.cpp:319–341` | object/type/action/act/animation/channel/map/skill, 19 bytes | `OnCS_ACTION_REQ`, `CSHandler.cpp:1233–1437`: own session player only when main and on a map, 1276–1280 |
| `SendCS_SKILLUSE_REQ`, 489–618 | 32-byte fixed prefix plus BYTE-counted 6-byte target rows | `OnCS_SKILLUSE_REQ`, 2429–3038: `m_mapPLAYER[attackID]` only when main and on a map; otherwise return, 2512–2528 |
| `SendCS_LOOPSKILL_REQ`, 620–862 | 23-byte fixed prefix plus BYTE-counted 6-byte target rows | `OnCS_LOOPSKILL_REQ`, 3040–3375: same main/map selection, then no attacker means return, 3111–3135 |
| `SendCS_DEFEND_REQ`, 245–317 | client hit/attack/target/power context | Target-owner resolution in `OnCS_DEFEND_REQ`, 1438–1781; not a generic own-PC cast admission rule |

The ACTION fixed body is **19 bytes**: 4+1+1+4+4+1+2+2. The SKILLUSE prefix is **32 bytes**: 4+1+1+2+2+1+4+4+12+1. LOOP prefix is **23 bytes**: 4+1+1+2+2+12+1. Each target includes ID, type and selection flag. No new body or opcode is needed for authority checks.

These source functions were reviewed through their actual next-function boundaries, including costs/reuse, effect hooks and success output. Selection precedes own-PC skill affordability/charge/cooldown/output. Source supports other roles that this native slice does not implement: Map-hosted monsters, main recalls, self objects and, for ACTION, main companions. Original SKILLUSE/LOOP can resolve a main foreign PC from the global map; the native supported transaction remains explicitly own-PC and does not inherit that broader authority.

There is an important ACTION exception: when own PC is not main, its `pOBJ` remains null, but the final `else if (OT_PC && skill)` at 1404–1433 can inspect actual RECALL/TRAP data and call `CheckRecallMon`. Therefore “all secondary ACTION requests are source no-ops” would be false. Native ACTION already rejects those unsupported definitions; silent secondary rejection is bounded to its currently supported own-PC action contract.

`FindTarget` (`Server/TMapSvr/TMapSvr.cpp:6323–6368`) resolves a PC only when its supplied host is main and the target is that host. `FindChar` immediately below merely finds an entry. DEFEND uses the former for its owned target; FINISH can use the latter for targets. These are distinct functions and must not be conflated into a universal main-only rule. Native foreign-target/PvP/NPC DEFEND remains unsupported rather than being sent to prototype combat.

## 3. Current native dispatch and implementable fence

At the starting revision, `handlers/dispatch.cpp:64–68` already silently rejects Ready replicas except MOVE/SKILLEND. Thus a directly received request from an already-Ready replica does not reach the prototype branch through that initial allowlist. That fact alone does not cover a request suspended as primary.

A concrete lifetime boundary exists: dispatch captures role/phase before a checkpoint wait, whereas `SessionRegistry::BeginGameplay` (`services/session_registry.h:185`) checks only Ready and checkpoint state. `BeginTransfer`/`FinishTransfer` at 149–170 can change role/epoch while the request is awaiting. Actual `OnConReadyReq` at `handlers/session.cpp:147–167` can complete demotion to Ready replica and remove the payload. A primary request that passed the old allowlist, awaited a checkpoint, and resumes after that completed transition can acquire `BeginGameplay` on the new partial replica. Ordinary SKILLUSE then used its payload-null prototype `Update`, ACTION its prototype broadcast, and DEFEND its prototype combat branch; LOOP has a separate no-payload ignore. The retained nonzero old bars make this different from only manufacturing NEEDHP from a fresh unknown-zero replica. This is a static reachable counterexample, not an executed reproduction or proof of an unauthorized PostgreSQL commit. Actual PG role/claim checks protect the durable native mutation branch, but do not authorize the prototype fallback.

The coordinated implementation contract is:

1. Keep the existing replica opcode allowlist. After acquiring the gameplay operation, verify open socket plus the full original char/user/key/channel/connection/epoch/role tuple and Ready phase; stale requests return before handler invocation.
2. Do not change source/native wire layouts or widen the replica allowlist. This increment closes the production dispatch tenure window; it does **not** add a new immutable backend-owner guard directly to each handler. Such direct-entry hardening remains OPEN. Direct synthetic handler calls cannot be cited as production dispatch proof.
3. Keep native complete-graph DEFEND unsupported, retaining the existing PG/native route and prototype separation through dispatch. Other original entity roles remain OPEN and are not fabricated by a primary-PC guard.
4. Retain actual PG cast/action/finish current-account/claim/owner/catalog/receipt checks and existing post-await output fences. The dispatch lease prevents an ownership handoff from completing over an in-flight operation, but transfer-out may start and drain; code must still honor phase/identity changes at output boundaries.

## 4. Bounded source FINISH exceptions

The complete `OnCS_FINISHSKILL_ACK` occupies `Server/TMapSvr/CSHandler.cpp:20197–20665`. The client emits it through its actual `m_pSESSION` (`Client/TClient/TClientGame.cpp:28768–28814`, sender `CSSender.cpp:2980–3018`), not `SayToAll`. It is `CS_FINISHSKILL_ACK=0x55F7`. Client ACK rank is not authority for either exception below.

### Skills 128 and 229: erase only when the partner actually exists

At 20601–20605, after target resolution/dead filtering and hit determination but before Defend, source checks `FindMaintainSkill(outgoing ID + 1)` and calls the WORD overload `EraseMaintainSkill` if found. For 128 this is 129; for 229 this is 230. `FindMaintainSkill` (`TObjBase.cpp:2767–2776`) returns the first match, but `EraseMaintainSkill(WORD)` at 2577–2593 iterates and erases **all matching rows**, preserving per-removal side effects. If absent, there is no END/STAT/HPMP event to synthesize.

The current actual graph proof is finite and inspectable: `ReadProgressEffects` (`postgresql_map_load.cpp:98–148`) creates templates only for 820/903/905/911/912/913/918/919/921/922/923/939 and supported actual markers 800/802. `SupportedPostures` (`services/posture_effects.h:17–25`) admits those templates or postures 131/132. Neither 129 nor 230 is a supported hidden active effect. Fresh and full-graph unsupported effects are not silently dropped to establish absence. Still require an explicit actual effects-vector absence guard for the relevant partner, so future template expansion cannot silently skip source erasure.

Within the existing all-CURE/stat-neutral graph and current guild/recall/companion/mode/map bounds, the unconditional outgoing-ID refusal can be removed. A present partner remains unsupported until its actual lifecycle, stat refresh and ordered outputs are ported. This is not a generic arbitrary buff cancellation feature.

### Skill 3604: current learned 3603 rank for Defend only

At 20380–20416 source establishes trigger rank, critical, country and attack powers. At 20556 it computes hit type. Only at 20607–20614, inside an actual eligible target, it looks up **session player** `FindTSkill(3603)`: missing means return from the whole handler; present, including level zero, assigns `bSkillLevel` to its current BYTE level before `Defend`.

The override therefore affects outgoing cure GetValue/Calculate and Defend's skill-level field. It does not replace trigger critical, AP/AL, outgoing formula/coefficients or ACT's fixed wire rank1 acknowledgement. Missing/present-zero are different source branches. The lookup is repeated at FINISH from current learned rows rather than copied from cast ACK or stored as a fabricated new receipt field.

Source keeps the overwritten local rank for later target iterations. Complete `GetAtkHitType` (`TObjBase.cpp:2266–2356`) first returns MISS for GOHOME/critical255, then returns normal early for kind21. Ordinary admitted `bHitTest=1, bHitInit=100, bHitInc=0` produces the same normal result irrespective of rank (although source consumes a hit-test RNG draw). Thus per-target effect rank with immutable original attack profile preserves the relevant hit result in this bounded pure CURE slice; equality of legacy/native RNG streams is not claimed. Richer damage/random/foreign-target hit semantics are not admitted.

Empty targets and targets skipped because already dead do not reach this lookup or its source return. In particular, an empty ordinary/loop/ACT FINISH must still consume its actual accepted receipt and run final `CheckEquipSkill` without requiring learned 3603. For a real eligible target, a missing 3603 is an explicit native capability refusal before target mutation. This is deliberately stricter than source's early return (which skips final equipment cleanup); it is not advertised as equivalent source missing-skill completion, a refund or permission to replay an earlier accepted ACT/cast.

ACT init/accept preflight may resolve the anticipated nonempty effect rank before action hooks/item debit, including MISS metadata, but must not evaluate cure arithmetic or RNG on MISS. Shared FINISH must resolve inside each eligible target and preserve empty/MISS arithmetic rules. This uses the same checked cure helper, actual outgoing rows and current maxima, not a synthetic empty skill or rank1 fallback.

## 5. Deferred acceptance and explicit limits

All implementation claims remain `INTEGRATED_UNVERIFIED` until later authorized execution. Required later checks include:

* Two source-compatible Map connections: fanout ACTION/SKILLUSE/LOOP produces one authoritative native caster; secondary unknown bars remain unknown and no prototype success/failure packet is manufactured.
* Dispatch captured primary across checkpoint wait and completed demotion: same socket with changed role/epoch cannot run the queued old request. Open/current tuple changes, reconnect, transfer-out start/drain and a fresh primary operation need distinct cases.
* Direct handler-entry defense by immutable native ownership remains deferred; preserve the production dispatcher boundary when constructing fixtures. Non-native prototype behavior is separately preserved, with unsupported native NPC/recall/self roles remaining OPEN.
* 128/229 with absent partner succeeds through the actual CURE/receipt path; a real 129/230 row is rejected without fake END output or silently erased state. Exercise fresh and full-graph checkpoints.
* 3604 current learned 3603 missing, present rank0, nonzero rank changed after acceptance, and trigger differing from 3603; verify magnitude/Defend rank change while critical/AP/AL and ACT ACK rank remain unchanged.
* 3604 empty, filtered/dead repeat target, normal, GOHOME/critical255 MISS and unknown/default/Calc2 cure definitions; empty/MISS must preserve their no-math/no-RNG boundaries and current metadata checks.
* Existing signed/modulo multirow living/death behavior, 800/802 lifecycle, ordered cleanup, final equipment processing, owner/nearby/neighbor output and party vitals continue under current full-tuple and PG fences. No general combat/foreign actor/secondary HPMP convergence claim follows from those packets.

No new migration, publication, grant, peer packet or original-client change is required for the coordinated bounded implementation. Original admission/full World bootstrap, secondary HP/MP/dead convergence, secondary revival movement, richer cancellation partners and missing-3603 source early-return equivalence remain explicitly outside this closure.

## 6. Final stable implementation review

The stable development handback and the actual complete diff were read directly. Reviewed production files:

* `Server/TMapSvrAsio/handlers/dispatch.cpp`: full original tuple/open/Ready comparison inside the checkpoint wait and after successful acquisition. The `SessionOperation` exists before the final check, so a stale return releases the acquired operation. The lambda references coroutine-frame locals only within their lifetime and is not retained elsewhere. The existing initial replica allowlist remains. No per-handler immutable owner guard was added or claimed.
* `Server/TMapSvrAsio/services/postgresql_map_actitems.cpp`: removed only the unconditional 128/229/3604 ID refusal; existing pinned profile, actual graph/stat, guild/map/recall/companion and own-PC bounds remain. The private resolver proves supported actual effects, explicitly rejects a present 129/230 partner and reads current learned 3603 by row presence, allowing BYTE rank0. Shared init/ACT preflight invokes it before the MISS return, and only normal completion invokes checked cure math with the resolved rank.
* `Server/TMapSvrAsio/services/postgresql_map_checkpoint.cpp`: the existing one-use accepted receipt, current claim/physical owner and actual equipment refresh remain. Inside each living target, a value copy of `SkillAttackProfile` changes only rank before last-attack clocks/RNG/effects; both `native_cure::Delta` and `EncodeCureDefend` receive that effect rank. Critical/powers/attack type/attack level and outgoing formula metadata remain from the actual selected profile. Empty and post-death skipped targets do not invoke the resolver. Existing death, zero-MP cleanup, output audiences, checkpoint/ordinal effects and final equipment cleanup are retained.
* `Server/TMapSvrAsio/services/postgresql_map_service.h`: the new `static std::uint8_t` declaration is private and matches its definition/callers. No public service API, wire shape, schema or identity numeric domain changes.

The unchanged real callers `postgresql_map_item_use.cpp:127` and `postgresql_map_actitems.cpp:145` both reach the revised preflight before action hooks/debit; the shared FINISH loop is the other resolver caller. Actual `finish_skill.h::EncodeCureDefend` reads rank from the passed value-copy profile. Source `CTPlayer::Defend` at 1884–1970 changes last-attack state only once Defend is actually entered, supporting placement of missing-rank refusal before those clocks.

The new `map-casts.md` and Map README were also read. They distinguish fresh unknown-zero replicas from demoted stale retained bars, initial replica rejection from the real checkpoint-wait transition, missing 3603 from present rank0, native refusal from source early return, and client UI from secondary state authority. The source WORD erase overload removes all matching partner effects; no first-only erasure claim is made.

No remaining source/API/authority blocker was identified within these four production changes by this static review. This conclusion does not establish compilation, executed fixture success, applied SQL, original-client operation, RNG-stream equivalence, or general secondary vitals convergence. Deferred acceptance above remains required. Analyst writes stop after this final record.
