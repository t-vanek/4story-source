# Ordered skill state, attack eligibility and disguise — Phase A, unverified

Baseline `504511b`, local `main`, 2026-10-17, continuing the ordered skill-state
work introduced from `5e835e3`. Native player load/full-graph receive, trainer,
quest grants, item/ACT and automatic postures now share ordinary source skill
availability. Real player data consumers use the same immutable ordered vectors;
initial admission normalizes only omitted operational rows in its checkpoint
transaction. These connections are **INTEGRATED_UNVERIFIED**, within the existing
supported player/stat/effect scope. Previous CheckAttack/disguise/replica work
retains its documented bounds. All populated-process admission and actual-player
Initialize/ACK guards remain. Full effects/hosting and Phase A are **IMPLEMENTING**.

No build, test, SQL, migration, import, publisher, service, client, container,
runtime check, push or deployment ran. This continuation used source/interface/
SQL-text/CMake/diff/inventory inspection and read-only backup hashing only.
No new schema migration: existing083 and its SELECT grants/publisher remain
written only; the added `map-player-skill-runtime-grants.sql` is also unexecuted.
044–083 remain unapplied, next084. Private extracts, query-order inputs and
pre-existing local `_rewrite/docs/audits/` stay outside Git.

## Authority and provenance

Original inspected server/shared/client source revision remains
`c77f79017d7940c1500ba36a5dbf8bda6fefb67c`. Reference trees, attributions, headers,
backups and old migrations were not edited. TVERSION0x2918 is a source marker;
matching executable/assets/region and actual original-client compatibility remain
**UNVERIFIED**. The implemented availability branch is ordinary non-BOW/non-BR.

Actual local backups were rehashed without restoring/interpreting their data:

| Archive | SHA256 |
|---|---|
| TGAME_RAGEZONE.bak | `2cff1635d523322d3de8ddafaeacc3335581dc338dca901239ac80288f51d259` |
| TGLOBAL_RAGEZONE.bak | `fca545bcb90bcc72aa4f3514a6177c2b88d2b0bf71bf847b0128f090a20877fb` |

Complete private historical manifests and original per-query receipts remain
unavailable in the checked publication paths.083 additionally needs every source
CTBLSkillData result order, including explicit empty results. No order, row count,
premium skill ID or historical content was invented from the archive filenames.
The new publication validates supplied evidence; a permutation alone cannot
establish that it was the actual original query result. Client compatibility and
historical runtime behavior still require later independent evidence.

## Source to actual modern paths

| Original source / symbol | Modern implementation / connected consumer |
|---|---|
| `DBAccess.h1934::CTBLSkillData`; `TMapSvr.cpp2775::LoadData` |083 `map_skill_data_catalog/queries/order`, `monster_compat.skill_data_release/queries/data`; `LoadNativeSkillData` runs after actual skill-template loading and before source allocation/Initialize |
| `TMapSvr.cpp4282+::CTBLPremiumSkillChart` ordinary branch; `DBAccess.h4375` | complete archived TPREMIUMSKILLCHART proof/view; loader removes actual matching runtime template/data entries after data loading, before learned monster objects are created |
| `TObjBase.cpp2594::CheckAttack`; `TAICmdChgMode.cpp::CanRun` | `NativeMonsterAttackAllowed`; existing executor CheckAttack/CHGMODE consumes all real maintained skill-data vectors |
| `TObjBase.cpp4122/4140/4962::HaveDisguiseBuff/GetCountry/GetWarCountry` | `NativeHaveDisguiseBuff/NativeDisguisedCountry`; actual `NativePlayerCountry` callback reads current accepted player snapshot and the same immutable ordered catalog |
| `TNpc.cpp80::CanTalk` and HaveDisguiseBuff caller | `NativeNpcCanTalk` now uses the same source ordered disguise result instead of DISTINCT/unordered SQL or conflicting-row rejection; existing quest/shop/trainer paths consume it |
| `TObjBase.cpp::CTObjBase`; `SSHandler.cpp::OnMW_ENTERCHAR_REQ/OnMW_RELEASEMAIN_REQ`; `CSHandler.cpp402::OnCS_CONREADY_REQ` | `CharSnapshot::replica_effects`; native replica entry establishes actual constructor empty vector; confirmed demotion retains current effects and aid before dropping primary payload; promotion uses incoming full payload |

`native_skill_state.cpp` and `postgresql_map_skill_data.cpp` are registered once
in the existing Map CMake target. There is no new service, actor registry or
parallel gameplay implementation. Existing explicit wire encoders remain intact.

