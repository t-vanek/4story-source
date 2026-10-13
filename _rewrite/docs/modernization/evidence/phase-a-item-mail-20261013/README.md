# Item expiration and native mailbox — Phase A

Code `18ee496958c3ab262911aa9a9e94a5df8c6c50cf`, based on `abc40d4`, local
`main`. **INTEGRATED_UNVERIFIED** within this scope. The complete Phase A gate
remains open. [Build metadata](implementation.json) records compilation/linking
only. No tests, sanitizer/fuzzer, migration/import/reconciliation, database,
server/client process or container was run for this increment.

## Original contracts and runtime connections

- `TPlayer.cpp:4983 CheckItemEndTime`, `TMapSvr.cpp:6844 SetAlarmTime` and
  `CSSender.cpp:5056 SYSTEMMSG` establish strict30/20/10/1-minute thresholds,
  scheduled interval reporting, unsigned bag/slot iteration and personal-store
  suspension. New owner-local alarm fields never enter client/transfer bytes.
  New Copy/SetItemAttr operations rearm; swaps/count merges/stat derivation retain
  alarms. Fresh load/transfer reconstruct alarms, as the original CreateItem does.
- The existing one-second Map tick now admits due item/bag expiry and all pinned
  probability consumables. Opcode0 remains internal only. The ready-primary
  lease and native ChangeQuest transaction retain account/claim/owner/epoch,
  inventory/checkpoint and operation-head fences. All mutations commit before
  responses; ambiguous commits quarantine the session and are not retried.
- Entire expired instances, including count0, are removed without USEITEM/
  GETITEM hooks. Equipment expiry reuses the ordinary ChangeEquipItem/posture
  path: DELITEM, neighbor EQUIP, private MOVE/CHARSTAT/HPMP, ordered skill removal,
  final private MOVE. This covers existing native supported statistics/postures;
  full active-effect/party/recall dependencies remain incomplete.
- `SSSender.cpp:4609`, `SSHandler.cpp:14875/14949`, TSavePost and TSaveItemDirect
  define expired-bag mail: remaining items in unsigned slot order, groups of three,
  sender message6/NAME_OPERATOR, message1/MSG_POSTINVENITEM, empty-header
  BuildNetString, type1/POST_PACKATE, read0 and zero funds. Bag removal and durable
  ownership now commit atomically. INVENDEL5366 is BYTE result/bag/showIF; queued
  POSTRECV53A4 is DWORD ID, BYTE read/type, sender/title strings and INT64 time.
  Source Tutorial suppresses POSTRECV and its live counter increments.
- TSavePost stores SMALLDATETIME, while POSTRECV sends current seconds. Native
  posts retain minute rounding and live notification seconds under explicit UTC
  policy. Raw item IDs, quantities, six extensions and source magic values are
  retained. Graph-owned items materialize only their mail attachments, retiring
  their own stale carried projection; foreign IDs cannot be overwritten.
- `CSHandler.cpp:8740/8772/8824/16542`, `SSHandler.cpp:10560/17628`,
  `CSSender.cpp:3706` and TGetPostInfo/TPostView/TPostGetItem/TPostDelete establish
  POSTLIST54A5 (WORD), VIEW53A6/GETITEM53AA/DEL53A8 (DWORD). Original response
  fields and no-slot item descriptors are retained. Protected claim/delete
  prompts precede decoding; Tutorial/tournament gates precede operations.
- Preserve unusual pagination: seven IDs advance beginID, but CTBLPostTable
  returns every remaining row; beyond the last page the last ID remains selected.
  Do not silently impose seven-row pagination. View missing IDs yields no ACK,
  as the source callback does. Viewing marks ordinary posts read but does not
  immediately decrement live unread counters; listing resynchronizes them.
- GETITEM requires the same previously viewed post. Capacity precedes mutation.
  All attachments/funds, stack identity placement, GETITEM quest triggers, gold
  titles and recovery checkpoint share one transaction. Repeated valid request
  bytes are allowed; an already claimed empty post returns POST_NOITEM2. Money
  uses the source EarnMoney64-bit amount. Entry sends original POSTINFO54A6 with
  two WORD counters; TGetPostInfo explicitly counts bRead=0 as PostRead.

## Unapplied database changes and explicit repairs

Migration059 extends native STORAGE_POST2 with a recipient FK and one global item
identity. Inventory/mail slot namespaces are independent; swaps remain deferred.
The duplicate post_id ownership column is excluded from item fingerprints so
adding NULL does not change existing hashes. Posts with attachments cannot be
deleted by cascading their contents. Grants cover only the required mutations.

`activate_quest_catalog.py --server-messages` publishes the complete pinned
TSVRMSGCHART with original varchar bytes and checks active Map/character/actor
manifests. Publication remains offline and cannot silently downgrade an extended
profile. No operator names/messages are fabricated. Missing/oversized source text
aborts expiry before any client response or lost item.

The backup's TPOSTTABLE uses nvarchar fields; server-message sources use varchar.
Decoded native text and original wire bytes are separate. TSavePost omits nullable
bContain; backup metadata has no default/computed definition. The modern native
containment projection derives actual funds/attachments, justified by client
`CSHandler.cpp:9880/10001` and `TMailListFrame.cpp:606` package-state consumers.
This is an explicit repair, not a reconstructed historical value. Archives stay
unchanged. Exact SQL Server ANSI binding/collation for future full mail imports
remains an acceptance dependency.

Additional intentional repairs: refuse unpaid bill grants with POST_NEEDSETTLE10;
the original GETITEM omits that guard. Protect every nonempty post from deletion;
TPostDelete tests attachment ROWCOUNT=1 and can orphan multi-item contents.
Reject unbounded packet overflow and invalid attachment counts rather than copy
undefined memory. Ordinary item upgrade/wrap helpers now own before-values,
avoiding references into an immutable payload replaced during mutation.

For **later Phase B only**, apply001–059 with the existing migration tool, refresh
Map grants and publish the full archive using `--server-messages` plus all already
published options (`--special-boxes --item-gambling --item-upgrades`). No such
command was executed. Migrations001–058 and both original backup hashes are intact.

## Exact remaining implementation and later acceptance

- POSTSEND, POSTRETURN, bill settlement/expiry/return jobs, World reserved/
  quest/event/admin mail, cross-player notification receipts, offline complete
  historical mailbox import and staged save reconciliation remain required.
  Add these to the existing native ownership path. No full mail completion claim.
- Store/deal/tournament producers, general maintained effects/recalls/party
  statistics, BOW/BR variants, broader World/social/economy/combat/AI and the whole
  source responsibility inventory still keep Phase A open. Runtime is not yet
  fully PostgreSQL-only. Original executable/assets are unavailable; supported
  binary build and original-client gameplay acceptance remain undetermined.
- Phase B: exact independent alarm boundary/delayed/clock-step packets; per-bag
  item-before-bag expiry; three-item grouping; empty bags; store pause; equipment
  responses; byte encodings; source pagination and missing/view/read selection;
  full inventory/merge/split, amounts >DWORD, repeated claims, secure malformed
  requests, Tutorial/tournament/bills, attachment FK and nonempty deletion.
- Exercise fresh/graph ownership, stale snapshots, simultaneous mail/item writes,
  transfer interruption, disconnect/reconnect, DB outage, process restart/crash,
  failed/lost COMMIT and lost response. Confirm one attachment owner, no revival
  from stale normalized/graph projections, preserved hashes/grants and upgrade
  001–058→059. Original-client acceptance is separately blocked, not passed.
