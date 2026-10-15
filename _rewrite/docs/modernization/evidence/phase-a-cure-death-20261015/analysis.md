# Source audit: bounded own-PC CURE Defend death

Date: 2026-10-15. Baseline: local `main` at `1a9fa82`. Scope: final HP0 in the existing odd-bPositive, SA_ONCE, pure own-PC HP/MP CURE path, using the current native-owned PostgreSQL PvE authority. This does not establish general source/legacy interoperability, arbitrary active duel/entity state, foreign-target combat or complete World bootstrap.

This is static source, type, declaration/caller, SQL-text and diff review. No build, test, SQL query, import, extraction, migration execution, service, runtime, server or client was run. The analyst owns and changed only this new analysis file; no commit/push, global documentation/CSV, private audit, older evidence, historical source, client, protocol, library or backup modification was made. Private audits were not read.

## Original per-target completion and packet order

The complete original `OnCS_FINISHSKILL_ACK` is `Server/TMapSvr/CSHandler.cpp:20197–20665`, followed by `OnCS_REGISTERBR_REQ`. For each actual target it checks eligibility, dead status and `CanDefendAtDie` before GetAtkHitType and the PC Defend call. Complete `CTSkillTemp::CanDefendAtDie` (`TSkillTemp.cpp:505–517`) allows only CURE REVIVAL or AI TEMPT. Our all-HP/MP profile cannot defend a dead target.

The complete PC wrapper (`TPlayer.cpp:1884–1970`, next OnDie) updates last-attack tick and calls base Defend; battle mode is entered only for a negative **template**. Complete base Defend is `TObjBase.cpp:767–1306`, next GetFormulaInit. Complete PerformSkill is 3077–3929, next FindRemainSkill. HP/MP CURE at 3553–3568 applies every actual ordered signed row and its RNG, with source DWORD resource addition. No intermediate HP0 death or per-row resource cap occurs.

After all rows of one Defend, source order is:

1. Compute effective maxima, cap HP/MP, and capture the changed-resource flag (1038–1048).
2. Run CheckEternalBuff (1108), which may erase actual zero-MP eternal effects and emit their original END/statistics/conditional maximum-bar changes. It examines actual pre-OnDie MP, not a forced wire zero.
3. Send the existing nearby DEFEND. Its hit byte becomes `HT_LASTHIT` when HP is zero; `NetCode.h:1055–1063` defines that byte as 4. Pure CURE still has no damage entries and has successful PerformSkill unless an independent MISS occurred.
4. Send conditional nearby HPMP if the pre-cleanup changed-resource flag was true. HP is zero and **wire MP is zero** when HP is zero, even if internal MP has not yet been cleared (1171–1203).
5. Delete the instance skill, perform the resource-change/zero-aggro portion and any applicable maintained-stat output, then reach possible DuelLose and OnDie (1265 onward).
6. For the bounded ordinary own-PC death, silently remove nonstatic maintained effects and send neighbor DIE. A later duplicated target occurrence must be filtered as dead before hit calculation, activity clock or RNG. It cannot heal again merely because its occurrence was present in the accepted list.
7. After the entire FINISH target loop, the source still calls `CheckEquipSkill` at 20662. Complete `CTPlayer::CheckEquipSkill` (`TPlayer.cpp:3964–4003`, next MoveGhost) has no dead-status guard. It may erase unsuitable own postures and dependent stance skills after DIE. A blanket early return after the first death would incorrectly skip this work.

Each client connection preserves its own resulting packet order; there is no cross-socket global ordering/application ACK. Nearby DEFEND/HPMP and 3×3-neighbor DIE are distinct audiences. CheckEquipSkill erasure retains neighbor END and private statistics audiences. No new opcode, death ACK or damage record is needed.

## Complete OnDie dependencies and what the bounded profile excludes

Complete `CTPlayer::OnDie` is `TPlayer.cpp:1971–2142`, next GetLeftTick at 2144. Before base death it can execute:

