# Native Map RPS production, completion and rewards — Phase A

Baseline: local `main`, `c379231` (World RPS continuation). The only pre-existing
untracked entry was `_rewrite/docs/audits/`, preserved outside Git. No applicable
AGENTS.md was found. Original server/client sources and migrations001–074 are
unchanged. Written075 and its grants are not executed. Recorded044–075 remain
unapplied; next076. Full Phase A remains incomplete.

The ordinary native ready-primary RPS chain is **INTEGRATED_UNVERIFIED** within
the bounds below. This closes the missing normal Map producer/consumer/reward
half recorded in the preceding World handoff. Full RPS parity remains
**IMPLEMENTING** for BR, late replica completion and original-client acceptance;
full World bootstrap/source Map registration are still OPEN. There is no new
parallel game server, protocol or persistence framework.

## Actual authorities and packet profile

Original Map/World/client source lineage is
`c77f79017d7940c1500ba36a5dbf8bda6fefb67c`. `TVERSION=0x2918` is a source
protocol marker, not an identified binary. `Client/TClient/TClientType.h21–22`
defines ADD_RPS/ADD_RPS_RL; client senders and handlers are guarded by ADD_RPS.
Map's original CS RPS handlers explicitly return in BOW_COMPILE_MODE. BR is not
silently classified as excluded: its native RPS integration is still missing.
No client or original source is edited to accommodate new behavior.

The real backups were hashed again read-only:

| File | SHA256 |
|---|---|
| TGAME_RAGEZONE.bak | `2cff1635d523322d3de8ddafaeacc3335581dc338dca901239ac80288f51d259` |
| TGLOBAL_RAGEZONE.bak | `fca545bcb90bcc72aa4f3514a6177c2b88d2b0bf71bf847b0128f090a20877fb` |

Original functions:

- `Server/TMapSvr/CSHandler.cpp16959–17100`: OnCS_RPSSTART_REQ and
  OnCS_RPSGAME_REQ. Start carries type/inventory/slot, not an NPC ID. Its
  precedence is current type → actual type0-win definition → actual bag/item →
  template equality/count → UseItem1 → type/wins → ACK. There are no secure,
  dead, riding, trade, class, level, reuse or quest-use checks in this source path.
- `TMapSvr.cpp9520–9565::UseItem`: exact one-count decrement, UPDATE or DEL to
  self; it does not run generic ITEMUSE gameplay. `TMapSvr.cpp2134–2167` loads all
  thirteen RPS columns and skips absent/zero-count reward templates.
- `TMapSvr.cpp11542–11617::RPSReward/SelectNpcRPS`: selected current-row reward,
  two original constructor items, real SetDuration(FALSE)/SetItemAttr0, DWORD
  money split, server PACKAGE mail, then ResetRPS. The bGiveup parameter is
  unused. `TItem.cpp` constructor and SetDuration confirm actual raw zeros and
  template durability, rather than invented item metadata.
- `SSHandler.cpp18738–18820`: World result increments or resets the current
  local object and sends the three-byte round ACK; Control modifies known
  probability/cap fields only. Both normal and World branches look for the
  NEXT key after the result and reward/reset when it is absent.
- `TPlayer.cpp38–39/6288`: constructor/ResetRPS zero the counters. Their names
  do not occur in original LOADCHAR/RELEASEMAIN serialization. Existing local
  target objects retain their own counters, while a newly constructed target
  starts at0.
- `SSSender.cpp1730–1782`, `SSHandler.cpp10943–11160`, original
  `TSavePost`/`TSaveItemDirect`: server senderID0/actual NAME_OPERATOR, reverse
  attachment traversal, direct post/item writes, source PACKAGE gold>10 audit
  and asynchronous post notification. No player mail capacity/protection/fee
  gate belongs to this server reward path.
- `CSSender.cpp5977/5988` and unchanged client `CSSender.cpp2478/2489`,
  `CSHandler.cpp19018/19053`: exact3-byte start request,1-byte card request,
 13-byte start reply (result plus three DWORD purses) and3-byte round reply.
  Existing MW request/reply bodies remain11/10 bytes, with no new round nonce.

## Concrete modern paths and persistence

`handlers/dispatch.cpp` routes both CS requests into the existing
`OnNativeQuestReq`/`PostgreSQLMapService::ChangeQuest` transaction. It owns the
actual session gameplay lease and immutable snapshot, current primary account/
claim/checkpoint/inventory/quest/effect/purse authority, physical presence,
configured ordinary mode and uncertainty rules. No quest-use credit or generic
ITEMUSE ACK is introduced. Every output follows confirmed commit.

`services/postgresql_map_rps.cpp` supplies:

- `LoadNativeRps/CheckNativeRpsRelease`: constructor selects the actual pinned
  World publication and complete13-field chart in its existing read-only
  snapshot. New075 views bind it to current Map, character, actor, routing and
  server-message releases using verified metadata/backups. Typed BYTE values,
  signed SMALLINT→WORD and INT→DWORD bits, real missing reward templates and
  legitimate source zero values are distinguished. No SQL Server dependency,
  synthetic chart, default probability or empty-on-error fallback is added.
- `ChangeNativeRps`: exact source start results and item debit through the
  existing normalized/full-graph `DebitNativeItem`; constructor-local type/wins;
  actual Win32 rand15 modulo and raw threshold order. bProb_Lose0 produces the
  source255 sentinel without an RNG draw. No probability normalization occurs.
  Draw, loss, normal win, giveup, absent definitions, terminal reward, next-key
  lookup and BYTE wrap are real branches. All return paths retain their actual
  original ACK/silence distinction.
- The capped current row sends the original request with next BYTE win count to
  World AFTER Map commit. World uses the preceding native global limit path.
  `handlers/rps.cpp::OnWorldRpsPacket`, called by the existing World dispatcher,
  decodes the real result, waits for the actor gameplay lease and rechecks the
  exact open character/user/key/channel/connection/epoch/primary tuple. Its real
  ten-byte body returns through ChangeQuest for the final counter/mail mutation.
- `RewardNativeRps`: uses actual reward templates without quest class filtering
  or stack splitting; original raw constructor extensions and duration/durability
  come from existing native item creation. Original server-message IDs6/45/46
  are derived from SERVER_MESSAGE, including preserved raw bytes and BuildNetString
  length prefixes. A complete published chart proves an actually absent message,
  for which source GetSvrMsg returns NAME_NULL; a missing publication fails.
  No player purse is credited directly. Existing item allocator, mailbox lock,
  TPOSTTABLE/TITEMTABLE,061 TPOSTERRORTABLE and notification queue all participate
  in the same transaction as the RPS operation/core/full-graph checkpoint.
  HydrateNativePosts refreshes real graph-owned attachments; pending notification
  counters are consumed later by the existing ready-primary mail timer, once.
  The existing expiry helper accepts an optional observed event timestamp;
  RPS supplies its actual event clock after authority waits, so reward expiry
  cannot silently rebase to an earlier transaction start across midnight.
  Existing callers retain their original default transaction-clock path.
- `UpdateRpsChart`: the existing registered World transport invokes the native
  service, validates the entire body before mutation and applies known-key
  Control edits in received order under the cache mutex. Group is source-unused.
  Item/money/reward fields never change. Overrides remain RAM-only as in source.

All added production translation units are in existing TMapSvrAsio CMake.
No thread mutex spans an await. Cache reads copy a definition under a short
mutex; the worker owns all copies/raw items/SQL parameters through each operation.
Incoming World bodies are owned vectors during offload and suspension.

New075 adds nullable before_rps/after_rps/rps_context audit columns to the existing
quest operation ledger. Old receipts retain NULL; new RPS receipts record actual
local state, release, observed row and actual draw (NULL when source draws none).
These audit arrays are never loaded as saved RPS state. The existing operation
head fences stale checkpoint writes. No new saved player table or transfer field
is invented. New Map view and diagnostic-column probe grants are written only;
existing mail/item/sequence/receipt privileges are reused.

`domain/rps.h::SourceRpsState` belongs to CharSnapshot outside its full payload.
It survives payload demotion/checkpoint copies. Capture, transfer hydration and
graph nesting explicitly clear remote RPS state; promotion restores the actual
local replica object's counters, while clearing old pending authority. Fresh
admission/relogin/process recovery constructs zero state. This preserves source
object lifetime rather than transferring counters that the original never sends.

## Deliberate repairs and remaining boundaries

- One World decision may be pending per local ownership tenure. Further card or
  giveup requests cannot overwrite it or create multiple indistinguishable wins;
  duplicate/mismatched/unsolicited replies do not create a reward. This repairs
  the source overlap/old-response vulnerability. The source protocol still
  cannot distinguish a delayed duplicate that matches a later identical pending
  card. No universal replay protection is claimed.
- Lost/unknown Map or World COMMIT/output is never retried or refunded. A changed
  local authority suppresses client output. The existing uncertain-snapshot
  reservation rules prevent a stale logout overwrite. Original async mail becomes
  atomic native mail before result; notification remains a later ordinary event.
- Native replies require Ready primary authority. A late result on a demoted
  replica is currently dropped, unlike the original handler's permissive local
  object update. Pending authority is cleared on later promotion/new tenure;
  original local counters survive. Full late-replica completion/mail authority
  and handoff-during-output parity remain OPEN; do not simulate a successful win
  or refund a potentially committed World cap.