## PostgreSQL and offline publication contract

The source query returns eight fields in actual row order for each WORD skill
key; it has no ORDER BY.083 retains the six BYTE fields plus two WORD fields,
including unknown enum values. Source SMALLINT bit patterns are preserved.
NULL/range failures are rejected; missing query authority is distinct from an
explicit successful empty result. Source SQL-open failure is not converted into
an empty catalog or successful native startup.

`tools/database/import_map_skill_data.py` is an offline Phase B tool, written but
not invoked. Its private order JSON has exactly `source_manifest_sha256`,
`server_id`, `allocation_order_sha256`, `source_revision`, `skill_data_queries`.
The final object maps canonical unsigned WORD skill keys to their original result
sequence of full archival row identities. Every actual source skill template
needs an explicit list; zero-row queries need `[]`. These identities bind rows,
and do not provide an inferred SQL result order. Duplicate keys/IDs, omitted
queries, wrong membership, NULL/type failures, manifest/revision drift and
unavailable sources refuse. Orphan TSKILLDATA rows stay archived: original
LoadData never queries them without a matching template.

The publisher requires the complete pinned historical manifest,079/082 equality,
full/active TSKILLCHART and TSKILLDATA row/value/byte equality, and faithful full
TPREMIUMSKILLCHART values/bytes. It checks actual archive ordinals as identities,
uses existing offline-owner/advisory fencing and table locks, publishes the
release/query/order records atomically, and refuses replacement or uncertain
commit retries. Error output suppresses private data. No original historical
table is modified and no SQL Server runtime dependency is introduced.

083 release visibility checks matching source/character/allocation identity,
source receipts/counts, one query per existing skill, query membership, positive
contiguous query positions, unique archive-row ownership and complete valid row
coverage. Grouped count readers avoid a full source scan per skill query. Native
boot checks all declared counts/order and preserves present-empty vectors; bounds
are65536 skill queries and one million loaded data/premium rows. Later existing
owner/battle/AI reads pin the cached order hash as well as previous releases.
Missing083/publication/grants prevents native boot; no lower-profile fallback.

The loader mirrors a previously omitted ordinary source branch: after loading
skill data, TPREMIUMSKILLCHART erases all matching runtime templates regardless of
bType. Repeated/missing IDs are harmless; data and template ownership are retired
together, without copying the original leak/dangling-pointer behavior. BOW/BR
branches are not inferred. Native player load/receive and existing learn/use/item
paths now use that exact surviving set, as detailed below. Remaining raw
TSKILLCHART lookups read actual scalar definitions only after proving runtime
presence; missing observed definitions refuse as catalog inconsistency. The raw
historical/character views themselves are not filtered or rewritten.

`deploy/sql/map-monster-skill-data-runtime-grants.sql` grants SELECT only on the
four new derived views. Neither migration, grants nor publisher was executed.
Historical `legacy_game`/`legacy_global` stay faithful. No live buff, country, job
or monster persistence table was added.

## Player availability, ordered consumers and atomic admission

| Original source / symbol | Modern implementation / actual connection |
|---|---|
| `SSHandler.cpp4896/4952::OnDM_LOADCHAR_ACK`, global `FindTSkill` checks | `FilterNativePlayerSkills` in real `LoadAuthorized` and `HydrateTransfer`; missing templates are omitted from learned/effect state, existing rank including0 and raw DWORD reuse survive |
| `CSHandler.cpp2206::OnCS_SKILLBUY_REQ`; `TNpc` skill listing; `TPlayer::LearnSkill`; quest skill rewards | `PurchaseSkill`, `TrainerOffers`, `ChangeQuest` QT_GIVESKILL and `CompleteQuestRewards` RT_SKILL/UP check the same source availability before reading scalar metadata or granting/debiting |
| `CSHandler.cpp9264/9516/19875::OnCS_ITEMUSE_REQ/OnCS_ACTITEMSUSE_REQ` | IK_SKILL source absence returns IU_FULL3 without item debit; nonzero IT_ACTITEMS use value still initiates ACTION, later ACTITEMSUSE returns SKILL_NOTFOUND1; direct ACT checks presence before current statistics/Defend preparation, after riding/owner gates |
| `TPlayer::ChangeEquipItem/CheckEquipSkill/ForceMaintain` and source `FindTSkill` | `ReadPostures` and `ApplyEquipmentPostures` preserve missing automatic131/132 as no force; progression/revival/avoid template producers preserve source absence independently of unsupported present definitions |
| `TSkillTemp.cpp134/457::IsRemainType/GetItemProbBuff`; `TObjBase.cpp1409/4088/4545/4789::CalcAbilityValue/HaveStopRecover/IsActionBlock/HaveItemProbBuff` | player `DeriveStats`, `RefreshEquipment`, `ReadNativeMultiAttack`, `ReadActItemSkill`, item upgrade/refine/economy, quest EXP, recovery/party and action-erasure predicates consume exact cached083 vectors; no invented column sort or conflicting-first-row rejection |
| `SSHandler.cpp7114/7131`; `DBAccess.h5389/5408::CSPSaveSkill/CSPSaveSkillMaintain` sends loaded objects | `NormalizeAdmissionSkills` runs within actual `MarkReady` before initial core/child/recovery checkpoint; `StoreTransferCheckpoint` uses existing canonical `Capture` for full graphs |

