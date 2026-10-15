# ACT own-PC CURE Calc2: source and integration review, 2026-10-15

Baseline: local `main`, `8ed424c`. Analyst ownership is only this new file. No build, test, SQL, migration, import, extraction, service, server/client execution, commit or push was performed. Private audits and older evidence were not read or changed. This is a bounded source/API/diff review, not executable acceptance.

The continuation admits actual exponential Calc2 for the existing positive own-PC ACT item CURE / SVI_INCREASE capability. The actual outgoing chart and published formula34 provide its coefficients; actual current trigger rank selects its exponent. It retains the source precision/conversion stages with explicit safe-domain refusals. Other combine modes, negative combat, arbitrary targets and outgoing128/229/3604 dependencies remain outside this increment.

## Complete original value calculation

`Server/TMapSvr/TSkillTemp.cpp:62–107`, complete GetValue and Calculate, establishes:

```
exponent = rank == 0 ? 0 : startLevel + (rank - 1) * nextLevel
scaled   = (WORD value * pow(FLOAT firstRateX, exponent)) / 100
nValue   = INT(scaled)
```

The multiplication precedes division; there is no integer division of `value` by100 before pow. Calc2 does not use `wValueInc`, trigger-template growth, character level as the skill rank, ACK rank1, or an assumed coefficient. Source rank, start and next are BYTEs promoted to INT. Rank0's special exponent is exactly0; otherwise the largest exponent is255+254×255=65025. There is no BYTE wrap of `rank-1` or exponent.

For SVI_INCREASE, Calculate returns GetValue's signed INT unchanged. The complete source PerformSkill's HP/MP branches (`TObjBase.cpp:3556–3568`) apply each ordered row as `nValue + nValue*(rand()%16)/100` to its actual DWORD resource. Defend caps resources afterward. An actual MISS skips PerformSkill, and an empty FINISH target list never calls Defend. Neither requires a pow/magnitude calculation.

The existing generic `services/skill_effect.h::GetValue` already represents the expression using std::pow and an INT cast, but its Calc2 conversion has no representability guard. Merely opening Calc2 in the ACT profile and calling that cast would introduce undefined/out-of-domain conversion for nonfinite or excessively large results. The new native checked wrapper handles this positive-INCREASE boundary; the generic engine and other combine formulas are unchanged.

## Actual coefficient authority and source precision

`NetCode.h:1303–1340`, FORMULA_TYPE starting at0, places FTYPE_1ST at34. `Server/TMapSvr/TMapSvr.cpp:2655–2674` reads CTBLFormulaChart and copies the actual FTYPE_1ST rate into local `FLOAT f1stRateX`; skill-chart loading then assigns `m_bStartLevel=query->m_bLevel`, `m_bNextLevel=query->m_bNextLevel` at2705/2707 and `m_f1stRateX=f1stRateX` at2730. These are the **outgoing** skill's fields for every loaded template. `TSkillTemp.h:37–39,72` stores BYTE start/next and FLOAT firstRateX.

The recovered SQL schema has `TFORMULACHART.fRateX float(53)` (`database/legacy-reconstructed/TGAME_RAGEZONE.tables.sql:590–595`). Source `DBAccess.h:81–105`, CTBLFormulaChart, binds that database column into C++ FLOAT. PostgreSQL's existing raw002 representation at1786 uses double precision, consistent with `tools/database/generate_migrations.py:51–58` mapping float precision>24 to double precision. Thus directly using the database double as a pow base would skip a real source FLOAT rounding step.

The implemented pipeline is: actual PostgreSQL double → finite/FLOAT-range validation → FLOAT rounding → explicit double promotion for pow → WORD-value multiplication → division by100 → checked INT truncation. It does not substitute SkillTemplate's constructor default1.0 or source loader's missing-formula initial0. Missing actual formula is unsupported authority. Only an outgoing profile containing Calc2 needs this additional metadata fetch; its actual bLevel/bNextLevel are read from that same outgoing row. Reading/checking metadata is distinct from evaluating the cure magnitude.

