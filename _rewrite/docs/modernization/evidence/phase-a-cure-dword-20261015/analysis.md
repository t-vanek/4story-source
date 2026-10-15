# Source audit: DWORD cure arithmetic and source default branches

Date: 2026-10-15. Baseline: local `main` at `cde2ded`. Scope: the existing admitted positive, pure, own-PC HP/MP CURE path for ordinary, loop and ACTITEMS casts. Three related changes cover DWORD combine arithmetic, actual source default branches and resource-addition wrap whose final per-Defend HP remains positive. This file records static source and current implementation review only. No build, test, SQL, import, extraction, service, server or client was executed; no migration was applied and no commit or push was made by the analyst. Private audits and older evidence were not read or modified.

## Source types and the Windows result projection

The complete `CTSkillTemp::GetValue` and `Calculate` bodies are at `Server/TMapSvr/TSkillTemp.cpp:62–105`; the next function, `IsMirrorType`, begins at 106. `Calculate` takes a `DWORD dwValue`, evaluates `int nCalcValue = GetValue(...)` before its switch, initializes signed `int nValue = 0`, and returns that signed result. `Server/TMapSvr/TMapType.h:1945–1955` declares actual chart fields `BYTE m_bInc/m_bCalc` and `WORD m_wValue/m_wValueInc`. The native equivalents in `Server/TMapSvrAsio/domain/skill_data.h` preserve those widths.