- ANGEL companion item revival outside arena/team/tournament, including actual source cooldown checks, full HP/MP output, ITEMUSE success and an early return without death.
- `DropGodBall:2175`, which mutates the carried ball and map/World state.
- `MapKillPoint:2149`, which reads map castle battle state or the map's tournament-player registry.
- Death/removal of actual recall actors, companion actors and self objects, with their original World removals.

It then invokes complete `CTObjBase::OnDie` (`TObjBase.cpp:2379–2484`, next FindTSkill at 2486): TA_DEAD/OS_DEAD, both bars zero, BOW/BR bookkeeping where compiled, `SetTransAttack(0)`, silent nonstatic `ReleaseMaintain`, and neighbor DIE. `ReleaseMaintain:4928` preserves static rows and defaults to no notification. Static800/802 survive death; nonstatic800/802 must disappear from the next durable maintained vector and transfer graph without fabricated END/stat packets for silent death removal.

After base death the player path contains arena completion, `FindLocal(m_wLocalID)` kill bookkeeping and a PvPEvent condition. For this own host, the final PvP kill condition explicitly excludes `dwAttackID == m_dwID`. Neither complete ordinary self OnDie body independently deletes inventory, deducts XP or changes server quest progress. Pure CURE did not call the IsDamageType/OnDamage branch, so no monster reward, foreign damage, kill-quest, soul HP or loot producer may be synthesized.

Immediately before OnDie, base Defend can call `DuelLose` (`TPlayer.cpp:4302–4367`, next DeleteAllRecallMon). Even its false branch can send SM_DUELEND; an active duel cannot simply be ignored. Base Defend then sets internal HP to GetMaxHP before OnDie. That temporary value matters to the full ANGEL/duel graph but is not an extra heal packet for the bounded no-ANGEL/no-duel ordinary death.

The source PC Defend updates last-attack tick but does **not** reset recovery HP/MP clocks to now+5000. That reset belongs to DROPDAMAGE (`CSHandler.cpp:10608–10740`). Reusing the entire native DROP wrapper for Finish would therefore add the wrong clocks, DROPDAMAGE ACK and pre-death-MP packet semantics.

## Coordinated native transient-absence proof

This proof was presented to the root integrator before enabling final death. Root accepted it only for the current native-owned PostgreSQL PvE own-PC capability. It is not a conclusion drawn from absent DTO fields alone, nor from empty historical duel records. It has four connected boundaries.

### 1. Original construction and actual wire content

`CTMapSvrModule::Accept` allocates a new `CTPlayer` at `TMapSvr.cpp:1016`. Its constructor initializes active duel ID/type/target to zero (`TPlayer.cpp:115–117`), carried GodBall to zero (125), and clears actual recall/self/companion actor and companion-record maps (169–172). Base constructor (`TObjBase.cpp:6–80`) initializes local ID zero, normal mode, empty maintained/remain graphs and can-attack true.

The complete `OnMW_ENTERCHAR_REQ` (`SSHandler.cpp:1447–1631`, next CreateRecallMon at 1632) was read. It projects current cluster/social/mode/riding and actual recall summaries into an already found connection, then initializes recovery clocks. It carries no active duel ID/target/type, carried-player GodBall or self-object map.

The complete source `SendMW_RELEASEMAIN_ACK` (`SSSender.cpp:1999–2641`, next ChangePartyChief) was read. It carries core, skills/buffs, quests, bags/items, cooldowns, recalls, actual companion records/items and many other sections. Its duel section at 2511–2532 contains **historical DuelRecordSet/DuelScore**, not active duel state. It carries no active duel ID/target/type, player-carried GodBall or self-object collection. `OnMW_RELEASEMAIN_REQ:6823` hands off through that writer; `OnMW_ENTERSVR_REQ:3072` copies the body and queues the actual DM entry/load path. The common `OnDM_LOADCHAR_ACK:4424` consumes that graph, including actual local ID; it does not infer active duel state from historical results.

