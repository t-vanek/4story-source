# ACT item cure with an actual learned trigger: source review, 2026-10-15

Baseline: local `main`, `5da1bf0`. Analyst write scope is only this new audit. No build, test, SQL, migration, import, extraction, service, server/client execution, commit or push was performed. Private audits and older evidence were not read or changed.

The bounded continuation removes the blanket learned-trigger refusal from the existing ACTITEMS positive own-PC HP/MP CURE path and admits its source Calc0/1/3 magnitudes. It uses the source's actual current trigger rank and baseline critical selection at FINISHSKILL. It does not cast the trigger, borrow its cost/reuse, apply its effects or widen to arbitrary outgoing effects. Existing guild/companion/recall/stat/publication, item debit, cell owner, receipt and output fences remain prerequisites.

## Exact original finish ordering

The complete `Server/TMapSvr/CSHandler.cpp::OnCS_FINISHSKILL_ACK` was read from20197 through20666, before `OnCS_REGISTERBR_REQ` at20667. For the admitted PC, non-server-run kind21 outgoing skill:

1. Actual outgoing template, used-skill record, map/context and source target gates remain required. The kind21 used-record check is distinct from ordinary learned-cast checks. The outgoing skill itself need not be learned.
2. At20380 source finds `pAtkSkill = pATTACK->FindTSkill(pTemp->m_wTriggerID)`. For **kind21 it skips the early assignment to `m_pInstanceSkill`**; that early assignment is guarded by `m_bKind !=21`.
3. It assigns `bSkillLevel = pAtkSkill ? pAtkSkill->m_bLevel :0` and `bCP = pAtkSkill ? pATTACK->GetCritical(pAtkSkill) :0`. These are the **current** object/rank at finish, not the previous ACT acknowledgement's rank1, and not a rank supplied by the finish request.
4. It determines physical long-range selection and attack-level selection from the **outgoing** template, not the trigger. In the PC branch, only a self trigger (`wTriggerID == outgoing wSkillID` and an actual learned object) is installed as `m_pInstanceSkill` for the subsequent AP/AL reads; the pointer is then cleared. A different trigger never becomes that instance.
5. Each actual target runs `GetAtkHitType`, then Defend with these source rank/critical/power values. The outgoing128/229 cleanup and3604 rank override remain separate hardcoded branches. CheckEquipSkill still runs after the target loop, including an empty list.

`TObjBase.cpp:2486–2494`, complete FindTSkill, is an ID lookup returning the existing CTSkill pointer with no rank test. The distinction is:

| Actual trigger object | Finish rank | Finish critical |
| --- | --- | --- |
| Missing |0|0|
| Present with rank0 |0|Actual baseline physical/magic critical selected by the trigger template|
| Present with rankN |N|Actual baseline physical/magic critical selected by the trigger template|

No trigger means the source's actual missing-object branch, rather than a substitute learned object or a guessed rank1. Present rank0 must not be conflated with absence.

## Rank0 is supported by the actual source load chain

`Server/TMapSvr/DBAccess.h:1965–1991`, CTBLSkill, selects `wSkillID,bLevel,dwRemainTick` by character ID only; there is no positive-rank filter. The column binding for rank is BYTE. `SSHandler.cpp:3644–3673`, inside OnDM_LOADCHAR_REQ, serializes every fetched rank. The OnDM_LOADCHAR_ACK loop at4878–4923 reads that BYTE, skips tutorial or a missing template, otherwise constructs CTSkill, assigns `m_bLevel=bSLevel` and inserts it into `m_mapTSKILL` without excluding0. The later FindTSkill lookup therefore returns a present rank0 object. Existing copying paths at5883/6081 preserve skill objects as well.

Native fresh hydration (`services/postgresql_map_load.cpp:577–578`) copies each actual skill rank without a positive-rank filter. Its ordered SkillJson/checkpoint representation also retains0. Native DeriveStats builds a template/profile for each hydrated row; the new ACT lookup must likewise test actual row presence rather than `bLevel !=0`. Missing metadata/profile for a present row is unsupported authority, not permission to fall back to the missing-trigger branch.

## Baseline critical is different from the trigger's cast profile

`TObjBase.cpp:2255–2265`, complete GetCritical, asks the **trigger template** for GetAttackType and chooses magic critical only when that type equals SAT_MAGIC. `TMapType.h:356–363` defines NONE0, PHYSIC1, LONG2, MAGIC3, MAGICLONG4. The actual GetAttackType implementation (`TSkillTemp.cpp:172–198`) returns PHYSIC immediately for PHYSIC/LONG attributes, MAGIC immediately for MAGICNO, keeps an elemental-magic fallback, or NONE. A NONE trigger selects physical critical in GetCritical. It must not be given an invented magic default.

