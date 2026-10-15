# Native World RPS bootstrap and win ledger — Phase A

Baseline: local `main`, `0533c73b4df3b93ff20c0824efc66ea4642638cb`.
Only untracked `_rewrite/docs/audits/` predated this increment; it is untouched
and excluded from versioning. No AGENTS.md was found in this repository or its
ancestors. Existing C++20/Boost.Asio/SOCI/CMake architecture is retained.

The World bootstrap, PostgreSQL batch and Control paths are connected in source
and **INTEGRATED_UNVERIFIED** in their stated World-only scope. The complete
RPS gameplay unit is **IMPLEMENTING**: the native Map start/round/reward producer
and reply consumer remain missing. Nothing here certifies playable RPS, full
World bootstrap, executable compatibility or Phase A completion.

## Authorities and local inventory

Original World/Map/client sources inspected here originate in
`c77f79017d7940c1500ba36a5dbf8bda6fefb67c`; no released client/data build is known.
World `SSHandler.cpp` SHA256 is
`21e89457f70203072bd659ce419031b65d04f26287e7e9c56fd9d2c3290345c2`, identical
in that commit and the current reference tree. Shared source `TVERSION=0x2918`
does not identify a game executable. Original `TNetDef.h` was also read with
`git show c77f790:Lib/Own/TNetLib/TNetLib/TNetDef.h`; its later technical port
must not be mistaken for an independent original CTime specification.

The actual backups were hashed read-only in this increment:

| File | SHA256 |
|---|---|
| TGAME_RAGEZONE.bak | `2cff1635d523322d3de8ddafaeacc3335581dc338dca901239ac80288f51d259` |
| TGLOBAL_RAGEZONE.bak | `fca545bcb90bcc72aa4f3514a6177c2b88d2b0bf71bf847b0128f090a20877fb` |

No backup, original attribution/header, client or archival snapshot is modified.
The preserved database `data-profiles.json` records48 chart rows and0 history
rows for the pinned backup profile; this is historical evidence, not a fresh
restore or proof that a current publication exists. The new importer enforces
that profile and exact verified full-manifest/checkpoint/target fingerprints.
Procedure text `schema/procs/TGAME_RAGEZONE/TRPSGameRecord.sql` SHA256 is
`a36e329c857145941f1b614f19b61fb3992f70ab3c74951fc4604b7c066351aa`.
The historical module comparison records IDENTICAL normalized backup/fragment
SHA256 `fdbd81b9787993f83f8b79b7deb2a9a149bf8fc171df7ed99faa5829d8e14ee7`;
the raw file hash and normalized SQL-module hash are different authorities.

The existing source index has206 legacy translation units still marked
NOT_ANALYZED, across all indexed server/tool roles. Lexical references do not
close them. The unified `legacy-modern-coverage.csv` now carries an independent
`verification` column; existing unverified implementation labels are preserved.
New WORLD.RPS identifiers distinguish catalog, gate, Control, persistence and
missing Map gameplay. No file/line-count completion percentage is calculated.

## Source contract and connected implementation

| Original files/symbols | Modern files/symbols | Transferred behavior |
|---|---|---|
| TWorldSvr/TWorldSvr.cpp844–877; DBAccess.h1401–1455 CTBLRPSGame/CTBLRPSGameRecord | main.cpp; PostgreSQLRpsRepository constructor/LoadGames/LoadWinDates; RpsRegistry::Insert/HydrateWinDate | One SQL snapshot reads exact full chart and mutable history; unknown history keys remain in SQL and are ignored in RAM as in source. No empty-on-SQL-error fallback. |
| SSHandler.cpp13173–13229 OnMW_RPSGAME_ACK | handlers_rps.cpp::OnRpsGameAck; RpsRegistry::PlanWin/PublishWin | Source key/type, global WORD count, period branch first, strict30-day expiry, future dates, zero cap and result bytes. Reply goes to the requesting current Map, as in source. |
| SSHandler.cpp13232–13256; CSPRPSGameRecord; actual TRPSGameRecord body | PostgreSQLRpsRepository::CommitWin/WriteRecord/Record; written074 | Duplicates retained; source deletion is same-key `time<=cutoff`, not deletion of one character/row. Complete expiry/insertion/receipt batch commits before RAM/result. |
| Original TNetDef.h::__TIMETODB/__DBTOTIME; original SQL_C_SSHORT/SQL_C_SLONG bindings | app_world.rps_source_minute; import_world_rps.py; Word/Signed helpers | Explicit source local calendar, integral second30 rounding, zero→1900, every pre2000 year→0, signed SQL SMALLINT/INTEGER bit patterns. Live RAM keeps actual seconds; restart hydrates stored minutes. |
| SSHandler.cpp13259–13325; SSSender.cpp76–98; TWorldType.h126 | OnRpsGameDataReq/OnRpsGameChangeReq; RpsRegistry::Snapshot | Exact whole-body decoding before mutation; MAKEWORD unsigned key ordering; known-key updates only, duplicate edits ordered, zero-count silence; Control changes remain process-local and reset on restart. Existing body forwarding and opcode/field widths remain. |

