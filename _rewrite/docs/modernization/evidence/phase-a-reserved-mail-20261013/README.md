# Phase A — native World reserved-mail delivery

Code `45d130d`, based on `0f05d33`, local main only. Delivery is
**INTEGRATED_UNVERIFIED**; written063/grants/importer are
**IMPLEMENTED_UNVERIFIED**. Complete Phase A and native World startup/gameplay
remain incomplete. No database or executable was run.

## Source and production path

Original `TWorldSvr.cpp::OnTimer` posts DM_RESERVEDPOSTSEND on minute change.
`SSHandler.cpp::OnDM_RESERVEDPOSTSEND_REQ` (6556) calls TGetReservedPost up to30
times, breaking on any nonzero result. TGetReservedPost selects pending bSend0,
includes soft-deleted recipient rows, resolves the current sender name to its
character ID or0, creates unread package type1 with zero funds, allocates a fresh
global item ID, stores all raw item fields, then marks1. Missing recipient marks2
and stops the batch. No country/block/capacity gate is invented for this producer.

Existing World `main.cpp` selects `PostgreSQLReservedMail` automatically for its
native PostgreSQL backend and configured `server.group_id`, requiring DB worker
threads and migrations001–063. Its one-second timer offloads the delivery to
the existing worker/pool. Native SQL stores queue/status, post, full raw item,
delivery lineage and durable notification. The existing fenced Map poll delivers
original POSTRECV53A4 with the **receiving Map's current second**. No timestamp or
new recovery metadata is added to the World/client wire. Disconnected recipients
retain normalized mail authority for admission/reconnect.

The batch keeps the source limit30 and missing-recipient break, exact signed
WORD/DWORD bit patterns, six magic percentages/extensions, original expiry,
refine, grade effect, gem and appearance. Storage is2, owner type0, slot0. New item
IDs use the existing fenced world high-water range **at delivery**, without
deleting a collided item or regenerating template attributes. The original queue
dwSeq is retained separately from new server-only queue identity.

## Deliberate modern changes and limits

- The whole bounded batch commits atomically, instead of the legacy separate
  per-row procedure side effects. Source terminal rows imported as1/2 have no
  invented historical post/item IDs.
- A durable epoch-minute gate and shared batch advisory lock prevent duplicate
  concurrent/restarted World ticks. Stable queue_id order replaces unordered
  TOP1. Native timestamps keep the existing explicit UTC policy and rounded
  stored minute; historical calendar conversion requires an explicit IANA zone.
- All mailbox locks use the same shared namespace as native Map producers and
  protected changes. World tries recipient locks in unsigned order before the
  item allocator, and rolls back a busy minute claim instead of waiting in a
  Map/allocator cycle. DB lock/statement waits are bounded5/15 seconds.
- Post/item/notification/status/clock commit together. An unknown COMMIT poisons
  the service and stops World with exit1; no automatic replay. Other DB failures
  also stop for reconciliation instead of pretending an empty queue. Fresh
  startup reads durable queue status and delivery lineage.
- Zero template/count or embedded NUL is explicitly rejected without consuming
  the queue. Native recipient-name and sender lookup retain the present ASCII/C
  collation profile; full historical non-ASCII identity/collation is not claimed.
- Full World bootstrap still uses `ValidateWorldSchema` and legacy SOCI guild,
  friend, event and other repositories. This delivery worker is wired into the
  real target; it does **not** establish a runnable PostgreSQL-only World cluster.
  No new empty repositories, fake schema or successful stubs hide those gaps.

## Import and unresolved producers

`tools/database/import_reserved_mail.py` requires the complete pinned archival
manifest, unchanged archived values/text bytes, exact injective recipient mapping,
offline native owners and settled sessions, explicit source VARCHAR/wire codecs
and source timezone. It preserves every queue status and raw attribute, including
terminal rows. Source strings already contain any encoded length prefix; it does
not prepend again or guess truncation. Unrepresentable output or unreconciled
recipient identities are rejected while the archival rows remain unchanged.
Existing queue/delivery clock cannot be overwritten. No item identity is allocated
by the importer and no unknown commit is retried. Diagnostics are redacted.

Written CLI: `python3 tools/database/import_reserved_mail.py` with `--manifest`,
`--world`, `--character-map`, `--source-timezone`, `--source-varchar-encoding` and
`--wire-encoding`; standard PG connection variables come from `migrate.connect`.
The command was **not executed**. `deploy/sql/world-mail-runtime-grants.sql`
contains only incremental delivery permissions; complete World role grants are
still required. Historical data-profiles.json records0 queue rows, without any
new extraction/import claim.

Quest/admin/event/BR/rank producers remain implementation work. A concrete source
conflict is now recorded: backup `TQuestSendPost.sql` omits bGradeEffect and
wMoggItemID from INSERT. Backup-schema.json proves bGradeEffect has default0, so
its passed parameter is discarded; **wMoggItemID is NOT NULL with no default**.
Do not invent an appearance value or silently claim that procedure is functional
against this backup. The native quest producer needs a documented repair along
with original TQTITLE VARCHAR conversion and prefix/truncation semantics. Existing
ordinary item rewards cannot simply be reused unchanged: SENDPOST takes RANDOM
rows without the completion routine's accumulated-sum break, and World allocates
the delivered item identity independently.

## Compilation and pending acceptance

Map and World compile/link in Debug and Release. Final World builds include the
fatal exit1 handling. Commands, exit0 results, exact commit/file/backup hashes and
logs are in implementation.json. BUILD_TESTING=OFF; no binaries were executed.
No test, fuzz, sanitizer, migration, import, DB reconciliation, server, client or
container run occurred. Migrations001–062 and historical evidence are unchanged.

Later Phase B must cover independent source packets; minute boundaries and quota;
missing/soft-deleted recipients; full raw item lineage; concurrent World ticks,
Map send/claim/return/block and allocator contention; busy rollback, item collision,
restart/process death/lost commit/DB failure; terminal import statuses and clocks;
explicit charset/timezone rejection; relogin/multi-Map mail counters and original
client UI. Original EXE/assets and supported binary identity remain unavailable.