`LoadNativeSkillData` creates one immutable process snapshot of surviving skill
identities and all ordered data vectors after actual premium removal. Native
fresh and transfer payloads share ownership of it; snapshot copies retain a valid
lifetime without per-player catalog copies or references to a temporary SQL row.
Ready also requires that the payload owns this process's current snapshot.
The pointer is server-only, absent from client packets, original transfer bytes,
core fingerprints and database child state. Missing metadata authority throws;
a known missing template returns false; a present empty query returns a real
empty vector. No missing-template-as-zero skill is introduced. Generic prototype
loaders are not promoted to authoritative native gameplay.

Player attack type, cure row sequence and ordered ability calculations now read
actual vectors. Remain membership still requires SA_CONTINUE; for each learned
skill, source SA_CONTINUE then SA_PASSIVE traversal order is retained. First
GetItemProbBuff status wins within a template even if its value is0; the outer
HaveItemProbBuff skips that zero and continues to the next maintained object.
EraseItemProbBuff-style consumers retain presence separately from value. These
changes replace native SQL ordering guesses, not the source formulas or their
existing checked arithmetic limits. Full arbitrary skills/effects/stats remain
incomplete and guarded; explicit empty metadata does not imply successful combat.

Fresh load is still a repeatable-read observation with account/claim ownership.
Only if source filtering omits something, it captures complete raw skill/effect
JSON from existing `map_skill_state/map_maintain_state`; it makes no child writes.
The original raw maintained ordinal gap check precedes filtering. `MarkReady`
then locks owner/account/current-user and the exact loaded claim, pins083 via
`CheckCatalogs`, locks effects then learned rows in checkpoint order and compares
the complete raw baseline. Any rank, reuse, effect identity/order/duration drift
refuses. It validates surviving effects, deletes only source-absent learned rows,
checks exact surviving learned identity/ranks and writes dense surviving effect
ordinals with their original durable durations. CONREADY already samples elapsed
time: the existing checkpoint validates its maintained before-vector and stores
current skill/effect timers, including legitimate expiry, after normalization.
No cooldown is restarted or compared as if time had stopped.
Core, children, initial recovery receipt and ready phase commit in that same
transaction. Any exception rolls all of it back; disconnect before Ready cannot
invalidate the preceding logout receipt. Existing account/claim serialization
excludes cooperating gameplay writers during admission; external SQL drift is
not accepted as a legitimate actor event. PostgreSQL race/failure behavior is
unverified until Phase B.

The deliberate native persistence deviation is normalization at initial Ready,
so the established exact child-state recovery/checkpoint contract observes the
same objects as source FindTSkill. Original load skips absent templates and its
save sends only live objects; this change does not claim unavailable historical
procedure bodies or their cleanup/transact semantics were executed or proven.
Only derived operational rows are normalized; `legacy_game`, `legacy_global`,
original backups and reference sources are unchanged. It does not debit currency
or grant rewards for removed templates.

Full transfer/recovery hydration filters live `p.skills/p.effects` while retaining
the original immutable received graph. Existing `Capture` replaces its skill/buff
arrays with that live projection. Accepted promotion stores the canonical graph
in the same ownership/checkpoint transaction; recovered journal admission stores
it at Ready. Received transfer journals are not rewritten and uncertain-COMMIT
confirmation uses the existing receipt/fence path. Existing rejection of a
transfer whose actual HP/MP would be silently clamped remains. General effect
reconstruction and alternate tutorial/BOW/BR profiles remain separate open work.

Existing Map grants lacked DELETE on operational TSKILLTABLE. The new standalone
`deploy/sql/map-player-skill-runtime-grants.sql` adds that one privilege; native
construction requires it before allocating/listening. Existing SELECT083, child
write and checkpoint grants remain prerequisites. No schema objects or migration
number were needed; next084 is still free. Grant text and all C++ SQL strings
were inspected only and were never submitted to PostgreSQL.

