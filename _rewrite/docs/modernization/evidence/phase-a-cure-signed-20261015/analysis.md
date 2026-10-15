# Source audit: signed living CURE and the following Defend-death dependency

Date: 2026-10-15. Baseline: local `main` at `e2e4784`. This increment admits signed numerical deltas inside the existing odd-bPositive, pure own-PC HP/MP CURE capability, with final HP positive after each Defend's ordered rows and cap. It does not implement even-bPositive negative templates, foreign targets, general damage or final-death completion. The following death section is a source dependency handoff, not an implementation or acceptance claim.

Only static source, current production diff, declaration/caller, SQL-text and new evidence review was performed. No build, test, SQL query, import, extraction, migration execution, runtime, service, server or client was run. The analyst modified only this new file, made no commit/push and did not read private audits or edit historical source, client, protocol, migrations, backups or older evidence.

## Template classification is separate from numerical sign

Complete `CTSkillTemp::IsNegative` at `Server/TMapSvr/TSkillTemp.cpp:31–34` returns the inverse of `m_bPositive % 2`; a negative Calculate result does not enter that expression. `IsPositive` at 26–29 tests the narrower `SPT_POSITIVE` value itself; this increment does not redefine it to mean every odd byte. Here “positive-template” denotes the already admitted odd-bPositive profile, whose source IsNegative is false.

The complete `IsDamageType` at 118–132 requires an SDT_ABILITY row with a nonzero attribute and one of its damage/HP/MP ability execs. All admitted rows remain SA_ONCE / SDT_CURE / SCT_HP or SCT_MP. They therefore do not become a damage skill merely because their signed number lowers a bar. No OnDamage, damage distribution, damage-based item durability, counter-skill, life-steal, negative foreign-target erasure or damage-derived quest/XP operation follows from that number.

Complete `CTPlayer::Defend` (`TPlayer.cpp:1884–1970`, next `OnDie`) changes battle mode only when the template IsNegative is true. It always updates last-attack tick, then calls base Defend. Complete `CTObjBase::Defend` (`TObjBase.cpp:767–1306`, next `GetFormulaInit`) also conditions revival800 protection, damage processing and relevant foreign-target erasure on template properties, not the sign of a CURE number. Thus signed CURE does not acquire new battle/recovery clocks or attack behavior. A source revival800 marker does not turn an odd-template CURE into a negative attack.

The base function initializes damage HP/MP to zero and calls CalcDamage/OnDamage only inside its IsDamageType block. Its later resource-change aggro loop still supplies zero damage-derived aggro for this pure CURE graph; `CTMonster::SetAggro` at `TMonster.cpp:141` immediately returns for zero aggro. No NPC registry mutation is fabricated for a signed resource change.

## Exact signed row arithmetic

Complete `GetValue/Calculate` are `TSkillTemp.cpp:62–105`, next `IsMirrorType` at 106. Signed GetValue, actual Calc2 coefficient/growth, all five combine modes and Windows DWORD bit projection remain unchanged from the admitted source arithmetic. `SVI_DECREASE` of INT_MIN remains unsupported because its source unary negation overflows signed INT. Unknown calc still returns actual zero; unknown combine still evaluates actual GetValue first, then keeps its initialized zero result.

The whole `CTObjBase::PerformSkill` at `TObjBase.cpp:3077–3929` was read by function boundaries. Its only admitted branches are HP/MP CURE at 3553–3568:

`resource += nValue + nValue * (rand()%16) / 100`

All operations on the right are signed INT. Multiplication precedes integer division, division truncates toward zero, and addition of that completed signed sample to DWORD then follows modulo 2^32. Negative signed values must not be converted to an unsigned type before variance division, floored, clipped to zero or converted to absolute damage.

Static examples, not executed test vectors or seeded chart rows:

- Delta -1, roll 15: `-15/100` truncates to zero, so the sample is -1.
- Delta -7, roll 15: `-105/100` truncates to -1, so the sample is -8.
- Resource 10 plus sample -1 becomes 9. Resource 5 plus sample -8 becomes DWORD 4294967293 before the final source cap; a max of 100 would therefore cap it to 100, not to zero.
- HP reaching zero between rows can be restored by a later actual HP row before Defend checks its final state. No intermediate dead flag or cleanup should be introduced.

