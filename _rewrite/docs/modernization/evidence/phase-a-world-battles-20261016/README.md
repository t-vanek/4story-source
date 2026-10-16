# Native World battle clock, timer and batch — Phase A

Baseline: local `main`, `8c0b11e`. Only `_rewrite/docs/audits/` was untracked;
it remains untouched/outside Git. No applicable AGENTS.md was found. Source
trees, attributions, backups and001–076 are unchanged. Written077, grants and
publisher are not executed; recorded044–077 remain unapplied, next078.
Full Phase A and end-to-end battle gameplay remain **IMPLEMENTING**.

## Actual source and data authority

Original source lineage was checked in Git:
`c77f79017d7940c1500ba36a5dbf8bda6fefb67c`. Client TVERSION0x2918 remains a
source marker, not an identified executable/data/region profile. Original
Map/World `stdafx.h` defines SKYGARDEN. No client/source edits are made.
Read-only hashing of the actual backups again produced:

| Backup | SHA256 |
|---|---|
| TGAME_RAGEZONE.bak | `2cff1635d523322d3de8ddafaeacc3335581dc338dca901239ac80288f51d259` |
| TGLOBAL_RAGEZONE.bak | `fca545bcb90bcc72aa4f3514a6177c2b88d2b0bf71bf847b0128f090a20877fb` |

Historical profiles record5 TBATTLETIMECHART and3 TCUSTOMTIMECHART rows, and
zero historical guild/member/PvP-history rows. Those totals are not runtime
values, native guild hydration, an observed custom-time order or game acceptance.
No restore, SQL query or synthetic historical row is used in this increment.

- `Server/TWorldSvr/DBAccess.h639/1530`: exact8-column battle and2-column
  custom-time queries. Battle INTs bind to DWORD; custom bType is SQL SMALLINT
  bound to source BYTE. Out-of-BYTE values are unavailable, not silently narrowed.
- `TWorldSvr.cpp1267–1355`: custom times are collected in query order; only
  MISSION3 feeds m_vCUSTOMTIMES. Loaded battle rows set status NORMAL; LOCAL
  peace duration is explicitly overridden to180. Source slots are a fixed
  global array, not optional objects. Static-storage zero initialization and
  tagBATTLETIME's start/day/week/running constructor are preserved; actual
  loaded rows are tracked separately. No failed query becomes an empty chart.
- Mission boot creates an ordered DWORD(time-current_second) map. It takes its
  first value, then the source's unusual find(2) branch when that value is past.
  Empty map/missing delta2 dereferences are undefined source behavior. Native
  startup refuses them; no guessed tomorrow/nearest-time repair is substituted.
- `TimerThread2203–2224` samples m_timeCurrent before WORLD_TIMER1000 wait.
  `OnTimer4147–4289` selects castle weekday, else unsigned SkyGarden window,
  else local; a running mission or its unsigned pre/post window may override
  a nonrunning selection. Arithmetic, strict/inclusive comparisons, daily
  modulo, alarms600/120/10 and60/10, NORMAL/BATTLE/PEACE transitions and the
  mission vector's actual next/duplicate/wrap branches are retained. No
  catch-up, sorting, boundary normalization or deduplication is invented.
- `TWorldType.h273::tagBATTLETIME`, BATTLE_TYPE and NetCode.h2032::BATTLE_STATUS
  define the actual types: NORMAL0/BATTLE1/PEACE4; tournament2/mission3/sky4.
- `SSHandler.cpp9870::OnSM_BATTLESTATUS_REQ`: type-specific enable packets,
  then non-mission PEACE updates recent record date, calls every guild's
  CalcWeekRecord and clears castle scores for CASTLE. Unknown types still have
  the source peace epilogue. Control also sends this SM packet in
  `Server/TControlSvr/TServer.cpp181`; it is not an unrestricted Map command.
- `TGuild.cpp615` uses DWORD date+7 wrap and WORD/DWORD aggregates.
  `TWorldSvr.cpp1077–1115` initializes recent date from actual loaded records;
  `SSHandler.cpp10379`/`SSSender.cpp3184` use it to select the actual last
  vRecord entry. An absent/older entry alone produces zeros in that slot.