GetCriticalPysProb/GetCriticalMagicProb at `TObjBase.cpp:2126–2156` derive actual baseline formula, equipment and CalcAbilityValue, plus source companion bonus. The current native statistics capability already bounds the active/guild/companion graph. `CalcAbilityValue:1409` includes an explicit or current instance only when one is installed. For kind21 at the critical read, the trigger has not been installed. Its own instance-only ability modifiers therefore do **not** participate in this critical value. Its already applicable ordinary passive contribution, if admitted by the native stat graph, remains part of the baseline as usual.

`services/postgresql_map_load.cpp::DeriveStats` builds `p.statistics=build()` before filling per-learned-skill `instance_abilities` and the outgoing `attack_profile` values. Thus the actual baseline `statistics.physical_critical` / `magic_critical` are appropriate. The trigger's `attack_profile.attack_type` is usable hydrated template metadata; **its `attack_profile.critical` is not the value to borrow**, because that profile was built with the trigger's instance abilities. Likewise none of its powers, attack level, long-range selector, costs or effects should be copied into the ACT cure.

The later self-trigger AP/AL case is safe within the existing outgoing profile. Every outgoing row remains SA_ONCE / SDT_CURE / HP or MP and INCREASE. Calc0/1/3 changes its cure value, not its data type. `TSkillTemp.cpp:197–209`, IsLongAttack, requires an ABILITY row; pure CURE remains melee for physical power even if an attribute is LONG. `CalcAbilityValue` can only get an instance ability subtotal from ABILITY rows, and the admitted maintained graph has no CURE-to-instance modifier. Thus installing a same-ID pure-CURE instance adds no AP/AL delta. Both self-trigger and different-trigger cases use actual baseline AP/AL. Outgoing attack-level selection stays source PHYSIC1 ? physical : magic; that rule is independent of the trigger's critical-type selection, including the NONE quirk.

## Source rank-scaled magnitude and integer domain

The complete `TSkillTemp.cpp:62–107`, GetValue/Calculate, proves Calc0=`value`, Calc1=`value+(rank-1)*increment`, Calc3=`value-(rank-1)*increment`. Source BYTE rank and WORD value/increment promote to signed INT; rank0 subtracts1 as INT rather than wrapping to BYTE255. `TObjBase.cpp:3556–3568`, inside the complete PerformSkill, calculates each ordered HP/MP row at actual finish rank, then adds `nValue+nValue*(rand()%16)/100` to the actual DWORD resource. Existing native `services/skill_effect.h::GetValue/Calculate` implements these same0/1/3 expressions and SVI_INCREASE. No new arithmetic engine or guessed growth coefficient is needed.

| Calculation | Full source rank0..255 domain |
| --- | --- |
| Calc0 |0..65535|
| Calc1 |-65535..16711425|
| Calc3 |-16645890..131070|

The largest admitted nonnegative value times15 is250671375, within INT32; its variance and resulting positive row magnitude also fit INT32. Existing native checked DWORD resource addition still rejects overflow instead of claiming modulo-wrap/death parity. Calc2 stays unsupported: its growth coefficient, floating evaluation and INT conversion require a separate bounded contract.

Only nonnegative NORMAL results belong to this positive-cure slice. The shared `PreflightActItemCure` runs at actual ITEMUSE initiation and ACT acceptance after context refresh, before ACT hooks/deadline/debit: a predicted MISS validates only actual802 capability and returns without magnitude arithmetic; otherwise each current-rank GetValue must be nonnegative. It never clamps a negative result to0 or replaces actual rank with ACK1. Mathematical guards are deliberately absent from the shared profile reader so empty completion and actual MISS remain usable. At Finish, the existing `!miss` per-target calculation and negative-value guard use the freshly derived current rank; MISS/empty do not calculate or draw variance. A post-acceptance rank change that makes a NORMAL row negative closes/refuses the existing finish transaction without a new finish receipt, cure or refund of the earlier accepted item action.

## ACT acceptance remains an item action

The source `OnCS_ACTITEMSUSE_REQ:19875–20196` still uses actual FindTSkill(template), not an ordinary learned cast. Its actual hook/item/deadline order is unchanged. The type25 ITEMUSE initialization remains ACTION-only; ACT acceptance owns actual source item reuse and exactly one selected stack debit, then emits SKILLUSE success with **wire rank1, critical0 and zero accepted powers**. Learning the trigger does not add RequiredHP/RequiredMP, a learned-skill reuse/charge receipt, learned effect invocation, or another item debit.

`Client/TClient/CSSender.cpp:2980`, FINISHSKILL, sends the existing34+5N body, with no effect rank or critical field. Server DEFEND84B carries the actual finish rank and critical; the client `OnCS_DEFEND_ACK:2806` reads them independently of the earlier SKILLUSE acknowledgement. Native origin071 correctly retains ACT `skill_rank=1` as acceptance metadata; it must not be rewritten to the trigger rank or used to freeze that later rank.

