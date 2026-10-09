# 05 — Source dependencies and procedure responsibilities

The actual architecture is **C++20 + Boost.Asio + SOCI**, with a shared custom ORM
(`EntityMapping`, `Repository`, `DbContext`, `SpCall`). No .NET persistence layer
is introduced. `vcpkg.json` enables SOCI ODBC; shared
`Lib/Own/FourStoryCommon/src/db/session_pool.cpp:18` throws for PostgreSQL.
Dialect branches and PostgreSQL dev DDL therefore do not prove an enabled PG server.

[source-object-mapping.json](evidence/source-object-mapping.json) records component
path/line → object → SQL operation/ORM usage/validator → available artifact and
restored metadata candidates. It includes legacy sources, tests, current portable
services and the copied `TMapSvrAsio/legacy_src` reference separately; that directory
is not compiled (CMakeLists.txt:41). Adjacent/raw C++ SQL strings are decoded;
EntityMapping types are joined to `.Set<Type>()` callers, not merely literal names.

Current portable, non-test source has **430 lexical edges**, **160 distinct
normalized names**, of which 158 are application object names and 143 have a
restored name match. These are explainable *name availability* counts, not runtime
behavior/column coverage. Name matches across global/game DBs are ambiguous until
the pool/owner is resolved. Shared peer authentication adds its own missing objects.
The source currently calls **37 distinct legacy procedure names**; older
SQL_AUDIT.md's no-SP conclusion does not apply to current code.

| Domain / owning component | Observed objects and operations | Concrete source |
|---|---|---|
| Login/auth | TACCOUNT_PW, TCURRENTUSER, TUSERINFOTABLE, bans, secure code; credential read/session insert/update | TLoginSvrAsio/services/soci_auth_service.cpp |
| Character lifecycle | TCHARTABLE, TALLCHARTABLE, TITEMTABLE, starter/reference tables; multi-step create/delete | TLoginSvrAsio/services/soci_char_service.cpp |
| Character/session loading | TCURRENTUSER, TCHARTABLE; read/targeted save, ORM mapping | TMapSvrAsio/db/queries.h; services/soci_player_service.cpp |
| Inventory/items | TINVENTABLE, TITEMTABLE, owner/storage fields; bag rows and instance persistence | TMapSvr/DBAccess.h; TMapSvrAsio/services/soci_inventory_service.cpp |
| Skills/quests | TSKILLTABLE, TQUESTTABLE, TQUESTTERMTABLE plus charts; learned/progress rows and boot catalogs | TMapSvrAsio/db/queries.h; services/soci_quest_service.cpp |
| Monsters/NPCs/maps | TNPCCHART, TMONSTERCHART, TMONSPAWNCHART, TMAPMONCHART, TMONATTRCHART, TMONITEMCHART | TMapSvrAsio/db/queries.h; services/spawn_manager.cpp |
| Guild/social | TGUILDTABLE, TGUILDMEMBERTABLE, article/cabinet/tactics tables, TFRIENDTABLE; ORM loads and parameterized writes | TWorldSvrAsio/services/guild_entities.h; soci_guild_repository.cpp; friend_entities.h |
| Mail/auction | TPOSTTABLE/TPOSTITEMTABLE/TAUCTIONTABLE families, item ownership/currency writes and procedures | TMapSvr/DBAccess.h; schema/procs/TGAME_RAGEZONE/TSavePost.sql, TAuctionBid.sql |
| Events/world state | Bow/tournament/rank/occupation charts and procedure calls | TWorldSvrAsio/services/soci_bow_repository.cpp; soci_tournament_repository.cpp; soci_month_rank_repository.cpp |
| Patch/control/audit | TVERSION/TPREVERSION, peer registry/metrics/auth, operator audit and TLOG_AUDIT | TPatchSvrAsio/services/patch_repository.cpp; TControlSvrAsio/services/soci_*.cpp; TLogSvrAsio/services/log_sink.cpp |