Original World/Map source on-disk paths are under `Server/TWorldSvr/` and
`Server/TMapSvr/`; all new production paths are under `Server/TWorldSvrAsio/`.
New `.cpp` is in its existing CMake core. Native `main` chooses the PostgreSQL
repository and sets `HandlerContext::native_rps`; the ODBC compatibility adapter
remains explicitly selected only for nonnative configuration. The existing
dispatch still calls real MW_RPSGAME_ACK and CT_RPSGAME* handlers.

World's current process owner, source INTEGER account/key identity, ready and
unlocked primary claim, live Map advisory owner and exact registered-socket Map
token are checked in the same SQL transaction as the batch. No character/role
is inferred from an opcode alone. Native network DM_RPSGAMERECORD_REQ is rejected:
the original message is an internal DB-queue operation, now executed internally
by CommitWin. Native handlers recheck current open actor/key/primary/handoff
before planning and after awaited persistence. On disconnect after COMMIT the
global cap still publishes, while no reply is rerouted to a replacement peer.

A reactor-scoped Asio operation permit spans plan/SQL/publication/result and
Control update/fanout; thread mutexes are not held over awaits. Existing World
uses one reactor and sequential per-peer dispatch. The worker repository mutex
serializes its SQL batches. The guarded publication compares the entire
observed entry, including transient Control fields and live timestamps.
Unexpected disagreement stops World for reconciliation. SQL/COMMIT failure also
stops through the existing native social stop callback; there is no automatic
retry, compensation, RAM-only success or response replay. A fresh process must
hydrate the authoritative committed history before accepting further work.

Written074 adds the pinned publication, typed read view, operational records,
source-calendar resolver and batch receipts. `import_world_rps.py` writes only
the operational projection after the existing all-owner offline barrier and
full source-value verification. It refuses any existing publication/records/
operations, including after unknown COMMIT. Historical source INT character
values remain audit values without a false FK to new operational identities.
The separate World grants give no archival writes or publication privileges.
Migrations001–073 are unchanged; migration074/import/grants are **written only**.
Next free migration identifier is075.

## Explicit repairs and remaining limits

- Source copies the timestamp vector, then erases the live vector while
  continuing to index the stale copy. The modern ordered iteration erases the
  working vector; it never reproduces stale-vector access or an unbounded loop.
  The source-defined ordinary comparisons/expiry order are preserved.
- Source WORD indexing beyond65535 rows and Win32 signed INT overflow in
  `wWinPeriod*86400` are unavailable, not widened into new game rules. Zero cap
  still skips this block. Oversized charts cannot fit source Plain16 output;
  publication/startup enforce the actual7279-row packet limit before output.
- Invalid/trailing/truncated Control bodies cause no partial edit. Native
  gameplay requires current SQL/socket authority and commit-before-result.
  These are deliberate safety changes to the original trusted-peer/queued-write
  behavior. No wire field or client modification is introduced.
- Source timezone must be chosen explicitly in Phase B even for the actual
  empty pinned history. IANA/Python/PostgreSQL agreement is checked for imported
  later dates. Native source-minute resolution counts all whole-minute UTC
  offset candidates within±24h: ambiguous/gap/sub-minute-offset minutes refuse
  without a fabricated CRT choice. Such profiles and resolver performance remain
  acceptance work; large expiry batches can reach the existing SQL timeout.