The supported final row delta is the symmetric range `[-INT32_MAX/15,+INT32_MAX/15]`, or ±143165576. For every roll 0..15, the product magnitude is at most 2147483640, inside signed INT, and the complete sample magnitude is at most 164640412, also inside signed INT. A comparison against `-limit`/`+limit` avoids an unsafe abs(INT_MIN). This is a bounded all-roll source domain, not saturation or a claim to support source signed overflow outside it.

Only after calculating variance in signed int64 does the native code convert the sample to uint32. The following uint64 resource-plus-uint32 sum and final narrowing keep the source low32 result; even its maximum wide sum fits uint64. This implements signed subtraction, positive overflow and negative underflow consistently without signed UB. A numerically negative CURE remains a successful PerformSkill operation when the resulting actor is alive.

## Ordered rows, maxima and the living boundary

The complete PerformSkill has no hidden epilogue that reclassifies CURE by sign. Other recall/transform/HP-transfer/status/AI/item branches are excluded by the existing all-CURE profile, not silently executed or replaced. There is no clamp, death or zero-MP cleanup between its HP/MP rows.

Base Defend clamps to actual maxima at 1038–1048 after PerformSkill, computes its changed-resource flag, runs CheckEternalBuff, sends DEFEND/conditional HPMP, and only later reaches its death branch at 1265. The effective GetMaxHP/MP base is not current HP/MP: `GetMaxHP:2011` and `GetMaxMP:2019` follow formula/CON/MEN, equipment and maintained/passive calculations. Actual guild/recall/companion dependencies remain excluded; outgoing all-CURE has no ABILITY, supported postures 131/132 modify attack/defense and the admitted progression/800/802 markers do not change maxima. The base therefore remains valid across mixed signed rows and supported cleanup. No richer resource-dependent graph was admitted.

The existing native boundary remains per actual Defend occurrence: apply every ordered row, cap HP/MP, then require final HP>0 before cleanup/output/receipt mutation. Intermediate HP0 is allowed. A later target occurrence cannot rescue a zero-HP result of an earlier Defend call, because the source would already enter that call's death branch. Final MP0 remains valid and runs the actual supported zero-MP effect cleanup, with END/statistics output before DEFEND/conditional HPMP. Intermediate MP0 followed by restored MP does not erase an effect prematurely.

Final HP0 still refuses the entire modern Finish transaction. Previously staged target responses do not escape on failure. The handler's existing failure policy marks local state uncertain and closes the connection; it does not generate a successful Finish/death response, refund a prior ACT debit, retry uncertain writes or promise source partial-result parity for the excluded death branch.

## Stable production change and authority review

The stable three-file production diff, the new `map-signed.md` and the current Map README signed-cure sections were directly read:

1. `services/native_cure.h`: internal `Magnitude` becomes `Delta`, with a symmetric signed final-domain comparison. Checked SignedValue/Calc2, actual default branches, DWORD product/subtraction and bit_cast projection are retained. There is no abs, early sign clamp or new generic skill-engine behavior.
2. `services/postgresql_map_actitems.cpp:95–106`: the direct preflight caller uses Delta at the actual current rank and effective maxHP/maxMP. Init and ACT acceptance still call it before action hooks/debit. Actual anticipated MISS checks 802 capability and returns before numerical evaluation.
3. `services/postgresql_map_checkpoint.cpp:1343–1352`: the direct Finish caller uses Delta; variance is signed int64 including `/100`, then the completed sample is converted to uint32 for source modulo resource addition. The existing post-all-row cap and living check are unchanged.

A source-wide native Map search found exactly these two direct Delta consumers and no stale native_cure::Magnitude call. No public service signature, schema, grant, publication, protocol, DTO or CMake entry was changed by this slice.

The actual dispatch remains `handlers/dispatch.cpp:138–139` → `OnFinishSkill` at `handlers/maintained_effect.cpp:98` → service Finish at `postgresql_map_checkpoint.cpp:1233`. Its claim/account/owner/current session, source guild-skill negative proof, pinned catalogs, checkpoint/inventory graph, CellOwner, accepted origin/current route, FIFO unused receipt and actual refreshed statistics guards remain. Current trigger rank is re-derived at Finish; ACT's rank1 wire ACK is not used as a learned cost/rank fallback. This change does not add ordinary cast cost or reuse to ACT.

Every non-MISS actual row, including a zero/default result, still draws independent RNG. Actual MISS/empty target completion skips GetValue/combine/pow and RNG; required metadata and profile checks remain separate. Current effect/collision/ordinal/expiry writers and uncertain-output policy are unchanged. Both helper calls use current effective maxima, not wire party/powers or bars as the calculation base.