- Item/post creation follows the existing explicit native UTC calendar policy;
  original historical data is untouched. World cap-history source timezone
  remains its separate explicit074 contract. New attachment private SQL slots
  distinguish the two constructor-slot0 items for native uniqueness; POSTVIEW
  omits that slot from the source item descriptor, and carried placement later
  assigns real slots. Source write traversal is reversed as in SSSender.
- Equipment-slot entry-item consumption derives cached native getters without
  inventing equipment/stat/effect ACKs; unsupported rich statistic contexts still
  refuse atomically. RPS does not add unrelated secure/dead/riding/trade gates.
- Existing secure rand15 preserves source range/modulo bias, not the predictable
  original CRT sequence. Exact packet/UI/CRT/libm/calendar and runtime behavior
  remains unverified. Source BOW return is retained; BR RPS is still unavailable.
- Full source Map admission/channel bootstrap, remaining native World repositories,
  general combat and other whole-server inventory gaps remain open. No global
  IMPLEMENTATION_COMPLETE_UNVERIFIED status is justified.

## Deferred Phase B acceptance — no execution in this increment

| Scenario | Required evidence |
|---|---|
|075 upgrade and least-privilege startup | New views/nullable audit fields only; unchanged001–074, private archives and old receipts; missing/mismatched releases fail. No duplicate mail table/allocator or runtime SQL Server call. |
| Start failure precedence | Active type→INGAME1; missing chart→NODATA2; absent bag/slot/wrong item/zero count→NEEDITEM3. Each actual13-byte reply carries unchanged source purses; no debit or generic ITEMUSE. |
| Actual start on fresh and complete-graph actors | One count UPDATE or DEL before START0; exact graph/normalized item fingerprints, operation/core/checkpoint atomicity; no USEITEM quest credit. Test source type0 if present, without inventing an exclusion. |
| Win/draw/loss thresholds | Stored real draw and raw row reproduce source rand15 modulo, strict win then draw order; zero denominator produces255/no draw; all three cards map to original NPC choices. |
| Missing current/next/reward keys and BYTE255 wrap | Source silence/reset, exact next-key membership and legitimate skipped reward templates; no guessed terminal rows or percentage normalization. |
| Capped win across actual native Map and World | Real11-byte request names next count; real global cap batch commits; Map consumes10-byte reply once for the current tenure; no Map-side fake cap verdict. |
| Giveup/terminal win/draw | Giveup sends no round ACK; automatic terminal branch sends round ACK before eventual mail notification; exact current-row money and both constructor items; no direct purse credit or quest reward selection/filtering. |
| Reward mail durability | Zero/one/two actual rewards and zero/nonzero money; allocator IDs, source template duration/durability/extensions, reversed writes, private slots, raw text/prefixes, gold>10 diagnostic, post view/get/delete/return/expiry and native SQL restart recovery. |
| Concurrent reward/other mail/timer/handoff | One mailbox lock and native ownership; canonical graph attachments, source counters, no duplicate notification, stale logout overwrite, allocator collision or ownership leakage. |
| Control updates | Exact full body, source unused group, known/unknown/duplicate keys and zero count; no partial edits after truncation/tail; source-transient overrides reset per process lifetime. |
| Disconnect/relogin/transfer/return | New target/relogin starts0; old local object keeps counters; old pending results cannot write after epoch/connection replacement; document remaining late-replica divergence rather than marking accepted. |
| Overlap/replayed result | One pending native decision; mismatched/duplicate/unsolicited replies do nothing; identical later round ambiguity remains explicit. No compensation after lost global decision. |
| Fault on debit/second attachment/post/receipt/checkpoint/COMMIT/output | Transaction rollback or uncertain closure/reservation; no precommit ACK, partial mail or retry of unknown outcomes. Actual database and socket evidence required. |
| Unchanged ADD_RPS client | Exact executable/data/region/profile identities first; actual start, all cards, draw/loss/win/cap/giveup/terminal mail UI. Currently BLOCKED on missing executable/data/environment. |

Only source/interface/SQL-text/diff inspection is performed. No build, test,
SQL, import, migration, service, client, container, push or deployment ran.

Static review records236 unique capability IDs with13 fields per inventory row,
and64 database-map rows with6 fields each. Both new production translation units
are listed once in the existing CMake target. Numbered migration075 is unique;
076 is the next free number. Original Map/World/client/protocol trees and
migrations001–074 have no diff; whitespace checks are clean. These are structural
source checks, not compilation, SQL validation or functional verification.

Next implementation point: the full-bootstrap prerequisite in `next-steps.md`
(source MW_CONNECT_ACK topology/ordered bootstrap and remaining native World
repositories). For RPS specifically, first resolve original late-replica result
authority across main transfer using actual source role/lifetime behavior;
then BR mode contracts. Keep these as explicit inventory gaps rather than a
claim that a normal-primary handler completes every original server feature.