Static catalogs are boot-loaded and cached in chart services. Monster instances,
AI/combat ticks, player registries and peer heartbeat timers are runtime state
(`domain/monster.h`, `services/spawn_manager.cpp`, `services/char_state_store.h`,
shared `cluster/peer_client.cpp`). Persist progression, economy, ownership and
durable world outcomes; do not turn per-tick position/HP updates into SQL writes.
TCURRENTUSER is operational session state and must be rebuilt/invalidate sessions
at a database cutover, not imported as active historical sessions.

Missing restored definitions, with exact evidence locations:

| Object | First observed source site | Artifact coverage |
|---|---|---|
| `TAddBOWPlayer` | `Server/TWorldSvrAsio/services/soci_bow_repository.cpp:101` | No supplied definition |
| `TBetaToVersion` | `Server/TControlSvrAsio/services/soci_patch_metadata_service.cpp:68` | No supplied definition |
| `TClearBOWPlayers` | `Server/TWorldSvrAsio/services/soci_bow_repository.cpp:118` | No supplied definition |
| `TDeletePreVersion` | `Server/TControlSvrAsio/services/soci_patch_metadata_service.cpp:88` | No supplied definition |
| `TDeleteSingleBOWPlayer` | `Server/TWorldSvrAsio/services/soci_bow_repository.cpp:133` | No supplied definition |
| `TLOG_AUDIT` | `Server/TLogSvrAsio/services/audit_query_repository.h:111` | Partial dev DDL/name match |
| `TOP_AUDIT_LOG` | `Server/TControlSvrAsio/db/schema_validator.cpp:146` | No supplied definition |
| `TPCBANG` | `Server/TLoginSvrAsio/services/soci_auth_service.cpp:536` | Partial dev DDL/name match |
| `TPEER_AUTH` | `Lib/Own/FourStoryCommon/fourstory/security/peer_auth_repository.h:138` | No supplied definition |
| `TPEER_AUTH_LOG` | `Lib/Own/FourStoryCommon/fourstory/security/peer_auth_repository.h:171` | No supplied definition |
| `TPEER_METRICS` | `Server/TControlSvrAsio/db/schema_validator.cpp:140` | No supplied definition |
| `TPEER_REGISTRY` | `Server/TControlSvrAsio/db/schema_validator.cpp:124` | No supplied definition |
| `TPEER_STATUS_LOG` | `Server/TControlSvrAsio/db/schema_validator.cpp:134` | No supplied definition |
| `TUSEREMAIL` | `Server/TLoginSvrAsio/db/schema_validator.cpp:92` | Partial dev DDL/name match |
| `TUSERPREMIUM` | `Server/TLoginSvrAsio/services/soci_auth_service.cpp:553` | Partial dev DDL/name match |
| `TUSERTRUSTEDIP` | `Server/TLoginSvrAsio/db/schema_validator.cpp:95` | Partial dev DDL/name match |
| `TUpdatePreVersion` | `Server/TControlSvrAsio/services/soci_patch_metadata_service.cpp:47` | No supplied definition |

Several missing objects are intentional modern extensions (2FA, audit, peers),
others are missing callable procedures (patch metadata/Bow). Archived legacy-only
requirements are additionally enumerated in the JSON; they do not establish
current portable runtime requirements. Optional catch-and-continue paths do not
make those features functionally covered.

**Contract conflicts requiring review:**

- Guild validator asks for `TGUILDARTICLETABLE.dwIndex/szBody/timeWrite`, while the
  backup and actual AddArticle SQL use `dwID/szArticle/dwTime` (soci_guild_repository.cpp:310).
  Its cabinet `wItemKind` and tactics `bRole` requests are also absent. These are
  warn-only validator drift, not evidence to invent replacement legacy columns.
- Modern inventory reads smalldatetime `dEndTime` into int64 and inserts NULL for
  a permanent row (`soci_inventory_service.cpp:34,64`); original TINVENTABLE declares
  it NOT NULL. Time conversion and bag/instance semantics need explicit contracts.