Transformation is not assumed absent merely because no independent transform byte exists: the source common load explicitly replays SDT_TRANS from actual maintained templates (`SSHandler.cpp:4973`). The native allowed maintained graph must therefore reject that restore path. Source `IsTrans:4533` reads maintained templates; source `ResetSize:4529` itself has an empty body, while SetTransAttack can modify actual attack capability.

### 2. Current native object creation, restoration and promotion

Native PostgreSQL startup selects `PostgreSQLMapService` and requires PvE mode, DB workers and World (`TMapSvrAsio/main.cpp:239` onward). The CMake target explicitly treats legacy_src as reference-only and compiles the native snapshot/handlers. The original CTPlayer/Win32 mutable object is not a hidden fallback instance for this path.

`PostgreSQLMapService::LoadAuthorized` in `services/postgresql_map_load.cpp` validates the native account/claim/catalog and restores a matching native full checkpoint or constructs a fresh CharSnapshot at 505 and fresh payload at 548. `HydrateTransfer:642` constructs from the actual typed core and preserves the complete transfer graph, including historical duel records and actual companion/recall vectors.

`handlers_native_entry.cpp:100` creates a new partial replica snapshot or projects an actual Loaded primary descriptor. It strictly parses the whole actual ENTER body and rejects recalls rather than inventing them. `handlers_world.cpp:427/430` directs both replicas and native payload primaries into this strict path, so the old no-payload partial descriptor adapter does not run on an admitted native primary.

`handlers_main_transfer.cpp:83` accepts full transfer only through actual native claim/receipt authority. After local operations drain, promotion at 113 grafts the actual target connection's cluster and recovery clocks into the hydrated native core. Capture/codec preserve actual typed source fields and reject trailing body data. They do not import hidden active duel, carried GodBall or self-object state. This is a closed native-owned process/receipt boundary; it does not prove a running original/legacy actor with extra transient state may be mixed into the modern authority.

### 3. No current native activation/fallback path

Actual source activations exist and were located: duel acceptance at `CSHandler.cpp:12274/12336`, carried GodBall at 13731/13760, self-object creation in `TMapSvr.cpp:7401`, and transformation through PerformSkill/SetTransAttack plus the maintained-load restore above. These are real responsibilities, not meaningless fields.

Both current native Map game dispatch (`handlers/dispatch.cpp`) and World inbound dispatch (`handlers_world.cpp::DispatchWorld`) were read. Neither registers the duel invitation/reply/end, GodBall take/mount/demount or relevant recall/self/transformation activation commands. Their default branches only log and return/break; they do not mutate character state or invoke a legacy handler. This absence is not claimed as protocol success or full feature parity.

Native ACTION explicitly validates its own supported profile. Ordinary SKILLUSE can accept costs for broader skill metadata, so it is **not** sufficient to say every ordinary accepted skill rejects recall. Actual native effect execution remains the all-CURE Finish profile; generic native `CS_DEFEND_REQ` throws before its prototype path when the actor has a native payload (`handlers/combat.cpp:305` onward). ACT profiles and native item/progress-effect writers admit their explicit pure/status capabilities, not richer entity/TRANS effects. Native startup also excludes prototype MonsterAI (`main.cpp:645`); its mutable cache producer cannot create an alternate native gameplay path.

`SupportedPostures` plus the actual pinned posture/progression template readers exclude maintained SDT_TRANS and arbitrary richer effects. Full supported statistics/death context separately refuses actual companions, companion items, recalls/recall buffs and nonzero local state. Existing party/social cluster writers do not create these transient gameplay entities. Consequently the original initialized absence is preserved on this current native-owned path; it is not a guessed replacement for a source field that has a reachable producer.

Any future duel/GodBall/recall/self/transform or persisted-import producer must add actual state/authority and extend this proof/guard before entering the shared death capability. Historical duel records remain permitted and preserved even when nonempty.

