# Native communication increment — Phase A, 2026-10-14

Baseline: local `main`, `c389976`. Existing `Server/TMapSvrAsio/` and
`Server/TWorldSvrAsio/` production paths are extended. Runtime paths below are
**INTEGRATED_UNVERIFIED**; migration069, importer and grants are
**IMPLEMENTED_UNVERIFIED**. This increment has no compilation evidence.

Only source, declarations, call sites, CMake registration, SQL text, client
consumers and diffs were reviewed. No build, test, implementation tool/import/extraction,
SQL/migration, service, client or container was run. Historical passing results
belong to their recorded revisions. Existing wire fixtures were adjusted only
by static review. Original sources/client/protocol libraries/backups and
migrations001–068 remain unchanged. Pre-existing `_rewrite/docs/audits/` is outside
this work. Changes and commits stay local on `main`.

[Detailed source audit and implementation handoffs](analysis.md) records exact
original line references and unclosed contracts. [Map integration review](map-communication.md)
records its packet/authority boundaries and deferred checks. The full rewrite remains
incomplete, including ordinary implementation work independent of missing inputs.

## Connected source-to-runtime paths

| Original responsibility | Original evidence | Actual modern implementation |
| --- | --- | --- |
| Local NEAR and Map fallback chat, whisper and World fanout | Map CSHandler5206–5526; TPlayer6294–6408; TMap770–807; CSSender2621–2640; client TRelaySender72–102/TRelayHandler87–190 | Actual CS_CHAT_REQ dispatcher → handlers/communication.cpp → native current claim/protection/ban/operator authority → local primary/replica delivery or MW_CHAT_ACK → World handlers_chat.cpp → MW_CHAT_REQ → original CS_CHAT_ACK |
| Recipient protected CHAT1 and re-invite INVITE2 | TPlayer5472–5480; Map SSHandler TMSINVITEASK | AuthorizeCommunication reads TPROTECTEDTABLE under recipient account/owner/claim fences, before delivery or ASK response; cache alone does not authorize a committed protection change |
| Source character operator catalog | Map DBAccess2854–2864/bootstrap3866; World DBAccess605–615/bootstrap1224–1231; TOPERATORTABLE; pinned exact_count0 | New069 native table and checksum-gated world_compat.operator_release; offline import_world_operators.py; World bootstrap/relay list and World/Map native authorization |
| Operator-name whisper | World SSHandler5463–5512; source message6/7 | handlers_chat.cpp compares exact published message6 bytes, visits current online operator IDs of the same raw country, echoes source identity or sender0/type7/no-receiver NetString |
| Spam/spoof ban producer and enforcement | Map TPlayer6336–6408/CSHandler; World AddChatBan/CheckChatBan5605–5641; SSHandler13098–13146; Map SSHandler2222–2254 | Current primary CS gates → original MW_CHATBAN_ACK → fenced PostgreSQLChatRepository::Ban → committed audit/deadline → current target projection and GM reply; entry restoration also reads native authority |
| Control chat ban | World SSHandler73–108; Control Handler1150–1238/1394–1438 | Registered Control CT_CHATBAN_REQ dispatch → native replace-deadline commit → original CT_CHATBAN_ACK correlation and target Map enforcement |
| TMS SEND/INVITE/OUT and solo re-invite | Map CSHandler8562–8634; World TMS handlers; client TMessengerCommander765–838/CSHandler | Actual three CS requests and five MW response families; existing World TmsRegistry with coroutine serialization and generation-bound members/pending expectations; original CS suffixes |
| Targeted operator notice | Happy Sender299/CharMsg37–62; Control Handler1062–1114; World SSHandler111–132; Map SSHandler15524–15541 | Existing authenticated Control routing → CT_CHARMSG_ACK → current native recipient Map → MW_CHARMSG_REQ → primary client CS_CHAT_ACK type7/sender0/published message6/NetString(empty,full text) |
| Event text announcement | Control Handler1786–1857; World event-message sender; Map SSHandler16026–16048 | Existing Control registry start/end text and authority → CT_EVENTMSG_REQ → registered Control/current typed Map fanout → MW_EVENTMSG_REQ → ready-primary CS_CHAT_ACK groupWORLD3/sender0/message6/NetString(empty,full text) |

Map implementation resides in `handlers/communication.cpp`,
`services/postgresql_map_communication.cpp`, the existing dispatcher/World-link
router and player service. World implementation resides in the existing chat/TMS/
event/entry/relay handlers, `services/postgresql_chat_repository.*` and native
friend authority. Both new source files are included in the existing targets.
No replacement server, separate mail backend or new client packet is introduced.

## Protocol and gameplay boundaries

