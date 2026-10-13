# Native mail return/payment and historical importer — Phase A

Code `d3904c5843478682263a3b75d4b2db964bbd2e65`, based on `802a232`, local main.
Production commits `5d97721` and `d3904c5` form this increment.
**INTEGRATED_UNVERIFIED** runtime scope; importer **IMPLEMENTED_UNVERIFIED**.
The complete Phase A gate remains open. [Exact compile metadata](implementation.json)
records compilation/linking only. Tests0; migration/import/reconciliation0;
server/client runs0; containers0. No original executable/assets are available.

## Source contracts and actual integration

- `Server/TMapSvr/CSHandler.cpp:8949 OnCS_POSTRETURN_REQ` and client
  `CSSender.cpp:881` define DWORD postID plus BYTE payment/return. The dispatcher
  retains secure-code prompting before decoding, Tutorial/tournament admission,
  viewed-post selection and unread gating. Invalid selection/read posts remain
  silent. Any nonzero BYTE means payment, as the original source does.
- `TPostBillsUpdate.sql` defines recipient authorization and locking of one unread
  post. Return keeps its ID, exchanges sender/recipient IDs and names, clears
  amounts, sets type3/read0, refreshes minute-rounded time and changes every
  STORAGE_POST attachment owner. The deferred recipient FK is checked again
  before proceeding. Payment uses the original64-bit CalcMoney arithmetic,
  creates type4 money mail, marks the original read and clears its billed funds;
  attachments remain with the payer for a later ordinary GETITEM claim.
- Original `CTPlayer::UseMoney` title effects and MONEY precede POSTRETURN BYTE0
  for payment. Return replies with POSTDEL DWORD ID, as `PostReturn` does in
  `TMapSvr.cpp:7866`; it does not substitute a return ACK. Native purse, post,
  attachments, notification, quest head and checkpoint commit before packets.
  No equal-byte deduplication or retry after uncertain COMMIT was introduced.
- `DBAccess.h:2586 CTBLPostBill` selects **dwSendID**, type2/read0. The original
  online timer belongs to the sender, not the recipient. `TMapSvr.cpp:6138`
  expires deadlines at <=current seconds; `SSHandler.cpp:6159` rebuilds them from
  minute-rounded timeRecv plus POST_DURATION259200 seconds (86400*3). Native ready-primary ticks
  perform these sender-owned returns. A nullable bill_due_at reserves the exact
  live-second deadline for future native send producers; old/imported rows use
  the source database-load deadline. Fixed seconds avoid connection-timezone DST
  drift. The offline SQL job remains unimplemented.
- Transient World POSTRECV routing is replaced within this scope by native durable
  mail_notifications. Only the locked current ready primary consumes a matching
  recipient/type, retaining owner token, connection, authority epoch and key.
  Idle primaries run internal opcode2; active ticks also poll. An empty poll
  does not append a quest ledger/checkpoint. Original POSTRECV53A4 layout and
  Tutorial suppression remain; at most256 events/deadlines are handled per tick.
- Admission/list counts and the notification watermark come from one SELECT
  snapshot. Persist the server watermark separately in map_checkpoints so
  transfer/reconnect cannot double-count events already included in POSTINFO or
  POSTLIST. The watermark is absent from all original protocol bytes. Queued
  events already covered by those counts are consumed without another POSTRECV.
- STORAGE_POST has normalized authority even for graph primaries. Hydration
  replaces its graph read projection with actual owned attachments. Carried
  inventory and all other unchanged storage contracts retain their existing
  authority. Claims and returns never reinsert another recipient's stale graph.

## Explicit modern repairs and unresolved source differences

Atomic commit-before-send replaces the original early purse/item change plus
later database callback/refund path. A missing sender row retains the recipient's
unread contents and returns NOTFOUND8 for a manual request; no orphan recipient
or item is invented. Missing original non-ASCII bytes abort before a money debit.
Only ASCII can be recovered from a decoded-only row without guessing a code page.