A late root review identified an upstream dependency: each actual caller first runs RefreshEquipment→DeriveStats→common `postgresql_map_load.cpp::ReadFormula`, which previously narrowed SQL-double formula34 before the ACT-specific guard. The stable patch now checks **both fRateX and fRateY** for finite source-FLOAT range before either common-reader cast. Existing cmath/limits includes cover that local conversion helper. This explicitly tightens all ReadFormula callers, including non-ACT statistics and even a read formula component not subsequently used by a particular expression; it does not introduce a coefficient fallback, a positivity requirement or a cure calculation during metadata reading. Representable signed values and source FLOAT rounding remain intact. The earlier upstream narrowing is therefore covered rather than relying only on the later ACT query.

All three tables already belong to the complete character publication. Their actual runtime views are `database/postgresql/017-map-actor-reference-catalog.sql:31–47`: character_compat TFORMULACHART, TSKILLCHART and TSKILLDATA join the active character catalog run.011 defines catalog authority, not those three views. `PostgreSQLMapService::CheckCatalogs` checks configured verified character/routing/actor manifests; existing `deploy/sql/map-runtime-grants.sql:5` grants Map SELECT on the character views. No new table, migration, publisher, seed, grant or import is required or executed.

The source project specifies MSVC toolsetv141; its actual CRT implementation/binary and the runtime rounding environment are not supplied by this source inspection. Existing native std::pow(float,int) and the new explicit double-base form both intend a double result from the source-rounded FLOAT. Exact original CRT versus Linux libm ULP behavior and truncation near integer boundaries are **OPEN executable acceptance**, not a bit-identical numerical claim. The inspected CMake options do not add fast-math; that text observation is not evidence of a particular built executable or its floating environment.

## Checked numerical domain

`services/native_cure.h::Magnitude` is a new private-use native positive-INCREASE helper. Calc2 checks finite FLOAT metadata, calculates its signed-INT exponent, then observes pow, product and scaled double. It checks finite intermediates and INT32 bounds **before any INT cast**. Source truncation is toward0. In particular a finite raw fraction such as-0.5 becomes INT0, not-1, and is not rejected merely because the raw fraction has a negative sign. A finite negative coefficient is not arbitrarily forbidden: integer even exponents can yield a valid nonnegative result.

After safe conversion, the helper admits only an INT magnitude in0..floor(INT_MAX/15)=**143165576**. This second bound is necessary: source `nValue*(rand()%16)` is signed INT arithmetic. Permitting any converted value up to INT_MAX and calculating its variance in the native uint64 representation would silently define outcomes where the original multiplication overflows. At this bound all possible variance rolls0..15, their product, division and final positive row magnitude fit INT32. Existing checked DWORD resource addition separately refuses overflow and retains normal resource capping.

The raw scaled INT32 check is conservative at the extreme conversion endpoints. That does not exclude a further admitted positive magnitude: all accepted values are already far below INT_MAX under the variance bound. Raw fractions in(-1,0) remain inside the conversion guard and truncate to0 correctly.

For Calc0/1/3 the helper reuses existing GetValue. Their WORD/BYTE domains already fit these conversion/variance bounds; rank0 arithmetic remains signed as before. It does not add a float path or growth rounding to these integer modes. No negative value is clamped, no exponent/rank is replaced and no overflow is treated as a successful0 cure.

Finite pow underflow to0 is an actual representable result. Pow/product overflow, nonfinite values, out-of-INT conversion, negative converted magnitudes and unsafe variance magnitudes are refused. Value0 does not justify bypassing an otherwise nonfinite pow/product, because the source still evaluates that expression. Original behaviors outside this explicit safe domain are not claimed.

## Actual production callers and ordering

`ReadActItemSkill` retains actual nonempty SA_ONCE / SDT_CURE / HP or MP / SVI_INCREASE checks and now permits bCalc0..3. It preserves every row's actual calculation/value/increment and reads formula34 plus outgoing start/next only if Calc2 is present. It does not evaluate pow or numeric magnitude. It preserves the already implemented current trigger rank, missing versus present-rank0 distinction, baseline critical selected by trigger attack type, and outgoing baseline power/attack-level selection. ACT acknowledgement remains rank1, critical0, zero powers and item-action provenance.