### 4. Fresh companion/ANGEL authority is not silently discarded

Source ANGEL reads `GetCompanion(slot):659` → actual `m_mapComp` → `GetPetItemUse:5626` → `GetPetItemKind:5638`. It does not read an arbitrary inventory item or TPET mount row.

The fresh native authority is `app_world`, created by migration012 and `PostgreSQLCharService::CreateOnce` (`TLoginSvrAsio/services/postgresql_char_service.cpp:110`, writes at 165–232). It creates the actual native character/children/start skills/items, possible start recalls and account pet rows. It does not create/import a companion record/item graph. A static schema search found companion records/items only in archived `legacy_game.TCOMPANIONTABLE` / TCOMPANIONITEMTABLE (`002-legacy-snapshots.sql:1574/1561`), not operational `app_world`. Archive rows carry import-run ownership and are not fresh native gameplay admission input.

Fresh LoadAuthorized reads operational app_world children into its new payload. It does not claim that real legacy-game companion rows were loaded and then replaced with zero. The non-PostgreSQL SociCompanionService is constructed only in the other backend branch (`main.cpp:282`), not as native PostgreSQL authority. Actual native TRECALLMONTABLE rows, including start recall rows if present, are explicitly rejected at fresh load606–607; they are not quietly removed to satisfy this profile. TPET mount history is a separate source graph and does not establish an ANGEL companion.

A full source DTO really can carry companion records and equipped companion items. The modern typed State and HydrateTransfer retain them. SupportedStatisticsContext and the shared death guard reject nonempty actual companion/item/recall/local vectors. Unsupported imported/full-graph rows are neither defaulted nor erased. If companion data becomes operational fresh authority later, a new actual input/guard is required; the present fresh-schema proof cannot be reused blindly.

## Actual death guards and current implementation review

The current implementation introduces shared private `RequireNativeOrdinaryDeathState`, `RequireNativeDeathMapContext` and `ApplyNativeOrdinaryDeath` in `services/postgresql_map_death.cpp`, with matching service-header declarations. The state guard requires actual payload, derived statistics/effects, entry clocks and hydrated cluster. It rejects actual guild, riding, tactics, arena/tournament, maps500..532, recalls, unsupported maintained state, and full-graph companions/items/recalls/recall buffs/local ID. It does not invent a duel-history-empty guard.

Lethal map context reads the existing verified migration070 `death_compat` release pinned to the actual routing manifest and counts all matching TBATTLEZONECHART map rows/global wID0 and TGODBALLCHART map rows in one SQL statement. Missing/unverified publication is different from a genuinely published empty relevant map graph. No map whitelist or fictitious castle/GodBall zero is used. Existing Map SELECT grant supplies this read; no new schema/publication/grant is needed, and no SQL was executed.

The Finish handler explicitly requires PvE. Existing ready-primary, account/session/current-owner, catalog, physical CellOwner, inventory/core checkpoint, accepted origin/route and FIFO one-use receipt checks precede effects. Signed Delta, actual source defaults, per-row RNG/modulo and current rank/maxima remain unchanged. The target loop now skips already dead occurrences before clock/hit/RNG processing. On a final HP0 occurrence it requires actual state/map proof, performs source pre-death zero-MP cleanup, encodes LASTHIT4 and conditional wire HPMP0/0, applies ordinary silent death and stages neighbor DIE. The final CheckEquipSkill loop remains outside and after the target loop.

A subtle native representation boundary was explicitly reviewed: zero-MP erasure calls RefreshEquipment, which can derive `bDead=1` from HP0 **before** source OnDie has been applied. The current occurrence therefore always applies its death helper when its captured lethal condition is true. A derived bDead flag is not mistaken for completed death cleanup. Only the following target occurrence uses dead filtering.

