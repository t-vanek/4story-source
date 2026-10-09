# 09 — Exact gaps, conflicts and required evidence

| Gap / observed conflict | Evidence | Required resolution |
|---|---|---|
| No 4story.bak and no authoritative game/client/content pairing | Repository-wide inventory; TVERSION=0x2918 source; global TVERSION empty | Supply original distribution/build/patch manifest and client assets to identify exact supported release |
| No recovered player world/auth corpus | TACCOUNT=1, TACCOUNT_PW=0, TCHARTABLE/TITEMTABLE/TGUILDTABLE=0; exact counts | A populated, authorized account/game backup pair and migration-safe fixture corpus; do not invent progress |
| Missing external DBs/services | sys.sql_expression_dependencies points to TGLOBAL_GSP/TGAME_GSP/TQUEST_GSP/TGLOBAL/TGAME/fourstory_ob and linked servers | Original schemas/data or a reviewed replacement contract; remapping only names does not recreate billing/content behavior |
| Global/game catalog conflict | Item catalogs have 44/52 columns, 2,519/7,706 rows, mismatches across 37 shared numeric fields; monster catalogs also differ | Select domain owner/source release explicitly; investigate stale-cache vs revision provenance, never merge solely by same table name |
| Current server cannot select PG backend | SessionPool throws; vcpkg SOCI only ODBC | Opt-in Linux SOCI PostgreSQL wiring, backend integration test, then repository adapters |
| MSSQL-specific calls/dialects remain | SpCall DECLARE/EXEC/OUT; World 37 unique SP call names; TOP/identity/date expressions | Preserve protocol return/output contracts in application/persistence transactions; no blind PG function conversion |
| Monster attribute key differs | Legacy TMap.cpp:629 wMonAttr versus spawn_manager.cpp:65 monster ID; 39 vs 3,468 misses | PostgreSQL migration 006 now derives the current monster-ID contract through backup wMonAttr/bLevel: 3,495 matches, 39 retained gaps. C++ backend/gameplay integration and gap classification remain pending |
| Required patch/Bow procs absent | TUpdatePreVersion/TBetaToVersion/TDeletePreVersion and TAddBOWPlayer/TClearBOWPlayers/TDeleteSingleBOWPlayer source callers | Obtain authoritative definitions or reconstruct behavior with call/parameter tests before enabling features |
| Modern extension schemas incomplete | Peer/auth/metrics/operator audit absent in backup; 2FA/audit dev scripts partial | Versioned production migrations for exact current requirements; independent permissions and data ownership |
| Validator/schema drift | Guild dwIndex/szBody/timeWrite vs actual dwID/szArticle/dwTime; wItemKind/bRole absent | Adapt derived PostgreSQL contracts after proving semantics; preserve backup columns and avoid inventing wItemKind/bRole values solely to satisfy validators |
| Inventory/time mismatch | smalldatetime NOT NULL dEndTime versus int64 read/NULL insert | Establish bag vs item semantics, expiration sentinel and time conversion; behavioral fixtures |
| Real reference anomalies | 62 reward→quest, 2 spawn→monster, 6 spawn→spawn mismatches; extreme NPC fPosZ | Determine deliberate retired/sentinel content vs errors; quarantine publication, retain original evidence |
| Unsigned/sentinel interpretation | TSKILLCHART negative dwDuration and wMapID=-1; legacy DWORD/WORD source | Per-field bit/sentinel mapping with protocol tests, not blanket widening or rejecting negatives |
| Collation/code page uncertainty | Latin1_General_CI_AS_KS, CP1252; Korean-origin/Unicode material; raw bytes preserved | Compare actual normalized login/name corpus, source-byte provenance and client encoding; choose PG policy based on behavior |
| Historical date timezone unknown | SQL datetime/smalldatetime naive, modern std::mktime/int64 paths | Original server timezone/runtime convention and expiry semantics; no automatic Europe/Prague/UTC reassignment |
| Missing game FK semantics | Zero declared game FKs, one untrusted global FK; sparse player data | Infer/test each relationship and polymorphic owner case, validate populated data before constraints |
| Item ID allocator semantics | TGenerateDBItemID world ranges 2^56; TITEMTABLE not identity; modern TOP/fallback | Preserve world/ID high water and atomic allocation; prove no duplicate/overlap on transfer/import |
| Authentication hashes not interchangeable | BCrypt over client SHA1 wire secret; generic offline wrapper; no populated TACCOUNT_PW corpus | Scheme classification, approved wrapper only for matching wire secret, reset/rehash policy and synthetic auth tests |
| Existing SQL logger exposes rendered values | fourstory/db/orm/sp_call.cpp debug/error query logging | Redact parameters and backend exception details before sensitive application migration |

These are concrete findings; they do not justify assuming every static unmatched
key is an error. The 7,170-row naive monster-attribute comparison is explicitly
not a proven FK. Existing `SCHEMA.md` claims game FK absence is intentional for
performance and labels a game version; neither inference is proven by metadata.

The supplied sources/definitions are sufficient for additional behavior work,
but schema metadata alone cannot establish exact wire versions, unknown external
service behavior, lost player rows or historical encoding/timezone decisions.
None of the blockers stopped backup recovery, metadata reconstruction, static
data migration or isolated verification. The canonical player/domain schemas and
full stored-procedure behavior suite remain proposed work, not hidden placeholders
or fabricated working data.
