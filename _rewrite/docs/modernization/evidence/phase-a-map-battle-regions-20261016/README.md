# Map battle-region boot and actual readers — Phase A

Baseline: local `main`, `81191cc`. Pre-existing `_rewrite/docs/audits/` stays
untouched/outside Git. No applicable AGENTS.md was found. New078, grants and
offline publisher are **written only**;044–078 remain unapplied, next079.
Full Phase A and battle gameplay remain **IMPLEMENTING**. Only the bounded
boot/read paths below are **INTEGRATED_UNVERIFIED**.

## Authorities and source contract

Original Map and client file lineage is
`c77f79017d7940c1500ba36a5dbf8bda6fefb67c`. SKYGARDEN is defined by the original
Map build. TVERSION0x2918 remains a source marker; an executable/assets/region
profile and original-client compatibility are unverified. Original sources,
attributions and database archives are unchanged.

Actual immutable backup SHA256 values:

| Backup | SHA256 |
|---|---|
| TGAME_RAGEZONE.bak | `2cff1635d523322d3de8ddafaeacc3335581dc338dca901239ac80288f51d259` |
| TGLOBAL_RAGEZONE.bak | `fca545bcb90bcc72aa4f3514a6177c2b88d2b0bf71bf847b0128f090a20877fb` |

The existing reconciled `database-reconstruction/evidence/data-profiles.json`
records26 battle zones,12 local rows,4 castles,8 missions,1 SkyGarden,72 weekly
occupation rows,0 guild rows and20 instance-map definitions. These totals bound
the pinned profile; they are not source row contents, live ownership or new SQL
evidence. No private row data or query order was invented/read into Git.

- `Server/TMapSvr/DBAccess.h::CTBLBattleZoneChart` and `TMapSvr.cpp3097` load
  all24 fields, resolve item pointers by actual item existence and retain absent
  templates separately. SQL SMALLINT/INT bits become source WORD/DWORD.
- `CTBLMissionTable`, `CTBLCastleTable`, `CTBLSkygardenTable`, `CTBLLocalTable`
  and `TMapSvr.cpp3150–3440` resolve zone keys before allocating local objects.
  Missing zones are skipped, not fabricated. The castle/local guild LEFT JOIN
  can lack a name even when the original DWORD guild ID exists.
- `CTBLLocalOccupy` updates only an existing local and day>0; occupation rows
  first zero their7-day arrays, while other source local types do not initialize
  all scalar/array fields. Native optionals record those unknown fields instead
  of presenting zeros as source state. Day>7 source out-of-bounds writes refuse.
- `CTBLMonSpawn` filters with independent **server-only** map and unit IN sets.
  Unit calculation truncates FLOAT/1024 to signed SMALLINT before z*256+x;
  PostgreSQL's rounding integer cast is not equivalent. Native uses explicit
  truncation and refuses unrepresentable casts. Loaded spawn IDs establish boss
  availability for occupation validity; ordered WORD-map iteration fills each
  zone's spawn list. This is template binding, not realized monster/AI support.
- `TMapSvr.cpp::InitEnvironment/FindTChannel/FindTMap`, `TChannel.cpp::FindTMap`
  and `CTBLIndunChart`: DEFAULT_CHANNEL1 is logical. Native resolves its actual
  physical channel with the existing076 topology and builds owned map/unit
  environments. A fresh instance template has no party0 instance. Castle and
  SkyGarden map bindings follow the original per-query order, including last
  assignment when multiple rows bind the same map. Mission validity also needs
  that actual default map. Occupation validity instead needs its boss template.
- `Lib/Own/TNetLib/TNetLib/TNetDef.h::__DBTOTIME`: year<2000 means0. Other
  source SMALLDATETIME values require the explicit same-world077 calendar.
  Ambiguous/nonexistent DST times refuse; Python/PG epoch agreement must be
  checked by the future publisher. No implicit host TZ or UTC assumption.
- `CSHandler.cpp14862::OnCS_HEROLIST_REQ`, `CSSender.cpp5013` and
  `Client/TClient/CSHandler.cpp14161`: BYTE local count, every WORD local ID
  and original CString hero, then the same castle section. No guild, tutorial,
  death, validity or empty-name filter. Source MAX_NAME is **50**, not20.
- `TMapSvr.cpp3629::FindOccupationZone` binds an NPC only to a nonzero,
  valid occupation object. `TNpc.cpp80::CanTalk` uses that country's actual
  state for neutral NPCs. Friendly country/aid and disguise checks are kept;
  a hostile player needs the territory's disguise. Missing/invalid/wrong-type
  local leaves the NPC neutral. `TObjBase.cpp4122::HaveDisguiseBuff` traverses
  maintained effects and the template's original data. CTBLSkillData has no
  ORDER BY: differing disguise execs within one template require source order
  evidence and now refuse, rather than choosing the smallest numeric exec.

## Production implementation and database path

078 adds the explicit routing/character/content/calendar-bound release, eight
source receipts, native `app_world.battle_regions` and `battle_occupation_days`,
and read-only chart views. Nine routing/item/NPC/skill/spawn dependency contracts
also require equal full-source and active-subset value/byte hashes and counts;
matching backup/metadata names alone do not establish that equality.
`import_map_battles.py` requires a full verified
pinned historical manifest and every real source row/byte sidecar. It checks
counts, values, original ANSI bytes and exact archival ordinals under offline
Map/World barriers and stable table locks, then atomically seeds the native
rows with their source run/row and query order. Existing publication refuses
replacement; a failed or unknown commit is not retried. No historical rows or
earlier migrations change. Guild IDs remain explicitly **source identities**,
not an invented native guild mapping or permission to mutate guild ownership.