- Original CS_CHAT_REQ/ACK remain0x52F4/0x52F5. CS strings are INT32-length raw
  bytes; chat type and audience group remain distinct BYTE fields. Map accepts
  the client's structurally valid NEAR WORD-count/DWORD-list tail, then selects
  its own surrounding3×3 source64-unit cells. It applies the sender-direction
  country/special-unit3 rule; incoming World chat applies recipient direction.
  Meeting maps1100..1105 waive the original country gate.
- Source refuses client WORLD3/OPERATOR7, identifies operators as type7, preserves
  SHOW9 name/raw-length gates, and requests30000-minute spoof bans. CheckChat uses
  low-WORD hexadecimal size, DWORD ten-second windows and regular-message timing;
  its result is ignored by the source caller, so a triggering message can precede
  the asynchronous five-minute ban. World preserves this by applying no second
  ban drop to an already accepted Map chat frame. Subsequent client requests
  read the committed native ban at Map. Flood observations remain process-local and
  are absent from persistence/transfer payloads.
- The original `strTalk.Left(1024)`/`strMsg.Left(1024)` calls discard their return
  value. Full strings within transport bounds are preserved. Active bans emit
  CS_SYSTEMMSG_ACK with BYTE10 and DWORD remaining seconds only. Native character
  names now convert UTF8 storage losslessly into the explicit client ANSI profile.
- Source client `RelaySession()` prefers a connected Relay during ordinary play;
  BOW/BR uses Map, and ordinary play falls back to Map without Relay. This increment
  implements the Map/World path. No original Relay server implementation is present
  in the repository, and the connected Relay path remains **OPEN**.
- Ordinary whisper preserves ID lookup then requested-name fallback and the two
  distinct echo-name branches. Every delivered chat, echo and administrative
  sender0 passes recipient protectedCHAT1. TMS has no chat-ban/CheckChat gate.
- TMS uses original0x534A/4C/50 requests and unchanged MW/CS bodies. INVITEASK ACK
  physically originates on the target Map although its first identity is the
  initiator. Pending expectations bind both current keys and message, consume one
  matching response, and permit multiple equal legitimate requests. Members and
  emitted rosters use unsigned-ID order. Source one-to-one re-pair shortcut, solo
  no-receiver text and OUT-before-removal ordering are preserved.
- Targeted notices use typeOPERATOR7; event text uses groupWORLD3. Both use sender0,
  published raw message6 bytes and the original uppercase eight-hex length envelope
  for NetString(empty,text). BYTE eventID/msgType stay wire metadata; this consumer
  changes no event gameplay/reward state. The complete output must still fit the
  original INT32 strings and transport frame, without silent1024 truncation.

## Native authority, races and deliberate repairs

World communication operations have an independent coroutine gate covering SQL
awaits, pending conferences, projections, fanout and logout. Exact registered
open typed Map peers, current CharRegistry objects, session keys, ready connection
rows, Map generations and native World/account/current-user ownership are checked.
No communication operation acquires the friend coroutine gate or calls CloseChar
while holding its own gate. Successful friend entry may acquire communication
only after earlier failure/close branches; SQL mutexes are released before offload
completion. Source country/actor snapshots are refreshed after invitation awaits.

Map asynchronous responses use projection leases and full process/connection/
authority-epoch identities. CONREADY takes the primary admission lease before
reading its snapshot and setting Ready, serializing ban/soulmate projections with
admission. Local ready replicas can receive source local chat through their own
read-only claim checks and a live primary owner; they cannot originate these
requests or receive primary-only World/TMS/notice fanout. GM success validates the
committed ban audit against its exact issuer generation and current World token.

Explicit changes from source:

1. Bans persist across World restart in069. Original bans lived only in
   `m_mapBanChar`; no historical ban SQL table is asserted or imported. Raw-name
   identity and positive MW stacking are retained. MW zero clears0; Control uses
   replacement `now + minutes*60`, including Control zero meaning now.
2. Ban/audit commit precedes caches/enforcement/results. Unknown commit/SQL failure
   stops native World for reconciliation without automatic write replay or cached
   success. Ban target uses canonical resolved native/raw name; case-varied request
   spelling cannot create an unenforced different entry.
3. Original accepted TMS re-invite incorrectly mixed target ID with each recipient
   key. Native sends actual recipient ID/key and target as inviter, preserving
   layout while allowing both old/new members to receive the roster. BYTE room
   count is capped255 and pending target queue bounded255 rather than overflowing.
4. Stale owners, unsolicited/replayed ASK responses, Map-role CT requests and old
   logout generations are rejected. Native recipient protection comes from the
   committed SQL state, not a potentially lagging cache.
5. Existing Control event-message routing requires GMLevel3 and honors the selected
   server. Source had no per-action authority check and `bSvrID == bSvrID`; these
   earlier deliberate repairs are retained, not mislabeled exact source parity.
6. Missing catalog/ANSI/provenance dependencies fail closed. Native startup requires
   a verified operator release even when its exact source catalog is empty.