- The protocol has no round nonce. A repeated admitted request is another
  request as in source; receipts do not manufacture universal replay detection.
  Committed cap reservations can survive loss of the result and are not refunded.
- Full PostgreSQL-only World startup still has required unported guild/war/
  event/rank/service repositories and validation. No required bootstrap check
  is removed. Original MW_CONNECT_ACK admission/channel/topology/full ordered
  bootstrap remains OPEN; modern RW-as-Map is still a documented extension.
- Native Map does not yet create RPS session state or persist its start-item/
  round/reward transitions, consume MW_RPSGAME_REQ, or apply RPS Control changes.
  The complete source random/card selection, terminal giveup/reward/mail/reset,
  transfer/disconnect lifecycle and original-client UI remain to implement.

## Deferred Phase B scenarios — not executed

| Scenario | Required observable evidence |
|---|---|
| Fresh/upgrade074 and least-privilege grants | Exact schema/grants; unchanged001–073 and historical hashes. No source/published data created by migration. |
| Missing/stale/incomplete publication or altered source target | Startup fails, never a successful empty chart; original48/0 manifest/profile and archive values agree. |
| Real bootstrap then restart | All48 actual config rows in source MAKEWORD order; exact duplicate/future/unknown-key/pre2000 history fixtures; genuine empty history remains distinct from SQL failure. |
| Zero cap; missing key; below/equal/above cap | Source result1/0; no history for zero cap; WORD counting and distinct game keys; no character-local cap substitution. |
| Period equality and age exactly30days/older; period>30days | Strict `<period` branch precedes strict `>30days`; actual ordered batch and deletion of all same-key earlier/equal stored minutes, including duplicates/other characters. |
| Source time0; pre2000; seconds29/30/59; midnight/year boundary | Exact1900 sentinel, pre2000 hydrate0, source-rounded local timestamp and second-level RAM vs restarted-minute difference. |
| DST fold/gap; timezone/PG/Python disagreement | Explicit refusal/rollback; no assumed CRT default, successful result, archive rewrite or invented zone. |
| Two Maps/Control update while first batch awaits | Single source-ordered gate; no stale cap plan or old Control fanout; original Control edits remain transient across restart. |
| Fault on a later expiry/INSERT/receipt, failure before/after COMMIT, lost result | All-or-nothing SQL; no precommit cache/result; stop/reconcile on uncertain commit; actual receipts and restart history determine the outcome. |
| Peer/character replacement, claim transfer, key change, disconnect at each await | Exact socket/token/ready/handoff guards; no replacement reply, refund or automatic replay; committed global history still publishes when caller disconnects. |
| Anonymous/nonprimary MW, remote internal DM and unauthorized Control | No SQL mutation/success; existing trust, role and source Plain16 packet boundaries retained. |
| Malformed Control tail; unknown/duplicate edit keys; empty edit list | No partial edit; source last-known-key update order; exact full snapshot/raw Map-forward body; zero-count silence. |
| Source signed-product/vector/packet limits | Explicit unavailable cases; no undefined arithmetic, stale vector access, count wrap or oversized output. |

Static inspection performed: new importer parsed with Python `ast.parse` without
importing or calling it; unified inventory228 unique IDs/13 fields and database
map63 rows/6 fields parsed consistently; migration filenames have a unique074
and next075; `git diff --check` reports no whitespace errors. Existing source
and old migration paths are absent from the diff. These checks do not compile
C++, parse/execute PostgreSQL or exercise gameplay.
No build, test, SQL, import, migration, service, game client,
container, runtime acceptance, push or deployment was performed.

Exact next implementation point: native Map RPS production/consumption from
`CSHandler.cpp::OnCS_RPSSTART_REQ/OnCS_RPSGAME_REQ`,
`SSHandler.cpp::OnMW_RPSGAME_REQ/OnMW_RPSGAMECHANGE_REQ`,
`TMapSvr.cpp::RPSReward/SelectNpcRPS` and original item/mail/reset/transfer
dependencies. Preserve probability threshold fields and the source request for
the next win-count key. This closes the missing half of this unit without
inventing data; original admission/full World bootstrap remains the parallel
prerequisite backlog, not a task to replace with a registration ACK.