## Concrete modern implementation

`PostgreSQLBattleRepository`, constructed in native World main under existing
World ownership before listening, requires the full077 publication. It reads
all actual battle fields and ordered custom rows, checks counts/NULL/types and
ordinals, and initializes `BattleScheduler`. Every tick and queued delivery
checks the same current release/order/timezone under native World ownership.
Calendar conversion uses the explicitly published IANA zone, shared with the
same-world074 RPS profile; no host/process timezone is inferred. Epoch→wall
hour/minute/second and Sunday1..Saturday7 are explicit PostgreSQL reads.

`BattleScheduler::Load/Tick` transfers the actual source calculations above.
Each tick works on a copy and publishes only defined completed transitions;
an out-of-range duplicate-tail read or source BYTE iteration overflow refuses.
State remains process-local as in the source, reinitialized on restart. No
invented saved battle table, probability, reward or status receipt is added.
The actual castle weekday now supplies the existing election consumer instead
of the native config stand-in; invalid weekday indexing refuses at election.

`main.cpp` wires the existing one-second RegistryRefresher, explicit sampled
clock and a specialized256-entry Asio FIFO for the source timer→batch boundary.
The clock advances independently of slow socket delivery. Tick enqueues the
original four fields; `RunBattleDelivery` validates ownership and drains FIFO
through the original enable senders. Queue exhaustion stops World rather than
dropping events. Output failures are not replayed or compensated. This uses
the existing World process/CMake/Asio infrastructure, not another server layer.

`handlers_battle.cpp::OnBattleStatusReq` validates the complete ten-byte body.
Native external SM requires the current open registered Control peer, rechecked
after SQL; a Map/anonymous peer cannot impersonate the timer. Control broadcasts
do not alter the source timer fields. They use the same native FIFO, original
LOCAL14/CASTLE5/MISSION9/SKYGARDEN10-byte bodies and peace epilogue. New SkyGarden
sender follows SSSender.cpp2347, including periodic day0 and actual start.

The batch gate also covers castle reports, cached LOCALRECORD fan-in and the
guild PvP reader, preventing a suspended peace fanout from later deleting a
newly applied score or interleaving record reads. No thread mutex spans an await.
Shutdown cancels the new timer and queue receive; the existing worker stop/join
runs before repository/pool destruction. Pending RAM events are not replayed.
After fanout the actual latest sampled timer clock supplies the source recent
DWORD day. Real cache histories are trimmed and real castle entries cleared.

`GuildRegistry::InitializeRecentRecordDate` scans actual loaded cache history,
then trims at its maximum. The existing PvP aggregate helper now preserves
DWORD date+7 wrap and rejects dates that cannot represent a source DWORD.
`OnGuildPvPRecordAck` emits actual weekly values and conditional last values
in original member-ID order. `SendMwGuildPvPRecordReq` no longer writes a
generic all-zero last slot. This completes the bounded cache path; the existing
SOCI guild loader still omits historical PvP rows, and full native guild/history
hydration/persistence and tactics are separate explicit implementation gaps.

077 adds publication metadata and read-only views over immutable archives.
`import_world_battles.py` requires the full verified manifest and exact pinned
profile, verifies archive values and custom-row ordinals under offline owner
barriers, and refuses replacing a published profile. Multiple mission entries
require a private explicit original-query order profile. Its JSON identifies
`source_manifest_sha256`, exact query `SELECT bType, dwTime FROM TCUSTOMTIMECHART`
and `source_rows` containing every real archival ordinal once. Its actual file
SHA256 is recorded. No automatic ORDER BY time is claimed equivalent to the
original unordered SQL. An explicit profile is still unverified compatibility,
requiring observed source evidence in Phase B. With at most one mission row,
its mission order is independent; no BOW/BR order parity is inferred.
The publisher, metadata and grants are written only, never invoked here.

## Remaining boundaries and necessary refusals

