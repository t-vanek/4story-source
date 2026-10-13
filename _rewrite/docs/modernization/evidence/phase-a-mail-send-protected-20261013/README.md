# Phase A — native player mail and protected lists

State: **INTEGRATED_UNVERIFIED**. Migration/import tooling:
**IMPLEMENTED_UNVERIFIED**. Complete Phase A, PostgreSQL-only World/gameplay,
production readiness and original executable compatibility are not established.
Code starts at `85bf923`; `feea4a1` redacts importer diagnostics. The final source
time correction and exact file hashes are recorded in `implementation.json`.

## Source contract and runtime path

- `CSHandler.cpp::OnCS_POSTSEND_REQ` (8635) checks Map/primary, secure prompt,
  tutorial/tournament and DEAL_READY/store before decoding. Request53A2 is three
  raw INT32-length strings, BYTE type, three DWORD funds, BYTE bag and slot.
  String maxima50/256/2048, exact live-name self check and nonempty title remain.
- Backup `TPostCanSend.sql` resolves recipient, reads both saved countries,
  refuses opposite country0/1, counts all inbox rows for types below3 (limit20)
  and refuses **any** recipient protected row for the sender, independent of
  option bits. Its same-account check is commented out and remains disabled.
- `SSHandler.cpp::OnDM_POSTCANSEND_ACK` (10816), `GetPostMoney` (7832),
  `TItem.cpp::CanDeal` (699) and `TPlayer.cpp::CheckDealItem` (635) define fees,
  title-bearing affordability checks and full item trade/cabinet rules.
  Fees are100/300/900. Bills debit only the fee; other types include funds.
  Preserve the DWORD copper/silver intermediate wrap and64-bit gold arithmetic.
  INVEN_NULL is **0xFC**, not default bag0xFF. The entire original item is sent.
- Existing dispatch → `OnNativeQuestReq` → `ChangeQuest` reaches
  `SendNativePost`. Existing account, owner token, session key, connection,
  authority epoch, checkpoint and mutation-head locks fence the transaction.
  Inbox advisory locks serialize admission/count with native block changes,
  expired-bag delivery and return/payment producers. SQL stores post, signed
  money fields, raw wire text, full item ownership, package gold-error audit,
  notification receipt and sender checkpoint atomically. Graph-owned items
  materialize only their own old projection and are removed from the sender graph.
  The original item ID, count, magic, expiry, full extensions and slot are retained.
- Source immediate sender packet order is DELITEM, MONEY and POSTSEND53A3 BYTE0,
  with source title packets at affordability/debit stages. No GETITEM quest hook
  is invented for removing a mailed item. Recipient delivery uses the existing
  current-primary durable notification timer and POSTRECV53A4.
- `OnDM_POSTRECV_ACK` (11049) and `OnMW_POSTRECV_REQ` (10710) use the
  **receiving Map's current time**. The World packet carries no timestamp.
  Therefore final code sends the recipient poll's live second, not the enqueue
  second or saved minute. Written062 renames061 `received_at` to
  `enqueue_second` and documents it as audit metadata only. This corrects the
  initial85bf923 interpretation without rewriting written061. Return atomically
  supersedes pending receipts whose recipient/type no longer matches the post.
  Receipt commit remains distinct from a client delivery acknowledgement.

## Protected state

`OnCS_PROTECTEDADD_REQ` (10490), ERASE (10522), OPTION (15185),
`OnDM_PROTECTEDADD_ACK` (12766), original CSSender3557–3600 and backup
TProtectedInsert/Delete/SaveProtectedOption supply the independent contract.

Original5377/5379/5472 requests reach native SQL. Remember the exact requested
name, not the target's later renamed display name; add begins with option1.
The source size check is **greater than200**, so201 entries are allowed. SQL
insert lookup includes soft-deleted targets. All eight option bits are retained;
option on an absent target deliberately has no response or write. Erase uses
exact remembered bytes. Add publishes World presence before client ACK; erase
uses client ACK then World. World accepts only the exact registered ready primary.
Entry LIST5376 follows POSTINFO and uses BYTE count, then DWORD/string/BYTE.

Normalized SQL is authoritative on fresh/recovered/transferred loads. Capture
uses the existing original protected transfer fields without new client metadata.
Source TDeleteChar removes only owner rows, so no target FK is invented.

## Explicit modern changes and limits

- Commit before publication replaces separate legacy saves/compensation, which
  could lose an attachment or currency after a later DB failure. No unknown
  COMMIT is automatically replayed; the existing handler closes uncertain state.
- Option persistence is immediate and fenced rather than delayed until logout.
- Player mail types above2 are refused with source NOTDEAL12: a forged client
  packet must not impersonate NPC/operator/payment/cash authority or bypass caps.
  Equipped mailing is refused with12 because the legacy handler transfers the
  equipment without cleanup. Raw DWORD wrapping still participates in CanDeal.
- Embedded NUL, invalid frames and unrepresentable text are rejected explicitly.
  `native_map.client_text_encoding` selects a proved ANSI-compatible PostgreSQL
  codec for non-ASCII mail input. Empty means invariant ASCII only. Existing
  native character names are ASCII; other historical names/CI collation need
  their own source-backed projection and acceptance. No default ACP is guessed.
- World friend erase still uses its existing repository and needs a complete
  native transaction/ownership replacement. Chat/invite/deal/duel consumers,
  trade reservation producers and full social behavior are not certified here.
- Reserved/quest/admin/event producers, offline expiry/job reconciliation,
  terminal receipt retention and full native gameplay remain required.

## Historical import and later acceptance

`import_protected_characters.py` requires the complete pinned archival manifest,
prior verified archive hashes and original VARCHAR name bytes, exact injective
character mapping, explicit source/wire codecs, offline owners and settled sessions.
It preserves remembered names/options, rejects unreconciled or unrepresentable
rows, and refuses existing protected/recovery state. It never creates identities,
guesses names or retries an unknown commit. Diagnostic text follows mailbox
importer redaction. The historical profile records zero protected/post-error rows;
this is older evidence, not a new extraction/import execution.

The written CLI is `python3 tools/database/import_protected_characters.py` with
`--manifest`, `--world`, `--character-map`, `--source-varchar-encoding` and
`--wire-encoding`; connection inputs use the existing PGHOST/PGPORT/PGDATABASE/
PGUSER/PGPASSWORD or PGPASSFILE. This invocation has **not been executed**.

Map and World compile/link in Debug and Release (logs/commands in the manifest).
No unit, integration, regression, TCP, protocol, real PG, sanitizer, migration,
import, server, client or container run occurred. Historical tests are untouched.
Pending Phase B scenarios include source-derived packet/error order; locked
malformed packets; self/case variants, countries, capacity20/201 and all options;
concurrent send/block/return; full raw graph/fresh attachment lineage; stale
checkpoint/owner; lost replies, DB/process failure and unknown commit; relogin
and multi-Map counters; strict codec/collation and real-client UI. No original
client executable/assets or exact supported binary identity is available.

Backups, original server/client sources and migrations001–060 retain their hashes.
Written061 is retained and corrected additively by062. Unrelated untracked
`_rewrite/docs/audits/` is preserved. All changes and commits are local on main.