- Legacy monster stat lookup uses `(wMonAttr,bLevel)` (`TMapSvr/TMap.cpp:629`), while
  portable spawn_manager.cpp:65 uses `(monster ID,bLevel)`. On supplied data these
  produce 39 versus 3,468 missing template combinations; this is a demonstrated
  behavior/data incompatibility, not a PG type conversion problem.
- Character item ID allocation still emits `SELECT TOP 1` before falling back to
  synthetic IDs (`soci_char_service.cpp:807`). PG must use a reviewed allocator
  rather than entering that fallback on dialect failure.

Every original procedure is covered by
[backup-procedure-modernization.json](evidence/backup-procedure-modernization.json):
323 objects with parameters, callers, dependencies, lexical writes/transaction
flags, proposed responsibility and PG migration status. A separate fragment matrix
retains each export file. Literal values and full private definitions are omitted.

| Procedure/family | Observed behavior / callers | Proposed replacement; transaction contract |
|---|---|---|
| TLogin, TCheckPasswd, TLogout | Auth/session, returns/OUT fields, optional external billing; modern auth/session services inline parts | Application auth + one persistence transaction for account/session claim; preserve protocol error codes and duplicate-login invariants |
| TOPLogin, TUserProtectedAdd | Operator auth and ban update; Control SpCall callers | Application policy + parameterized persistence; audited operation, PG permissions |
| TCreateChar/TDeleteChar | Global/game wrappers and multi-table mutations; current SociCharService | Domain orchestrator in one PG DB across auth/game schemas; slot/name uniqueness and rollback, explicit ID RETURNING |
| TGenerateDBItemID | World-partitioned 2^56 ID ranges; counter UPDATE/SELECT under transaction | Persistence allocator/sequence with world-range mapping and high-water verification; no synthetic collision fallback |
| TSaveItemDataStart/TSaveItem/TSaveItemDataEnd | Temp inventory/item writes followed by transactional replacement | Application save orchestration + persistence transaction; preserve snapshot atomicity and ownership, eliminate shared temp data when proven safe |
| TSavePost/TAuctionBid/TAuctionBuyDirect | Money checks, bidder/item/mail mutations; several originals lack explicit transaction | Domain trade/mail service with item/auction/account locks and atomic currency/ownership transfer; behavior tests before improving transaction boundaries |
| TGuildEstablish and guild writes | Duplicate checks, guild/member state, transaction/return codes | Guild aggregate application layer + constraints and transaction; lock stable IDs in consistent order |
| Tournament/Bow/rank/event procedures | Current World EXEC callers; scheduling, persistence, awards | Application scheduler/domain plus persistence transactions; idempotent reward keys; do not blindly translate to PG functions |
| OPTool*, tests, batches, mass clear | Administrative/destructive maintenance and helper operations | Explicit admin/background worker; separate privilege and controlled policy, never automatic migration startup |
| TALLCHARTABLE_PW triggers | Change feed into TALLCHARTABLE_TRIGGER; scalar SELECT over INSERTED/DELETED | Reviewed set-based outbox/transactional change feed; scalar historical code is unsafe for multirow batches |

No legacy business procedure was converted to a PG function or claimed verified.
Constraints own relational invariants; application services own behavior; the
persistence layer owns SQL/transaction boundaries; scheduling belongs in existing
workers. Small PG functions remain an option only for a proven atomic operation
that cannot be expressed clearly with RETURNING/constraints/parameterized SQL.

The index is **lexical, not a compiler or runtime trace**. Preprocessor branches,
runtime-generated identifiers, aliases and concatenated dynamic SQL require review.
[dynamic-sql-review-sites.json](evidence/dynamic-sql-review-sites.json) lists those
generator sites. `SpCall::BuildSql` is T-SQL-specific and its current debug/error
logs include rendered argument literals; redact that before credential/admin cutover.