- Map `OnMW_LOCALENABLE/CASTLEENABLE/MISSIONENABLE/SKYGARDENENABLE` source
  handlers mutate actual TLOCAL objects, next-defence times, occupation results,
  monsters, switches, GodBall/war state, rewards and SYSTEMMSG. Those producers
  and PostgreSQL contracts are not implemented by these World calculations.
  The modern Map dispatcher now explicitly retires that World link instead of
  silently dropping enable packets or accepting only a numeric flag. This is a
  protective refusal, **not completed Map support**. A scheduled battle can
  therefore retire the current partial Map/World connection and drain clients.
- Native World still constructs other unported legacy SOCI repositories.
  Full PostgreSQL-only World startup, guild-history loading, BOW/BR scheduling,
  tournament/quarter/day/month timer integration, original MW_CONNECT_ACK and
  initial LOCAL/MISSION/SKY replay are unfinished. This is the normal battle
  timer tail, not a claim that the full source OnTimer/bootstrap is complete.
- Source all-past/empty mission initialization and duplicate-tail undefined
  reads remain unavailable. Multiple mission query order needs an explicit
  source profile; no unavailable data is replaced with a sorted/default list.
- The component FIFO/gate does not serialize every other unported World batch
  handler. Whole-server batch ordering, slow-peer deadlines, actual timezone/
  CRT behavior and client/map acceptance remain unverified. RegistryRefresher
  uses a one-second cadence; original timer-job early wakeups are not modeled.
- No original client packet layout, attribution, archive row or applied/earlier
  migration is changed. Security/resource refusals above are explicit repairs.

## Deferred Phase B evidence

| Scenario | Required evidence |
|---|---|
|077 publication/grants | Exact5/3 private source rows and ordinals, full manifest/backups, same-world RPS calendar, least privilege, missing release fails. No SQL Server call in new paths. |
| Order and undefined boot | At-most-one mission independent order; explicit multi-row source profile; duplicates, empty times, all-past missing find2; no invented next-day start. |
| Selector and arithmetic | Castle weekday priority, unsigned SkyGarden start/window, local fallback, running/near mission override, DWORD overflow/underflow and daily modulo. |
| Transition boundaries | Exact battle start, zero start, alarm thresholds600/60 and intervals600/120/10/60/10, battle end, peace expiry, no-match/duplicate/last mission and +DAY wrap. |
| Clock and queue | Sample before wait; source IANA zone, midnight/DST/backward clock; slow Map does not stop tick calculation; FIFO order/repeated alarms; full256 stops without silent drop/replay. |
| Control and delivery | Actual Control SM only, takeover/disconnect while awaiting SQL, current owner after queue wait, exact four enable bodies and unused zero fields. No packet claimed received merely because queued. |
| Peace and record UI | Mission PEACE leaves records alone; other/unknown PEACE recent-day trim; castle-only scoreboard clear; real conditional last slot, WORD/DWORD wraps and date+7 boundary; concurrent report/read/fan-in order. |
| Initialization and restart | Slots missing versus loaded zeros, actual LOCAL peace180, actual castle day0 and invalid indexing refusal, original process-local restart state; complete historical guild loading remains a separate prerequisite. |
| Map/full gameplay | Requires real local ownership, spawn/AI, switches, occupation DB, rewards and unchanged client SYSTEMMSG. Current explicit refusal must remain classified missing, not accepted gameplay. |

Only static source/interface/SQL-text/diff review, metadata parsing and backup
hashing are performed. No build, test, SQL, import, migration, server, client,
container, publisher execution, push or deployment ran. No global
IMPLEMENTATION_COMPLETE_UNVERIFIED status is justified.

Static structure checks:251 unique capability IDs with13 fields per row;
66 database mappings with6 fields. All three new production translation units
appear once in existing CMake. Migration077 is unique; next078. The publisher
AST parses without importing/invoking project code. Original Map/World/client/
protocol trees and001–076 have no diff; whitespace checks are clean. No compiler,
SQL validator or test runner was invoked.

Next: port actual Map TLOCAL production from `TMapSvr.cpp3150–3440`, its
metadata/ownership/spawn/switch/persistence dependencies, then all four enable
consumers. Do not replace the missing regions with empty vectors or remove the
refusal until those state producers and downstream effects exist.
