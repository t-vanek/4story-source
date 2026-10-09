# 10 — Prioritized implementation roadmap

| Priority | Concrete step | Acceptance evidence |
|---|---|---|
| **P0** | Establish authoritative client/content/server pairing and freeze source hashes | Explicit version/protocol matrix; signed-off UNKNOWNs or supplied release manifest |
| **P0** | Database compatibility view implemented through legacy wMonAttr; review remaining 39 gaps and bad NPC coordinate, then connect the C++ PG backend | Migration 006: 3,495 real matching monster stat rows; C++ spawn/stat/combat integration still required |
| **P0** | Extend derived PostgreSQL contracts for guild validator drift, inventory expiry/container semantics and missing required calls, retaining backup authority | Each derived transform/extension has a reviewed behavior fixture and owner; optional features explicitly disabled if unavailable |
| **P0** | Define auth wire/hash, source collation/timezone and item/world ID policies; redact rendered SQL logging | Synthetic auth/ID/time/encoding cases, duplicate-login/slot/item invariants, privacy audit; no unknown account data conversion |
| **P1** | Enable SOCI PostgreSQL on Linux through an explicit build option and link/backend test; keep Windows ODBC supported | Fresh Linux container SessionPool PG acquire/query/transaction test; unchanged ODBC checks |
| **P1** | Implement first complete persistent vertical: account/session + character create/load/save/delete and reviewed item ID allocator | Canonical DDL + explicit legacy transforms + real PG repository tests including rollback/concurrent slot/name/session conflicts |
| **P1** | Extend canonical catalogs from immutable snapshots; configure an approved active content release and production roles | Content semantic checks, name/ID mapping, checksum/byte verification and cache boot without old cross-DB wrappers |
| **P2** | Port inventory/item transfer, quests/skills, guild/social/mail/auction transactions | Idempotent operations, owner/location constraints and failure/concurrency tests; populated migration corpus needed |
| **P2** | Replace event/rank/Bow/tournament/admin procedure responsibilities using existing workers/services | Preserve return/output contracts; award idempotency, schedule recovery and no hidden SQL Server fallback |
| **P2** | Add streaming/chunk checkpoints and restricted player/audit extraction | Repeatable large imports with interrupted recovery, exact byte/value/row reconciliation and no private row output |
| **P3** | Optimize indexes, persistence batching, retention and partitioning from measured workloads | EXPLAIN plans, contention/concurrency tests and recorded benchmarks; no fabricated throughput claims |
| **P3** | Containerized staging rehearsal, client smoke flow and controlled deployment rollback plan | Full login→load→save→transfer→logout behavior on pinned client/catalog release, coordinated backups and recovery drill |

Recommended next implementation unit: **P0 source/schema compatibility contract
and monster-stat key correction**, then P1 opt-in Linux PostgreSQL SessionPool and
the account/character vertical. Merely switching backend or applying a dev schema
would conceal the observed content/behavior conflicts. Do not migrate production
players until their corpus and auth/encoding/time/ID policies exist.

This task delivered the isolated forensic/import foundation. The current server
still uses MS SQL/ODBC; no wholesale repository rewrite, new application ORM or
unrelated server changes were introduced.