Durable notifications can survive a routing/process interruption. Consuming a
notification in PostgreSQL is **not** an original-client delivery ACK. A lost
postcommit packet is not replayed blindly and does not delete the actual mail;
POSTINFO/POSTLIST still expose the durable state. Admission/list watermark
suppression is an explicit counter-consistency repair. Pending events whose
post recipient/type has subsequently changed are not presented to the old owner;
full retention/cancellation policy belongs to the remaining mail lifecycle.

`TJobsPostReturn.sql` differs from the server procedure: it gates on the recipient
being absent from TGLOBAL_GSP.TCURRENTUSER and updates TPOSTITEMTABLE, whereas
TPostBillsUpdate and runtime mail read TITEMTABLE/STORAGE_POST. Do not silently
port the job or fabricate equivalent old attachment attributes. Full offline
expiry/World jobs still need this contract resolved and integrated.

## Historical importer, written only

`tools/database/import_mailboxes.py` requires a complete hash-pinned manifest and
an already verified archival import, exact injective character mapping, explicit
source timezone, source-varchar codec and wire codec. It checks decoded values
against original bytes, all source table hashes and archival byte hashes.

Backup metadata distinguishes varchar szRecvName from nvarchar sender/title/body.
The importer verifies UTF16LE source nvarchar bytes and the explicit varchar
profile, then encodes strict original-client wire bytes. It retains archival
bytes unchanged and stores both decoded native text and wire representations.
No profile has been executed or accepted as binary compatibility proof.

All worlds must be offline: native owner advisory locks, settled sessions and
write-blocking parent/mail/item locks precede import. Existing mailboxes, recovery
receipts, duplicate slots, invalid owners/counts, unrepresentable text, unknown
clock transitions and conflicting IDs are transparent blockers. No overwrite,
row-drop or invented historical defaults. Original IDs, signed funds, NULL
bContain, full item attributes/magic and six DWORD extensions are preserved.
Native wCustomTex is the explicit low16-bit projection of original dwTime6,
while full dwTime6 remains intact. Item allocation uses the exact original index;
post identity advancement is monotonic and may leave harmless sequence gaps on
rollback. An import receipt records profiles, mapping and source row counts.

Nonempty TPOSTITEMTABLE cannot be materialized as TITEMTABLE: its rows lack the
item ID, durability and expiration needed by the active source contract. They
stay in the complete archive and block this projection rather than being silently
ignored. Empty authoritative source mail creates no fictional player content.

Migration060 and refreshed Map grants are prerequisites. Migrations001–059 and
both backups remain unchanged. **No migration/import command was run.**

## Continuation and deferred Phase B acceptance

- Implement POSTSEND/TPostCanSend including native protected-list/country/name/
  capacity authority and source trade permissions; use this same item/post path.
- Finish World reserved/quest/event/admin producers, offline expiry and obsolete
  attachment/job reconciliation; complete source save staging and notification
  lifecycle. Full mail and the full rewrite are not complete.
- Later Phase B: independent request/reply order; payment versus return; original
  signed/unsigned money boundary; multiple attachments and empty bills; same
  request twice; blocked/unknown/read selection; source deadline boundary and
  sender logout; mailbox visibility across two Map servers; Tutorial suppression.
- Exercise simultaneous payment/return/expiry/claim, deleted sender, stale graph
  and checkpoints, transfer before first notification poll, list before poll,
  admission snapshot races, disconnect/restart/DB outage/lost COMMIT and response.
  Check upgrade001–059→060, dedicated grants, full import profiles and sequence
  allocation. These scenarios were **not executed**.
- Broader combat/AI/general effects/companions, World/social/economy/war/BOW/BR,
  operational tools and remaining legacy SQL calls still keep Phase A open.
  The whole runtime is not PostgreSQL-only. Phase B/C have not started; supported
  original executable identity and gameplay acceptance remain unavailable.