This is specifically the original Windows build contract, not a Linux `unsigned long` assumption. `Server/TMapSvr/TMapSvr.vcxproj:5–12` declares Win32 configurations, its toolset is v141, and lines 91/139 select `MachineX86`. `Server/TMapSvr/stdafx.h` includes `TNetLib.h`, whose lines 4–8 include the Windows socket/ATL headers. Microsoft documents [DWORD as unsigned 32-bit and INT as signed 32-bit](https://learn.microsoft.com/en-us/windows/win32/winprog/windows-data-types). The SDK definitions themselves are not vendored here.

Microsoft's [integral/arithmetic conversions](https://learn.microsoft.com/en-us/cpp/cpp/standard-conversions?view=msvc-170) describe the unsigned-long common type for mixed `int`/`unsigned long` arithmetic. Its [unsigned integral conversion table](https://learn.microsoft.com/en-us/cpp/c-language/conversions-from-unsigned-integral-types?view=msvc-170) documents preservation of the bits with the top bit becoming the sign bit, and equivalent conversion behavior for MSVC unsigned int and unsigned long. These official pages were directly read. Together with the original Win32 project, they support preserving the resulting 32 bits when the source assigns its DWORD expression to `int`.

`Server/TMapSvrAsio/services/skill_targets.h:41/46` already uses explicit `std::bit_cast<std::int32_t>` for this source conversion in another formula consumer. That existing implementation is corroboration, not the sole proof. The new cure implementation uses fixed-width types and explicit bit projection; it does not rely on a portable compiler's out-of-range unsigned-to-signed cast or introduce signed-overflow arithmetic.

## Exact arithmetic and its admitted final domain

Let `B` be the current effective source `GetMaxHP()`/`GetMaxMP()` DWORD, `V` the checked signed result of `GetValue`, `U(V)` its conversion to uint32, `low32` reduction modulo 2^32, and `S(bits)` the Windows signed interpretation of those same 32 bits. `SVI_*` values 1–5 are original `NetCode.h:1521–1528`.

| Actual combine byte | Source result before native positive-domain admission |
| --- | --- |
| INCREASE 1 | `V` |
| DECREASE 2 | `-V`; signed INT_MIN negation remains unsupported |
| MULTIPLY 3 | `S(low32(low32(B * U(V)) - B))` |
| DIVIDE 4 | First replace `V <= 0` with divisor 1, then `S(low32(B / uint32(divisor) - B))` |
| PRECENT 5 | `P = low32(B * U(V))`; compute `Q = int(P / 100.0)` by truncation, then `S(low32(uint32(Q) - B))` |
| Any other BYTE | `GetValue` still runs first; initialized result remains signed zero |

The product is DWORD arithmetic **before** the floating division in PRECENT. Computing a wide unwrapped product and then dividing, using a signed product, treating the result as an amount instead of a delta, or projecting to signed before `/100.0` would change the source result. Since the reduced product lies in 0..UINT32_MAX, its double conversion is exact and `P/100.0` is within the signed INT range; the implementation nevertheless checks before the INT cast. Negative `V` is first converted to DWORD by the actual source rule, rather than rejected as a negative factor.

These are static arithmetic examples, not executed tests or claims about published chart rows:

- `B=65537, V=65537`, MULTIPLY: product low32 is 131073; final delta is +65536. Product wrap is part of the source result.
- `B=0xfffffff0, V=65535`, DIVIDE: quotient is 65536; wrapped subtraction projects to +65552. Rejecting every quotient smaller than B incorrectly excludes this positive source result.
- `B=65537, V=100000`, PRECENT: reduced product is 2258732704; quotient truncates to 22587327; final delta is +22521790.
- Unknown calc produces `V=0`; known MULTIPLY with `B=UINT32_MAX` consequently gives final +1. Unknown calc does not imply an unconditional zero final delta.
- Unknown combine always returns final zero after evaluating its actual `GetValue`, independent of B.

The implementation admits only final signed results in `0..INT32_MAX/15` (143165576). This is the existing all-roll-safe source `nValue * (rand()%16)` bound; it is applied **after** modulo and signed bit projection. Negative signed final results remain outside the positive cure capability, even if they have large positive DWORD representations. `SVI_DECREASE` of INT_MIN remains rejected because the original unary minus overflows signed INT; this increment does not invent modulo semantics for that separate operation.

The follow-on third change also ports DWORD addition to the actual HP/MP bar for living completions. The shared Finish now reduces each sampled resource addition modulo 2^32, accumulates ordered rows, then clamps to effective maxima after all rows of that Defend. It admits the result only when final HP remains positive. The signed variance bound still covers all possible rolls before any addition; the final living-state condition is checked against the actual sampled outcome. No source death branch is invented for final HP zero.

## Default calc/combine are real row behavior

In the complete source `GetValue` switch, calc 0/1/2/3 have explicit branches and every other BYTE reaches `return 0`. Calc 0/1/3 retain the source WORD/BYTE-to-INT arithmetic, including rank zero. Calc 2 retains actual outgoing start/next/formula34 metadata and the existing finite/range checks before FLOAT/INT conversions.

The complete source `Calculate` initializes zero and its unknown-combine default only breaks. It has already called `GetValue` at that point. Thus an invalid/nonfinite/out-of-INT Calc2 result cannot be bypassed by choosing an unknown combine byte. Conversely, a source unknown calc byte is a documented zero branch, not absent data and not grounds to replace a missing chart/template with a default. Both raw bytes remain actual values read from the pinned TSKILLDATA row.

The original `CTObjBase::PerformSkill` function is bounded by `TObjBase.cpp:3077–3929`, followed by `FindRemainSkill`. Its HP/MP CURE branches at 3553–3568 call `Calculate` for every actual row and still call `rand()%16` when the result is zero. They do not turn a default-valued row into an empty skill or skip Defend. The current Finish similarly calls its RNG once per actual row, including zero results. All existing nonempty SA_ONCE, SDT_CURE, HP/MP profile constraints stay in force. A zero-result nonempty skill can still update the Defend activity clock, perform zero-MP cleanup, return actual DEFEND and consume its existing accepted-cast receipt. Conditional HPMP is emitted only for actual resource changes.

MISS and an empty target vector have different source control flow: they do not call PerformSkill/GetValue/Calculate or sample cure RNG. This increment does not apply the default-zero branch by manufacturing a row for them. Actual profile/publication/route/receipt metadata is still required. An actual Calc2 row still requires representable formula metadata even when that particular completion skips magnitude evaluation.

## Third slice: living resource-addition wrap

`TObjBase.h:22–23` declares the actual resources as DWORD. In the complete PerformSkill function, the HP/MP branches at `TObjBase.cpp:3553–3568` use `m_dwHP/MP += nValue + nValue * (rand()%16) / 100`. The admitted final magnitude ensures this signed right-hand expression is nonnegative and all-roll-safe; adding it to DWORD then follows unsigned modulo 2^32. The source performs no clamp, death test, dead-status transition, zero-MP cleanup or resource reply between these pure HP/MP rows.

Base Defend performs its maxima/clamp and records its changed-resource flag only after PerformSkill returns (`TObjBase.cpp:1038` onward). It subsequently calls CheckEternalBuff, emits DEFEND and conditional HPMP, and reaches the death test only at 1265 onward. That later branch includes possible DuelLose and source OnDie side effects; an existing bounded DropDamage death handler is not proof that this entire Defend death branch may be reused without analysis.

Consequently a wrapped HP value of zero between two rows can be followed by another HP row that restores a positive value, with no intervening source death event. Likewise an intermediate zero MP value does not trigger cleanup if a later MP row restores it. Conversely, final zero MP is legitimate for a living player and must run the existing source zero-MP cleanup. The native completion now implements those distinctions:

- Each resource update explicitly narrows `uint64(resource) + sampled_delta` to uint32. The widened sum is far below UINT64_MAX, and narrowing preserves the source low32 result. A positive CURE template may therefore reduce actual HP/MP through wrap; that outcome does not change `IsNegative`, mode, damage classification or negative foreign-actor erasure semantics.
- No intermediate HP-zero rejection or `bDead` change occurs inside the row loop. The existing final clamp runs after all rows; a final HP-zero result then throws before zero-MP cleanup, packet publication or a finish receipt/core commit.
- The check is per actual Defend target, not after the entire outer FINISH target list. Repeated self targets correspond to separate source Defend calls; source OnDie from one call cannot be deferred until a later target happens to heal again.
- A supported living final zero MP passes to existing `ErasesAtZeroMp` processing, including the actual 800/802 `bIsuse`/template-duration metadata and supported postures. Existing per-row END, possible bars and private statistics precede the actual DEFEND/conditional HPMP as before. Intermediate MP zero does not erase anything.

The source maxima are independent of transient resource values in this admitted graph. `GetMaxHP/MP:2011/2019` call `Calc2ndAbility:1558`, whose HP/HPR and MP/MPR branches use `GetCON:1816` and `GetMEN:1855`. Those functions use base stats, aftermath, equipment and `CalcAbilityValue:1409`, not actual HP/MP. `TSkill::CalcAbilityValue:151` and `CalcCure:163` use actual template rows, instances and supplied bases, not the resource bars. Source `ApplyEffectionBuff:4247` is a maintained MAGIC-status calculation; that richer modifier is not admitted here. Current supported postures contain attack/defense modifiers, and 800/802 plus other admitted progress statuses have no maximum-changing abilities; actual guild/recall/companion graphs remain excluded.

Thus no stats re-derivation or dead-marker hydration runs on an intermediate zero resource. Supported zero-MP erasure re-derives only after the full row loop, with final HP positive. It does not lose the remaining static 800/802 marker through the native dead-effect filter. Richer effects that change maxima or depend on current resources remain outside this proof.

The final-death refusal is atomic for this modern Finish transaction. Even when earlier repeated-target responses have been assembled locally, none is published and no successful finish receipt/core mutation is committed if a later target reaches the unsupported final-death branch. The handler retains its existing exception policy: uncertain local state and connection close, with no invented success, death reply, new Finish opcode, refund or retry after an earlier accepted ACT/cast. This bounded refusal is not a claim of full source partial-result/death parity.

### Existing persistence supports the resource bits

No positive-only resource constraint or new database representation is required:

- Applied source text `database/postgresql/012-native-characters.sql:43–44` declares native TCHARTABLE HP/MP as INTEGER NOT NULL, without a nonnegative resource CHECK. Historical snapshot DDL also uses INTEGER; neither file was modified.
- `PostgreSQLMapService::WriteCore` in `postgresql_map_checkpoint.cpp:1552` onward binds HP/MP using `std::bit_cast<std::int32_t>`. `postgresql_map_load.cpp`'s U32 helper at 34 and actual resource loads at 526–527 convert the signed stored INTEGER back to the unsigned wire bits. An upper-bit resource value is not a negative gameplay resource or an invalid account identity.
- The existing fingerprint packs snapshot HP/MP as little-endian DWORD bits (`postgresql_map_checkpoint.cpp:43–60`); checkpoint core-state equality compares the same canonical stored signed representation (`018-native-map-checkpoints.sql`). It does not require an increasing bar.
- The finish receipt columns in `036-native-skill-finishes.sql:15–18` are BIGINT with 0..4294967295 bounds, and current Finish binds the actual DWORD resources widened to long long. These operational values intentionally differ from core INTEGER bit storage; no signed identity conversion is introduced there.
- Full-graph capture copies the actual snapshot (`main_transfer_runtime.cpp:36–46`), the codec writes its DWORD bars, and `StoreTransferCheckpoint` stores those bytes and their hash. Fresh/core and full-graph checkpoint paths retain the final bars, and existing hydration re-derives/caps against actual maxima. No new DTO field or stale pre-wrap resource projection was added.

## Effective maxima, effect ordering and unchanged boundaries

The complete original `OnCS_FINISHSKILL_ACK` is `Server/TMapSvr/CSHandler.cpp:20197–20665`, followed by `OnCS_REGISTERBR_REQ`. Its actual target loop calls `GetAtkHitType` then the PC Defend path; a missing/filtered target does not invoke a calculation. Its final `CheckEquipSkill` remains applicable to an empty target vector. Hardcoded 128/229 erasure and 3604 rank override remain excluded from the native admitted profile; they were not removed to admit more chart rows.

`CTPlayer::Defend` at `TPlayer.cpp:1884–1970` updates `m_dwLastAtkTick`, then calls `CTObjBase::Defend`. The complete base function is `TObjBase.cpp:767–1306`, followed by `GetFormulaInit`. It applies PerformSkill only on the non-MISS path, derives effective maxima and caps after the row loop, calculates its resource-change flag, calls `CheckEternalBuff`, then sends DEFEND/conditional HPMP. Its later MISS path forces 802. Positive own-PC CURE does not enter the negative foreign-actor attack-erasure/damage branch.

The calculation base is `GetMaxHP/GetMaxMP` (`TObjBase.cpp:2011/2019`), not current HP/MP, pure unequipped maxima, incoming wire powers or a caller-supplied percentage base. The supported native statistics context excludes actual guild/recall/companion-dependent formula graphs; admitted outgoing rows have no ABILITY effects. The retained supported postures affect attack/defense, not HP/MP maxima. Thus the current native maxima remain the appropriate effective source base across the ordered pure CURE rows and supported cleanup. This is a bound on the admitted graph, not a general proof for unported maximum-changing effects.

Pure CURE does not enter `IsDamageType` in base Defend; its damage accumulators stay zero. If the resource-change aggro loop is reached, it passes zero to `CTMonster::SetAggro`, whose complete entry at `TMonster.cpp:141` immediately rejects `nAggro == 0`. No NPC aggro or foreign actor state is synthesized by admitting modulo/default arithmetic.

Party/corps wire fields remain inert metadata for the admitted own-PC cure, not input to B or membership authority. Existing current local party projection, authoritative World membership, physical Map ownership, current PG claim and live recipient fences continue unchanged. No party zero, new stat field, assumed item, learned trigger rank, formula coefficient or success response was added.

## Current implementation and end-to-end static review

The bounded implementation consists of three production-file changes:

1. `Server/TMapSvrAsio/services/native_cure.h:13–64`: checked `SignedValue` keeps actual calc 0..3 and returns source zero for other BYTE values. `Magnitude` always obtains that result before its combine switch; its uint64 product is reduced to uint32, uint32 subtraction is explicitly projected with `bit_cast<int32_t>`, and unknown combine retains initialized zero. The uint64 product cannot itself overflow for two uint32 factors. The existing final positive/variance and signed INT_MIN negation checks remain.
2. `Server/TMapSvrAsio/services/postgresql_map_actitems.cpp:64–106`: only the calc/inc whitelist was removed from the existing nonempty pure-CURE profile reader. Actual raw bytes are retained. Existing profile, source route, metadata, graph, hardcoded-ID, learned-trigger/current-rank and 802 capability checks remain. Both init and ACT acceptance use the same existing preflight helper; no helper signature or new API was introduced.
3. `Server/TMapSvrAsio/services/postgresql_map_checkpoint.cpp:1344` onward: each actual sampled positive cure now uses low32 resource addition. The final per-target clamp is followed by a final HP-positive check before cleanup/emit/write. No intermediate zero check or new death state was added.

The actual native path remains `handlers/dispatch.cpp:138–139` → `handlers/maintained_effect.cpp::OnFinishSkill` at 98 → pooled service call at 113 → `PostgreSQLMapService::FinishSkill` in `postgresql_map_checkpoint.cpp:1233`. The service checks the ready primary claim, account/session, configured publications, source guild-skill negative proof, current checkpoint and physical CellOwner; refreshes actual stats; selects the oldest matching unused accepted cast; and reconstructs the current ordinary/loop or ACT profile. It then calls the same four-argument `Magnitude` for each non-MISS row at 1343. Finish rank/maxima are current and are not frozen from ACT's rank1 wire acknowledgement.

ACT init (`postgresql_map_item_use.cpp:125–127`) and actual acceptance (`postgresql_map_actitems.cpp:127`) call this shared preflight before action hooks, cooldown/item debit or accepted item-cast effects. Preflight skips numerical evaluation on an actual anticipated MISS. It does not promise that the current rank, mode, maxima or relevant context cannot change before Finish; Finish re-evaluates the actual accepted origin/current context. Invalid actual arithmetic can still refuse after an earlier accepted item/cast. Existing uncertain-output/no-refund/no-replay boundaries remain explicit.

The shared Finish still writes ordered responses/effects and the one-use finish receipt, core and transfer checkpoint in its transaction before returning its immutable snapshot. Handler fanout retains `DeliverNativeGridProjection`'s actual actor/recipient tuple, party projection, Ready/open session, PG recipient authority and fresh presence/Map/channel/3×3 plus 64-range checks around awaits. Conditional owner HPMP continues through `PublishPartyVitals`; request arithmetic bytes do not authorize membership. A source transfer-out can start while an operation drains, and a stale route can lose output after a committed result; this increment does not invent replay or a completion ACK.

No protocol field changed. The original client FINISH producer is `Client/TClient/CSSender.cpp:2980`: fixed 34-byte header plus five bytes per target, with raw party metadata but no calc/base/result authority. The original DEFEND consumer begins `Client/TClient/CSHandler.cpp:2806` and parses the existing 84-byte base plus damage entries. Pure CURE retains no damage entries and a successful perform flag even for a legitimate zero delta. The full HPMP consumer at 3492 assigns the actual four DWORD bars/maxima; `Server/TMapSvr/CSSender.cpp:1315` writes that 21-byte body. No synthetic HPMP packet is needed for a zero-delta result if cleanup did not change resources.

The production helper/profile diff and the follow-on checkpoint diff, the new `map-dword.md` and the current Map README arithmetic sections were directly read. The resource-addition extension is limited to living results; final death and negative final effects remain outside it. Static production review found no remaining critical blocker in these three changed files and their existing preflight/Finish integrations. No executable or runtime verification is claimed. Status remains **IMPLEMENTED / INTEGRATED_UNVERIFIED**.

## Deferred acceptance and remaining OPEN scope

Phase B must exercise both direct helper boundaries and the actual original-client request/PG/client paths. It remains unexecuted:

- All three unsigned combines with product and subtraction wrap separately/together, high-bit B/V conversions, positive final projections, negative final rejection and the final 143165576 bound; include PRECENT wrapping before double division rather than after it.
- Actual calc unknown with each known combine, especially zero GetValue followed by positive wrapped MULTIPLY, versus actual combine unknown with every calc. Invalid/overflowing Calc2 plus unknown combine must still refuse before source hooks/item debit in init/acceptance and before any Finish commit/output.
- Nonempty zero-result rows still draw RNG and complete Defend/receipt/clock/zero-MP cleanup; truly empty targets and real MISS do not evaluate GetValue/pow/combine or RNG. Include actual metadata reads and unavailable/richer 802 profiles without a fake fallback.
- Multiple ordered HP/MP rows, effective maxima versus depleted bars, modulo resource addition and one final clamp. Exercise a positive wrap that reduces a living bar, intermediate HP zero followed by positive HP, intermediate MP zero followed by positive MP without cleanup, final MP zero with supported 800/802/posture cleanup, and final HP zero with atomic refusal/no finish receipt or output. Repeated self targets must apply the final-death check per Defend call. Check unsigned high-bit core INTEGER round trips, BIGINT receipt values, current actual HPMP/party fanout and no negative damage classification.
- Ordinary/loop and ACT origins, missing/current/rank0 learned trigger, current rank/context changes after acceptance, matching FIFO one-use receipt, stale/current actor and recipients, party transitions and handoff/drain/unknown-output behavior.
- Source Windows versus native Calc2 pow/CRT/libm rounding near FLOAT/INT truncation boundaries remains a separate unproven numerical acceptance boundary; this integer/modulo increment does not change that engine or claim bit-exact libm parity.

Final HP-zero/Defend death side effects, negative final damage, signed INT_MIN negation, richer maximum-changing effects, foreign targets, source exception skills and unported combat graphs remain OPEN. Living resource-addition wrap is implemented within the explicitly supported graph; it is no longer an unconditional refusal. No new migration, catalog publication, grant, seeded data, backup edit or database execution is required by these three arithmetic changes. This does not close the overall unchanged-client production modernization gate.
