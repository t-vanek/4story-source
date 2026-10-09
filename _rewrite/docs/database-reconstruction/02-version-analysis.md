# 02 — Version and compatibility analysis

| Concept | Established value | Confidence and evidence |
|---|---|---|
| Backup writer engine | SQL Server **11.0.2100** (2012 generation) | HIGH: both RESTORE HEADERONLY outputs |
| Original database format/compatibility | DatabaseVersion **706**, compatibility **90** | HIGH: backup headers; independent of game version |
| Inspection engine | SQL Server **16.0.4265.3**, Developer edition | HIGH: SERVERPROPERTY; SQL Server 2022 instance |
| Restored compatibility | **100**, collation `Latin1_General_CI_AS_KS` | HIGH: restored sys.databases |
| PostgreSQL verification | PostgreSQL 18 image; actual version in verification evidence | HIGH: database query and image digest |
| Schema lineage | RAGEZONE snapshots backed up **2019-01-27** | HIGH: headers plus normalized metadata/module comparison |
| Schema revision/build number | **UNKNOWN** | No authoritative migration ledger or release ID in artifacts |
| Game server/client release | **UNKNOWN** | Repository labels mention 5.0, insufficient to date the database or client build |
| Source protocol constant | `TVERSION = (WORD)0x2918` | HIGH: `Lib/Own/TProtocol/include/ProtocolBase.h:4`; not a game release number |
| Live content/patch version | **UNKNOWN** | Global TVERSION has zero rows; no authoritative client assets/patch catalog pairing |

The full **per-artifact matrix**, including suspected generation, compatible
implementation, evidence, conflicts and HIGH/MEDIUM/LOW/UNKNOWN classification, is
[version-compatibility-matrix.json](evidence/version-compatibility-matrix.json).
All exact game-version fields remain UNKNOWN. HIGH database-lineage confidence
does not imply HIGH game-version confidence.

Important historical anchors: global `TCheckPasswd` was created/modified in May
2015; global `TLogin` modified 2015-05-26; game `TCreateChar` modified 2015-12-30;
`TGetMountSaddle` modified 2018-08-17. These timestamps are catalog metadata,
not proof of release order, authenticity of clocks, or a complete migration chain.
Physical column-ID gaps also prove dropped-column history, without identifying
the dropped definitions or a game revision.

`git log` associates the checked-in schema directories with initial commit
`c77f790` (2026-05-18); current server HEAD is `47762c5` (2026-07-07). The duplicate
directory name `old-dump-2019` does **not** identify an older schema than `schema`:
their files are byte-identical. Fragments therefore share the same observed
backup lineage, with export-side procedure renames described in report 04.

The portable rewrite contains later requirements (2FA, peer authentication,
registry/metrics, modern audit, Bow procedure names). These are source revisions,
not evidence that another complete game/database generation was supplied.
The old `SCHEMA.md` claim of original compatibility 100 actually describes the
post-restore value; its C#/EF Core architecture and general type assumptions do
not describe this checkout's C++ rewrite. `SQL_AUDIT.md`'s claim that the modern
stack invokes no stored procedures is obsolete for current Control/World/Patch.

There is also independent evidence of **different catalog structures and values
inside the two co-dated databases**: global/game TITEMCHART have 44/52 columns and
2,519/7,706 rows; only 2,517 item keys overlap, and 37 shared numeric fields contain
mismatches on at least one overlapping key. Monster catalogs have 1,811/3,534 rows,
1,216 overlapping keys and differences in 27 shared numeric fields. These may
reflect different database roles, stale caches or content revisions; no exact
game release can be inferred. See cross-database-catalog-comparison.json. The
target importer deliberately selects the TGAME catalog and never unions both.
