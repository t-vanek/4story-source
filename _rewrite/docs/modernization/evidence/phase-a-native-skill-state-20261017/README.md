# Ordered skill state, attack eligibility and disguise — Phase A, unverified

Baseline `5e835e3`, local `main`, 2026-10-17. Source ordered skill-data loading,
ordinary premium-template removal, CheckAttack, HaveDisguiseBuff/GetCountry and
replica effect authority now connect to existing native boot/host/NPC/entry/
demotion paths. Catalog/lifecycle connections are **INTEGRATED_UNVERIFIED**;
conditional monster status and disguise calculations are **IMPLEMENTED_UNVERIFIED**.
All populated-process admission and actual-player Initialize/ACK guards remain.
Full effect producers, hosted gameplay and Phase A remain **IMPLEMENTING**.

No build, test, SQL, migration, import, publisher, service, client, container,
runtime check, push or deployment ran. Only source/interface/SQL-text/publisher
AST/CMake/diff/inventory inspection and read-only backup hashing were performed.
New083 and SELECT grants/publisher are written only;044–083 remain unapplied,
next084. Private extracts, query-order inputs and pre-existing local
`_rewrite/docs/audits/` remain outside Git.

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
branches are not inferred. Existing native player learn/load/use/item readers
still need the same source availability filtering; their remaining raw-catalog
lookups are a concrete implementation gap, not proof those removed IDs are valid.

`deploy/sql/map-monster-skill-data-runtime-grants.sql` grants SELECT only on the
four new derived views. Neither migration, grants nor publisher was executed.
Historical `legacy_game`/`legacy_global` stay faithful. No live buff, country, job
or monster persistence table was added.

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

Next apply source premium availability consistently to native player learn/load/
use/item paths; then implement BeginAtk/CanSilenceAttack/SkillUse and actual host
movement/attack, reusing083 ordered data and existing source clocks. Connect
effect/replica mutation and expiry plus combat SetAggro and PC/loot-lock producers
before enabling coexistence. Inventory333 unique IDs/13 columns; DB map77 rows/6
columns. Counts are navigation metadata, not completion metrics. Next migration084;
Phase A is incomplete and Phase B/C remain deferred.