After the service transaction commits core/effect/finish/transfer state, existing output uses the captured party projection plus actual Ready/open actor/recipient tuple, PG recipient authority and fresh presence/Map/channel/3×3/64 fences around awaits. `PublishPartyVitals` relays the actual committed HPMP packet under current local party identity; World `OnPartyManstatAck` (`handlers/handlers_party.cpp:906–988`) authorizes actual membership and current pair identities before fanout. Numerical sign creates no membership or alternate target authority. A source transfer-out can begin while operations drain; committed state and stale output retain their existing no-replay boundary.

The original client DEFEND consumer (`Client/TClient/CSHandler.cpp:2806–3301`, next LEVEL at 3303) receives the unchanged zero-damage-entry CURE form. No damage entry is fabricated to explain a negative number. The complete HPMP consumer at 3492–3550 assigns the actual four DWORD bars/maxima and performs its existing UI/party relay behavior. Lower living bars and source-underflow-to-cap bars require no new wire format or client reinterpretation. Core INTEGER bit storage, U32 reads, unsigned BIGINT receipt bars and raw DWORD transfer encoding already preserve these final values; this increment does not change their numerical domains.

Static review found no remaining critical blocker for signed living completion. Status is **IMPLEMENTED / INTEGRATED_UNVERIFIED**; this is neither compilation nor runtime/original-client acceptance.

## Subsequent Defend death: exact source dependencies, not implemented here

The following is a bounded follow-up plan requiring root coordination. Removing the living check alone is incorrect.

### Original order and actor state

When final HP is zero, base Defend still performs pre-death zero-MP cleanup first. It sends the nearby DEFEND with `HT_LASTHIT` in place of the ordinary hit, and conditional HPMP with HP0 and **wire MP0**, even if the current internal MP before OnDie remains nonzero (`TObjBase.cpp:1171–1203`). It then deletes the instance skill, handles zero aggro, and reaches possible DuelLose and OnDie. A future implementation must preserve this packet snapshot separately from later death-state mutation; it must not simply call the living `EncodeCureDefend`, which currently encodes only MISS/NORMAL.

The actual condition at 1265 checks HP0 and status not already dead. If the PC has an active duel, `CTPlayer::DuelLose` (`TPlayer.cpp:4302` through the next DeleteAllRecallMon) can end/restore a duel or send SM_DUELEND before returning false. For an own host, the normal duel opponent differs from the supplied self ID, so even the false path can emit a duel message. Historical transfer duel records/scores do not prove active duel ID zero. The native Map graph currently has no active duel ID/target field or native duel gameplay producer in the searched dispatcher/services; a follow-up must explicitly prove the fresh/native-transient absence contract or introduce actual authority rather than guessing from empty historical records.

If DuelLose did not handle it, the source sets internal HP to `GetMaxHP()` before calling `OnDie(dwHostID, OT_PC, 0)` for this own-PC origin. That temporary value must not be published as an extra heal. Unlike DROP, positive-template PC Defend only updates last-attack tick; it does not reset recovery HP/MP clocks to now+5000. The later OnDie body contains no such reset either.

### Complete player/base OnDie graphs

Complete `CTPlayer::OnDie` is `TPlayer.cpp:1971–2142`, followed by GetLeftTick at 2144. Its branches are:

- Outside arena/team/tournament, actual equipped companion item kinds may contain ANGEL. `GetPetItemKind:5638` obtains those kinds from `GetPetItemUse:5626`, which calls `GetCompanion(slot):659` on the actual companion record map. ANGEL may restore full HP/MP, send neighbor HPMP and ITEMUSE success, arm its source cooldown and return **without death**. Missing/expired cooldown and BOW delay have their actual separate rules. No companion record/item or cooldown may be replaced by zero. Current strict no-companion graph can supply a bounded absence proof if carried into the future shared death guard.
- `DropGodBall:2175` mutates the actual carried ball/map registry and sends World gameplay state; `MapKillPoint:2149` depends on actual map castle battle status and tournament-player registry. The module also later calls `FindLocal(m_wLocalID)` and mutates local kill counts. Existing migration070 verified map/global-ID0 absence proof, actual local_id=0 and non-tournament bounds must be reused/verified; route ownership alone does not prove these branches absent.
- Recall actors, companion actors and self objects die and generate their original World removal calls before base death. Actual empty graph/native producer capability must be proven; silently clearing a nonempty rich graph is invalid. Source pickup, transformation and dependent entities remain unported bounds.
- Complete `CTObjBase::OnDie` (`TObjBase.cpp:2379–2484`, next FindTSkill at 2486) sets TA_DEAD/OS_DEAD and both bars zero, contains BOW/BR bookkeeping, resets transformation attack state through `SetTransAttack(0):4361`, silently releases nonstatic maintain rows, then sends five-byte DIE to neighboring cells. `ReleaseMaintain:4928` retains static effects and defaults to no notification. Retained static800/802 must survive; nonstatic rows must disappear from the following core/ordinal/full transfer checkpoint without invented END/stat packets.
- Player arena completion, local kill bookkeeping and PvP kill event follow base death. For own host `dwAttackID == m_dwID`, the final PvPEvent condition explicitly excludes self. Neither complete OnDie body independently deducts XP, deletes inventory or progresses server quests for this ordinary self-CURE. Pure CURE did not call OnDamage, so foreign damage/kill-quest/loot producers must not be synthesized.

The source client full DIE handler is `CSHandler.cpp:3552–3770`, followed by EXP at 3772. It sets HP0, starts the actual dead action and revival/UI behavior, clears local auto-path/quest-navigation pointers and may offer an existing revival item. That is not a server quest deletion or proof of revival completion. LASTHIT alone is not a substitute for DIE; HPMP alone is not a substitute for the correct death action. Native client entry projection already maps bDead to source TA_DEAD7 in `services/client_senders.cpp:299`.

### Existing native helper and a concrete future implementation boundary

`services/postgresql_map_death.cpp::DropNativeDamage` already performs ordinary PvE actor/graph/CellOwner checks; lethal requests additionally read verified `death_compat.catalog_release`, TBATTLEZONECHART map/global-wID0 rows and TGODBALLCHART map rows in one SQL statement. It then implements the **DROP** order: DROPDAMAGE ACK, HPMP with pre-death MP, recovery-clock +5000, silent nonstatic effect removal/dead state and DIE. Its whole function was read. Those DROP-specific packet and clock rules differ from Defend and cannot be copied into Finish as one call.

A coherent next slice would separate reusable actual death-context proof and ordinary death-state/effect mutation from origin-specific packet/clock production. Finish would retain its current transaction and accepted receipt, produce exact pre-death LASTHIT/HPMP snapshots and audiences, invoke only a proven no-ANGEL/no-duel/no-rich-entity/no-special-map state helper, retain source current clocks, append DIE with neighbor audience, and store final dead core/effects/full graph atomically. It must also model subsequent repeated target filtering against the now-dead actor, preserve source zero-MP-before-death versus silent nonstatic-death ordering, and publish actual party HP0/MP0 under the same current World authority.

Source nearby DEFEND/HPMP and neighbor DIE have different audiences. The intended helper must keep per-packet recipient metadata; it must not promote all packets to a single audience or replay unknown output. The existing Finish writer can persist effects/core/transfer state, but the exact dead-state profile and grant/query coverage must be reviewed before enabling it. If an active duel, ANGEL, carried ball, castle/tournament/local, transform, recall/companion/self-object branch cannot be proven absent or fully implemented, that actor remains unsupported. This plan does not authorize or claim any death code change in the present increment.

## Deferred acceptance

Phase B remains unexecuted. Required cases include signed row deltas across all Calc/combine/default modes, current rank/max changes, ±143165576 and just-outside refusals, signed division examples around zero, DECREASE INT_MIN and unsafe Calc2 refusal, actual modulo underflow/overflow, mixed signed ordered HP/MP rows, restored intermediate HP0, per-Defend final HP0 atomic refusal, final MP0 effect cleanup and true conditional packets.

Run the actual original-client/PG ordinary, loop and ACT receipt routes; verify no fake negative-template damage/aggro/mode behavior, default-zero RNG, MISS/empty no numerical work, current party/bar fanout, save/full-graph transfer, duplicate/stale/unknown-commit handling and no refund/replay for earlier item acceptance. Future final death requires its own acceptance cases for LASTHIT→HPMP0/0→DIE order/audiences, silent versus zero-MP effect removal, retained static800/802, current death environment and client revival presentation. No final-death or full source parity gate is closed here.