The required private `--source-order` JSON contains `source_manifest_sha256`
and `queries`: exactly `TMISSIONTABLE`, `TCASTLETABLE`, `TSKYGARDENTABLE` and
`TLOCALTABLE`, each mapped to a permutation of all that table's real archival
row ordinals. It describes the corresponding original CTBL query result order.
Its actual SHA256 is saved. Supplying a file does not prove original order or
client compatibility; that evidence remains required in Phase B.

`PostgreSQLMapService` now acquires the existing Map owner transaction before
startup reads and calls `LoadNativeBattleRegions` before accepting TCP. It owns
the immutable `BattleRegions` graph: zone metadata, map/unit environments,
source guild/name/hero/time fields, per-type source order, weekly data, boss
availability, zone spawn IDs and castle/local membership. Counts must match
the publication; missing release/data never becomes a successful empty world.
All three new translation units use the existing `tmapsvr_asio_core` target.

`handlers/dispatch.cpp → OnHeroListReq → BattleHeroList → HeroList` is an
actual request path. Existing primary/Ready gameplay lease, real map presence,
DB owner/account/claim and release checks guard it. The worker returns an owned
buffer. Open/Ready/role/key/connection/epoch, World registration and current
boot authority are checked again before sending. Serialization explicitly
emits little-endian WORD and signed32 string lengths plus original ANSI bytes;
no native C++ object layout is copied. Counts>255 refuse instead of truncating
the count while sending additional entries. A genuine published empty list
can be sent; missing data cannot produce one.

`BattleRegions::CanTalk` and `NativeNpcCanTalk` feed existing fenced
`ChangeQuest` NPCTALK, `PurchaseSkill`, `ListNativeNpcItems` and ordinary
`BuyNativeItem`. They use the actual boot occupation binding and current
maintained vector. NPC talk uses the post-expiry snapshot, not an old effect
graph. Existing money/item/learning transactions and source response paths
remain in use. MAGICITEMBUY retains the source's absence of a CanTalk call.
Guild/hero discounts remain separately unsupported where their authority is
missing; this change does not grant zero discounts or native guild membership.

Static review found that the previous main callback swallowed dispatch errors:
the earlier enable refusal therefore did **not** actually reach socket closure.
This increment fixes that integration: its scoped operation counter unwinds,
the error reaches `AsioWorldClient::Run`, and the existing disconnect callback
closes clients and awaits durable saves. No successful disconnect is claimed
without the deferred runtime evidence.

Unported enable, occupation, hero and castle-guild mutations explicitly retire
both the World link and the boot graph's authority. Retirement is atomic across
DB workers, permanent for that process, and checked by readers. Reconnection
cannot silently turn a stale boot graph into current ownership. Loss of an
already registered World connection also retires this authority; unsuccessful
initial dials before registration do not. This protective
refusal can drain clients and is **not implemented battle/mutation support**.
Map workers are joined before service destruction; no new background task,
service or alternate server architecture is introduced.

## Open implementation dependencies and deferred verification

The existing generic static spawn population remains a prototype and native
monster AI remains disabled. Full spawn/group/party/event/regen selection,
source monster IDs, switches/gates, GodBall, occupation writes, rewards,
hero selection/THeroSelect, guild identity/lifecycle and all four enable
consumers remain open. Current boot state does not authorize those mechanics.
Original MW_CONNECT_ACK/full PostgreSQL-only World bootstrap, initial replay,
other timers and end-to-end battle/client compatibility remain unfinished.

| Future Phase B scenario | Required evidence |
|---|---|
| Publication | Actual full pinned values/bytes/order,8 receipts, same backup/metadata across active catalogs and077 calendar; missing/stale publication fails. Grants are SELECT only. |
| Projection | Missing zones skipped; duplicate/cross-type keys refuse; real guild0/missing guild name/empty hero remain distinct; zero source rows require complete receipts. |
| Time/text |50-byte names including non-ASCII; NUL/overflow rejection; exact pre2000 sentinel, minute range, DST fold/gap and Python/PG agreement. |
| Environment | Logical1→physical other than1; missing logical route; map absent; instance template without party0; multiple map bindings in observed source order. |
| Spawn binding | Independent map/unit IN sets, negative/truncation boundaries, signed SQL bits, boss missing versus existing ID0, WORD spawn-list ordering. |
| HEROLIST | Exact client count/WORD/int32/string bytes, source order, empty names, invalid regions still present, full primary identity after delayed SQL and disconnect. |
| NPC consumers | Bound/unbound/non-occupation/invalid local, neutral/friendly/aid/hostile disguises, expired effects, source effect order and ambiguous per-template query refusal; existing transaction rollback/output order. |
| Retirement | Unsupported World mutation during worker read, no stale output after retirement/reconnect, drain and process-restart boundary; refusal remains classified missing gameplay. |

No build, test, SQL, import, migration, publisher, service, client, container,
push or deployment is run. Only source/API/SQL-text/diff review and metadata/
AST parsing are used. Historical verification does not certify this code.

Static structure review:258 unique coverage IDs with13 fields each and68
database mappings with6 fields each. All three new production translation
units occur once in the existing CMake target.078 is unique; next079. The
publisher AST parses without module imports/execution. Source server/client/
protocol trees and001–077 have no diff; whitespace review is clean. Both actual
backup SHA256 values above were re-read. No compiler or SQL validator ran.

Next implementation: replace prototype monster realization with original
`TMap.cpp::AddMonSpawn/InitMonster`, `TAICmdRegen::ExecAI` and their actual
AI/event/group/party/chart producers. Then integrate switches/GodBall and the
four enable consumers with native occupation/reward persistence. Preserve the
explicit refusal until each producer and its effects exist. Native guild
identity and THeroSelect authority are required before hero mutations.