Source GetAtkHitType at `TObjBase.cpp:2266` returns MISS for critical255 or GOHOME2 before its kind21 NORMAL rule. Actual learned-trigger baseline critical255 can therefore now select the existing truthful MISS→Avoid802 completion for ACT, without a cure delta. Real ITEMUSE initiation and ACT acceptance preflight anticipated mode2 **or critical255** against actual802 capability before hooks/debit; the shared finish checks current nonempty-target MISS after rederivation. A changed trigger between acceptance and finish uses current rank/critical and actual802 capability. Empty completion still has no hit test or Avoid requirement. No accepted item/cost is refunded or replayed if a later context becomes unsupported.

## Existing authority and implementation review

The production changes are limited to `services/postgresql_map_actitems.cpp`, `services/postgresql_map_item_use.cpp` and the shared private helper declaration in `services/postgresql_map_service.h`. ReadActItemSkill removes the blanket learned-trigger rejection, finds the current actual skill row (including0), then requires its current hydrated template/profile to match rank, character level and aftermath. It takes only the trigger attack type to select actual baseline critical and copies the actual current rank. Outgoing cure type, baseline powers and fields remain separate. A missing trigger retains0/0. The reader preserves actual Calc0/1/3 and WORD value/increment, rejects Calc2/other calculations, still requires nonempty pure CURE and installs a valid outgoing profile before callers use it. The new static preflight helper is declared with matching argument types and directly includes the existing skill_effect header.

All three production ReadActItemSkill callers rederive actual context through RefreshEquipment: type25 init, ACT acceptance and shared Finish. Finish still runs current owner/account/session/Ready claim, unlock, pinned publications including072 outgoing guild-skill absence, physical cell, inventory/effect/core/accepted-cast head and one-use receipt validation. It re-reads the outgoing ACT profile after that refresh, so the accepted rank1 ACK does not freeze trigger rank or critical. The existing accepted request/ground/actual-route/ordered-target correlation remains unchanged. No client-derived learned rank, party membership or trigger template enters this authority.

Source pure own CURE still ignores the incoming raw party WORD for membership authority. Actual local party tuple and bar publication use the existing fenced projection/current World membership path. The patch does not change handlers, recipient/current-identity fences, packet audiences, maintained writers, recovery clocks or process-local party authority. Committed Finish replies/effects/core state still precede cache publication and one queue attempt per output; unknown outcomes are not replayed. No new schema, grant, publication, receipt origin or wire field is needed.

Hardcoded outgoing128/229/3604 refusals remain.128/229 would erase maintained `ID+1` before Defend;3604 requires learned3603 and overrides the calculated effect rank. Those dependencies are not implemented or bypassed by learning a trigger. Calc2, percent/negative CURE and richer value/cast graphs, arbitrary targets, general trigger effect application, companions/guilds/recalls and offensive combat remain outside this slice.

## Deferred executable acceptance

PhaseB must use actual published outgoing/trigger rows and exercise missing, present0 and presentN triggers; same-ID versus different-ID triggers; PHYSIC, MAGIC and NONE selection; a trigger whose instance-modified critical differs from baseline; actual current rank or trigger membership changes between item acceptance and finish; Calc0/1/3 at0/1/above1/255, normal nonnegative/negative preflight and post-acceptance changes, MISS/empty bypass of magnitude and Calc2 refusal; per-row variance and DWORD overflow bounds; changed level/equipment/aftermath under real supported producers; baseline255 and GOHOME2 MISS802 missing/supported/richer branches; unchanged ACK1/critical0/item reuse/debit/cost; actual DEFEND rank/critical/powers; empty completion; stale owner/session/route/party/recipient and uncertain commit/output with no replay; normalized and full-graph checkpoint/recovery/handoff. Unsupported changes require their own source-supported producers, not fabricated successful runtime transitions. Exact actual dataset eligibility, original client behavior and these executable gates remain unverified.

Final stable production diff, new `map-trigger.md` and Map README were read directly against the full source Finish ordering, GetCritical/type selection, load/FindTSkill rank0 chain, DeriveStats baseline/instance sequence and all three profile callers. The Calc0/1/3 extension, matching static helper declaration/definition/include, both actual preflight callers, MISS/empty scope, current-rank shared Finish, preserved hardcoded bounds and unchanged origin071/packet fields are present. No remaining critical blocker was found in this bounded static review. The implementation is **IMPLEMENTED / INTEGRATED_UNVERIFIED**; actual data eligibility and all executable acceptance remain deferred. This is static source/API review, not compilation or runtime confirmation.