The shared state helper clears both bars, sets bDead, silently removes actual nonstatic maintained rows, retains pinned static postures and supported static800/802, then re-derives supported equipment/statistics. It adds no packet or recovery-clock reset. DROP retains its own existing +5000 recovery clocks, DROPDAMAGE/old-MP HPMP ordering and DIE audience while sharing state/map proof. Finish keeps its original last-attack clock and nearby/neighbor/private packet audiences.

`EncodeCureDefend` now selects LASTHIT from actual zero HP before MISS/NORMAL, while retaining the 84-byte source body, current rank/powers and zero CURE damage entries. `EncodeDieAck` retains the five-byte source body. Client `OnCS_DEFEND_ACK:2806` recognizes LASTHIT, `OnCS_HPMP_ACK:3492` assigns actual DWORD bars, and complete `OnCS_DIE_ACK:3552–3770` starts dead action/UI/revival presentation. The client resets local navigation/quest UI pointers; this is not a server quest mutation or automatic revival.

Core, finish receipt, ordered response/audience list, effects and transfer checkpoint are committed atomically before fanout. The existing standalone maintained writer versus full-graph Capture/StoreTransferCheckpoint paths preserve removed nonstatic effects and retained static duplicates. Entry projection already derives source TA_DEAD7 from bDead; no new action field or wire byte was added. Existing conditional HPMP→PublishPartyVitals and World actual-membership/current-pair authority relay the final dead bars without claiming a party from request metadata. Ready/open identity, local-party and fresh spatial recipient fences remain around awaited output, with existing no-replay/uncertain-commit handling.

Secondary Map server-cache convergence remains **OPEN**. Nearby/neighbor packets update the original client's visible state and may reach its ready secondary connections; that network fanout is not a writer for another Map process's local partial character snapshot. Party vitals likewise do not establish complete secondary gameplay state. This increment does not claim a new replica-death producer or general secondary combat reconciliation; native full-transfer/hydration authority and its existing boundaries remain separate.

The stable five-file production diff was directly reviewed: maintained_effect.cpp mode guard, finish_skill.h LASTHIT encoding, checkpoint.cpp ordering/filtering, postgresql_map_death.cpp shared helpers/DROP preservation and matching postgresql_map_service.h declarations. The completed new `map-death.md` and current Map README death section were also read. Actual non-invitation AuthorizeParty and current party/bar fanout allow dead actors under their existing ready/current identity checks; they do not require an invented second durable mutation. Standalone ordinal DELETE/INSERT and full-graph Capture overwrite the maintained vector from the committed payload, so silently removed rows are not retained for the next hydration.

No remaining critical source, type, declaration/caller or behavior blocker was found in this bounded static review. Status is **IMPLEMENTED / INTEGRATED_UNVERIFIED**. No compilation, SQL or original-client/runtime verification is implied, and no general source/legacy transient authority is claimed. The analyst's writes stop with this owned document.

## Deferred acceptance and remaining capability boundary

Phase B must run real original-client/PostgreSQL routes for lethal signed/positive-modulo HP results, mixed ordered rows and restored intermediate HP0; exact pre-death zero-MP cleanup versus silent death removal; LASTHIT4→conditional HPMP0/0→DIE order and distinct audiences; repeated target skipping before clock/RNG; CheckEquipSkill END/statistics after DIE; retained static800/802 and removed nonstatic ordinal/full-graph round trips; final party bars/current handoff recipients; and unknown commit/output without replay or refund of earlier ACT costs.

It must also distinguish unavailable/stale migration070 context from actual published map absence, reject all special/companion/recall/local graphs without data loss, preserve legitimate nonempty historical duel records, and verify that unhandled duel/GodBall/entity commands cannot activate hidden state. Source ANGEL, active duel, carried ball, castle/tournament/local, transformed/recall/self actors, foreign combat, hardcoded exception skills, general damage/XP/loot and original World admission/bootstrap remain outside this capability. Adding any such producer requires revisiting the closed native transient proof.

No original-client/runtime, source CRT/libm, original DB-only maintained-save projection or overall production parity gate is closed by this static increment.