## Calculations and object lifetime

CheckAttack first rejects actual getaway or canAttack0. It then reads every data
row of every current maintained monster skill in vector order. STATUS BLOCK3,
HOLD8, HYPNOSIS53, FLASH12 and DARK45, or AI RUNAWAY1, reject attack for OT_MON.
No action/rank/start/duration/expired-effect filter is added; source expiry is a
separate vector mutation. Unknown/unrelated data permits continuing. Missing
template/query authority refuses instead of becoming an existing zero-valued
template. The caller remains the actual CHGMODE eligibility branch, with its
original host/normal-mode/base-condition order.

HaveDisguiseBuff preserves maintained-vector order and the original per-skill
data order, returning the first TRANS DISGUISE_D4/C5/B6. GetCountry maps that result
to countries0/1/2, otherwise raw country; the source's repeated immutable scan is
equivalent to the shared result. GetWarCountry independently uses non-neutral aid
or raw country and does not use disguise. NPC access consumes the disguise code
itself. Multiple differing rows now follow supplied original order, with no
numeric sorting, DISTINCT collapse or invented preferred country.

Current accepted key/connection/role/epoch fencing still precedes host country
reads. Primary snapshots require actual payload effects. Partial replicas require
explicitly established local effects and a hydrated cluster descriptor; absent
remains unknown. Initial source replica entry carries no player-maintained list,
so its constructor vector is actually empty. During confirmed demotion the
existing sampled vector and actual aid country survive before payload reset;
promotion uses the newly accepted full graph. This repairs the previous loss of
local effects on demotion. The optional vector is object-local, appended after
existing C++ aggregate fields, absent from SQL/fingerprints/original transfer wire.

This lifecycle support does not implement general replica buff delivery or its
expiry/timer producers. Monster maintained-state creation/removal/stat derivation,
runaway/getaway producers, BeginAtk/CanSilenceAttack/SkillUse, combat SetAggro,
host movement, PC events/loot locks, non-PC/effect/transform/death/loot, dynamic
spawns and party instances remain incomplete. All player barriers stay.

## Deferred verification and exact handoff

Phase B must cover incomplete/empty queries, duplicate/orphan rows, signed WORD
IDs, NULL/unknown enums, source/active receipt and order drift, failed/uncertain
publication, native role grants, premium repeated/missing/all-type removal,
nonempty blocker and RUNAWAY rows irrespective of action/duration/rank, getaway
short-circuit, first disguise across effects and within one skill, raw country0
versus missing state, aid/disguise independence, NPC access, initial replica
constructor versus unloaded graph, demotion retention, promotion replacement and
future live effect mutation/expiry. No scenario was executed or promoted from
historical test results. Build and actual client/runtime acceptance are unknown.

Additional Phase B scenarios: no publication versus explicit empty query; all
ordinary premium bTypes/repeated/missing IDs; fresh/transfer saved removed or
never-existing IDs; rank0, unsigned high WORD IDs and DWORD cooldown bits; mixed
survivor/omitted effects and dense ordinal order; changed skill rank/timer or any
maintained field/order between load and Ready; nonzero CONREADY elapsed time and
expiry alongside omitted rows; added/removed rows; owner/session/
role/epoch revocation; missing/revoked DELETE grant; failure before delete, after
delete or during child/core/receipt/Ready write; lost COMMIT and recovery/transfer
confirmation; disconnect before Ready; subsequent timer/save/learn/inventory
exact-state checks; NPC offers, quest grants, automatic postures, absent IK_SKILL
without debit, nonzero absent IT_ACTITEMS ACTION then ACT NOTFOUND, removed3603
and existing3604; first data row effects/attack type, first matching status0 then
later conflicting row, next maintained object and item buff erasure. Existing
native fixtures may need complete083 evidence and the added role grant; they were
not run and no historical test pass was carried over.

Next implement source BeginAtk/CanSilenceAttack/SkillUse and actual host movement/
attack using083 ordered rows, the same source availability and original clocks.
Then connect monster maintained creation/removal/stat recomputation, replica buff
mutation/expiry, combat SetAggro with real damage/heal/skill producers and PC
Enter/Leave/channel/MoveGhost with MONITEMLIST/TAKE locks before coexistence.
Remaining subsystem inventory stays open; no guard was removed. Inventory335
unique IDs/13 columns; DB map78 rows/6 columns. Counts describe navigation scope,
not completion. Next migration084; Phase A is incomplete and Phase B/C deferred.
