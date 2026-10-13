# Phase A — original quest mail and offline bill returns

Code `e84ece4` and `65be6d1`, plus SQL grant restriction `c2a90ba`, based on `c537883`, local main only.
Production paths are **INTEGRATED_UNVERIFIED**; migration064/065 and content
publication are **IMPLEMENTED_UNVERIFIED**. Phase A remains incomplete. No
test, migration, database publication/import, server, client or container ran.

## Source and production paths

`Server/TMapSvr/QuestSendPost.cpp::ExecQuest` selects only ITEM/MAGICITEM,
draws before CanRun, ignores SELECT and uses wrapping BYTE RANDOM accumulation
without COMPLETE's sum-break. Missing templates/class matches skip. Stack chunks
retain raw magic and all six extension fields. An ordinary temporary item keeps
ID0; magic Copy(TRUE) allocates a temporary Map ID. World allocates another ID
at delivery. No temporary item enters the player's inventory.

Existing native QUESTEXEC and child/reward hooks now reach the SENDPOST branch
of `PostgreSQLMapService::ChangeQuest`, then `QueueQuestPost`. The existing
account/primary owner/connection/epoch/checkpoint/operation head fences protect
queue insertion and subsequent child effects in one transaction. The existing
World minute worker delivers the queue; the existing Map receipt poll sends
original POSTRECV53A4 with recipient live time. There is no new client opcode,
mandatory field or direct SENDPOST success ACK. Equal valid command bytes remain
repeatable actions, never a deduplication key.

The backup metadata, rather than an inferred Unicode schema, proves
TQTITLECHART contains varchar255 title, varchar1024 message and varchar1024 NPC
name with Latin1_General_CI_AS_KS. Written064 publishes all original byte rows.
`activate_quest_catalog.py --quest-mail` requires the complete pinned archival
manifest, byte/value checkpoint equality, unique quest IDs, strict CP1252
text/byte agreement, current compatible Map/character/actor receipts and offline
Map owners. It prevents a later older profile from removing this publication.
No historical title, sender or reward is fabricated. The actual publication has
not run. Missing/NULL title data is the source SP return2: no mail, but a true
original bRun still runs children. That branch is not a successful mail response.

The source SQL casts sender to255, prefixes the **pre-assignment byte length**
of title/message as eight hex characters, then truncates prefixed output again
to255/1024. The native view preserves those lengths and truncations using exact
archived bytes. SQL-literal prefix casing and byte equivalence still require
independent Phase B comparison. NUL, undefined/unrepresentable CP1252 bytes,
oversized sender, source BYTE overflow and zero-stack divisor are rejected
transparently. The archive is retained, not corrected or silently shortened.

`TJobsPostReturn.sql` uses an **offline recipient**, unread type2 and stored
timeRecv+3 days. This differs from the sender-owned online Map bill timer. It
changes recipient/name/type/funds/time and intentionally leaves dwSendID alone;
the online `TPostBillsUpdate` swaps dwSendID. Native `ReturnOfflineBills` preserves
both distinct contracts. Existing World DB workers run it after reserved
delivery. An account row lock shared with Login admission, offline recheck,
ordered nonblocking mailbox locks and row locks precede the mutation. Post,
unchanged raw item IDs/attributes, new item owners, superseded old notifications,
new durable notification and audit commit together. The normalized mailbox
remains authoritative for graph characters across relogin/handoff.

## Explicit repairs and limits

- Backup TQuestSendPost omits NOT NULL/no-default wMoggItemID. The native producer
  explicitly takes the original CTItem appearance field (ordinary constructor0,
  magic Copy's appearance); no historical default is invented. The original
  procedure/archive stay unchanged. Its ignored bGradeEffect parameter selects
  the proved source default0; bGem remains its literal0.
- The offline job writes obsolete TPOSTITEMTABLE. The native replacement moves
  sole authoritative STORAGE_POST owners alongside the post under the deferred
  recipient FK. It retains full item identity/attributes and prevents an old
  graph projection restoring attachments. Nonempty obsolete historical rows
  still require explicit importer reconciliation; this runtime repair does not
  pretend to reconstruct their missing identities.
- Offline delivery uses the existing World minute hook, a durable minute gate,
  stable post-ID order and maximum128 atomic returns per batch. Historical SQL
  Agent job schedule metadata is unavailable; this native schedule/bound is an
  explicit modern choice, not a reconstructed source job schedule. Locked posts
  can wait until a later minute. Busy accounts/mailboxes roll the minute back.
- A missing hard-deleted sender retains the original bill/items and emits a
  count-only warning. Soft-deleted existing rows retain source treatment. This
  prevents orphaning; no recipient is invented. Non-ASCII text without original
  mail bytes requires reconciliation rather than a guessed wire encoding.
- UTC/minute rounding follows the existing native clock policy; zero item expiry
  uses source __TIMETODB1900-01-01, nonzero expiry respects SMALLDATETIME bounds.
- Unknown COMMIT poisons the mail service and stops World; it is not replayed.
  Durable queue status/audit/minute state is inspected after restart. The source
  cursor/per-item side effects are replaced with bounded atomic transactions.
- Full World bootstrap still validates legacy tables and instantiates other
  legacy repositories. This is actual target/timer integration, not evidence of
  a complete runnable PostgreSQL-only World or complete gameplay.

## Compilation and deferred acceptance

Map and World compiled/linked in Debug and Release with C++20,
BUILD_TESTING=OFF and native PostgreSQL enabled. Commands:

```sh
cmake --build build/phase-a-compile --target tmapsvr_asio tworldsvr_asio -j1
cmake --build build/phase-a-quest-release --target tmapsvr_asio tworldsvr_asio -j1
```

Both final commands exited0. Logs include earlier Map-only compilation of the
new quest-mail TU; final combined builds cover offline bill integration. Existing
warnings remain. No executable was launched. Python publication was AST-parsed
only, without importing its modules or connecting to a database. Exact code/file/
backup hashes and execution counts are in [implementation.json](implementation.json).
Backups, original server/client sources, migrations001–063, existing tests and
older reports remain unchanged; untracked `_rewrite/docs/audits/` is untouched.

Phase B must independently check source RANDOM wrap/multiple rows, selected and
missing/null rewards, magic/raw field losses, temporary/delivery IDs, complete
publication and long prefix truncation, recipient offline/online races, stored
versus live expiry, two Worlds/Maps/Login, both retained/swapped sender-ID paths,
missing sender and soft deletion, deferred FKs/roles, queue+child rollback, repeat
valid commands, lost commit/reply, process/DB failure, restart and transfer.
No original client executable/assets exist here, so supported binary identity
and real-client acceptance remain unavailable. Sanitizers/tests/imports/migrations
and full container deployment remain deferred by the incomplete Phase A gate.

Continue full native World ownership/social/bootstrap, other quest map/monster/
effect actions and event producers, admin/event/BR/rank mail, notification
retention, complete combat/AI/economy/social/modes and operational scope. These
two integrated paths do not complete the whole mail or gameplay implementation.