`PreflightActItemCure` remains shared by exactly the two actual initiation/acceptance callers:

- Type25 ITEMUSE initialization refreshes actual equipment/stat context, reads the outgoing profile and calls preflight before its ACTION-only output. There is no new debit or reuse at this initialization step.
- ACT acceptance refreshes actual context, reads the profile and calls preflight before its existing activity/action-erase/zero-MP hooks, deadline/item response and actual stack debit. A predicted GOHOME2 or critical255 MISS checks actual Avoid802 capability and returns **before Magnitude/pow**. Normal preflight calls the checked helper per ordered row at actual current trigger rank before those mutation hooks.

Accepted ACT Finish independently refreshes current context and re-reads the profile while validating its existing one-use receipt. It does not freeze growth/rank/critical from the acceptance ACK. Its current target loop invokes Magnitude only inside `!miss`, once per SVI_INCREASE row of each actual target occurrence, before variance draw or resource addition. A MISS or empty target list has no Magnitude/pow call. Actual formula metadata must still be available and representable; bypassing numeric evaluation is not permission to invent missing metadata or change its provenance.

The shared Finish's SVI_INCREASE branch now uses this same checked helper for already admitted ordinary/loop cures as well. This is scoped safe conversion/variance hardening of those positive rows; other ordinary combine modes still call the existing generic Calculate unchanged. The generic skill engine, ordinary cost/reuse/charge acceptance and non-INCREASE combat have not been ported or rewritten by this increment.

Any actual rank/equipment/context change after acceptance is evaluated at finish. If it makes a normal cure unsupported, the existing transaction fails without a new finish receipt, visible heal or refund/replay of the prior accepted item use. Local intermediate row changes are not output before commit. Current owner/account/session/claim, unlock, configured publications, physical CellOwner, graph/item/effect/cast heads, origin071 route/ground/ordered-target correlation, party tuple and post-await recipient projection fences remain unchanged. Finite validation adds no client fields, fake packet, target or membership authority.

## Static review conclusion and deferred acceptance

Final stable direct review covered all four changed production files: new native_cure.h, postgresql_map_actitems.cpp, postgresql_map_checkpoint.cpp and the upstream postgresql_map_load.cpp conversion repair. It covered the complete source GetValue/Calculate, actual metadata stamping and C++/SQL field widths, existing generic value engine, checked helper, actual profile reader, both preflight call sites and shared Finish branch. The actual publication views/grants and current CheckCatalogs text were inspected without executing queries. The new header's includes/types and call signatures match; its exceptions can propagate through the existing transactional refusal/uncertain-output handling. New Map README and `map-exponential.md`, including the explicit non-ACT formula admission tightening, were read directly and document the same scope and limits. No remaining critical blocker was found in this bounded static review. Status: **IMPLEMENTED / INTEGRATED_UNVERIFIED**.

PhaseB must exercise actual published coefficient/start/next/value data through original ITEMUSE→ACT→FINISH; source database-double→FLOAT rounding; missing metadata and unrepresentable/nonfinite FLOAT refusal, including upstream common ReadFormula X/Y for non-ACT consumers before narrowing; current missing/present0/presentN trigger rank; rank0 exponent0 and0/negative/tiny bases; integer truncation and negative fractions to0; finite pow underflow and nonfinite pow/product/scaled failures; boundaries near INT32 and143165576; independent row variance, caps and DWORD overflow; normal refusal before hooks/debit and post-acceptance context refusal without refund; identical unsupported numeric metadata through actual MISS/empty without pow; normal/MISS packet order and source802 lifecycle; shared ordinary/loop INCREASE hardening versus unchanged other combines; source CRT/libm near-integer differences; current authority/party/recipient movement/handoff, atomic checkpoint/recovery and uncertain commit/output without replay. These cases are specified, not run.

Other combine modes, negative/offensive damage, richer effect graphs, outgoing128/229/3604 side effects, general companions/recalls/guilds, original SQL save/filter parity, full World/Map bootstrap and original-executable acceptance remain OPEN. No new schema application or runtime parity is claimed.