Static API review caught and corrected a client send call that treated
`AsioSession::SendPacket` as bool; its actual return is `awaitable<void>`.
`WorldClient::SendPacket` remains bool. A local enqueue attempt is not a client
delivery receipt and can never trigger a duplicate World fallback. Complete
chat/notice CS body bounds and the eight-byte SS relay envelope are checked;
known frame overflow logs and drops without trimming, retry or closing a valid
recipient. These declaration/call corrections have not been compiled.

## Migration/import/grant boundary

Written chain001–069; new044–069 remain unapplied; next unused number070.
Only069 is introduced by this increment. It holds source operator projection,
verified import receipts, raw-name bans and current-generation mutation audit.
`deploy/sql/communication-runtime-grants.sql` is written only. Existing social,
soulmate, native Map and059 server-message grants/publication remain prerequisites.

The operator importer requires the complete pinned historical manifest for both
backup fingerprints, verified archival run/checkpoint, fresh count/value hashes,
explicit injective character mapping, offline Map/World ownership barriers and
locks. The public pinned profile proves TOPERATORTABLE has zero rows; any nonempty
extract is rejected. Explicit `{}` mapping can publish that evidenced empty
projection. A checksum/count view binds the actual native rows to the receipt.
There is no implicit empty fallback, GM-name heuristic, generated character or
Control credential substitution. Source TOperatorReset `%GM%` is manual historical
administration; native mutable operator provisioning/reset remains **OPEN**.
Private credentials, extracts/manifests and identity maps stay outside Git.

## Required OPEN implementation and missing inputs

- Connected Relay transport, RW_CHATBAN_ACK enforcement0x99A9 and complete Control
  ban response-loss/list/recovery correlation. No imaginary Relay state is seeded.
- Native Map guild/party/corps/tactics topology and target producers; full guild/
  war/event/rank/item/service bootstrap still includes legacy SOCI/SpCall paths.
  Source World group receivers exist, but Map cannot manufacture membership IDs.
- BR team-local whisper/party and BOW cross-country peace-status authority;
  implemented BOW same-country local delivery does not complete either game mode.
- Full operator administration/reset and character DWORD high-bit/collation parity.
  Existing generic native claim bindings retain their documented high-bit limit;
  ASCII case folding is not proof of arbitrary original locale collation.
- Full combat/AI/rewards/quest producers, economy/trade/crafting/storage, companions,
  modes/events/ranks/timers/internal messages/build variants/admin tools remain in
  the overall inventory. `ALL.RESPONSIBILITIES` is still NOT_ANALYZED; no percentage
  or complete implementation gate is asserted.
- Full private pinned historical manifests, missing Patch pre-version SQL and real
  unchanged client executable/assets remain missing source/acceptance inputs.

## Deferred Phase B acceptance plan

After the complete Phase A implementation gate, perform in isolated local clones:

1. Build all six C++20 native PostgreSQL targets in Debug and Release, including
   declaration/link/CMake checks. Compile aligned wire fixtures before running
   them. Existing event fixtures still contain Map-role CT senders and need actual
   registered Control/typed Map setup; historical passes do not cover this code.
2. Fresh001–069 and043→069 upgrades, least-privilege roles, exact complete archive/
   empty operator publication, hash drift, forged nonempty extract, wrong mapping,
   active/dead-owner barriers and required dependency/ANSI failures. No execution
   occurred here; never run these against originals/private production data.
3. Byte comparisons against source writers and independent client consumers:
   local NEAR cells/channels/meeting/special-unit direction, full1500-byte strings,
   operator type/group, remote triggering chat after an automatic ban commit,
   whisper echo branches, no-operator message7, protected
   CHAT1 and INVITE2, BOW/BR bounded paths and unimplemented source branches.
4. Two-Map/World current/stale process and actor generations; loaded→ready ban
   restoration, concurrent protection commit, movement/handoff/logout/admission,
   GM/automatic/Control distinction, stack/clear/replace/expiry and restart.
   Inject commit-result loss: no second write or fabricated success, reconcile
   audited deadline before enabling traffic.
5. TMS two members, multiple equal pending asks, unsolicited/duplicate ACK, next
   legitimate equal request, different target Map, pending logout/key rollover,
   faction change across awaits, sorted unsigned roster,255/256 boundary, OUT
   order and one-to-one re-pair. Verify the recipient-address repair with client.
6. Targeted/event notices through actual Control registry routing, stale Control
   replacement, typed Map fanout, primary-only delivery, blocked sender0, message6
   non-ASCII bytes, full1500-byte text and near-frame limits; verify BYTE event
   fields cause no additional gameplay effects. Test missing source publication.
7. Only after these checks and the broader native bootstrap implementation, run
   the unchanged original client with real assets in the local cluster. Check
   both Relay availability profiles and all documented mode/collation differences.

No full functionality or real-client compatibility has been verified.
