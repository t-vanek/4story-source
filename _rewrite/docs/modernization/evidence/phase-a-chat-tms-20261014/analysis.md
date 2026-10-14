# Phase A: original chat and TMS source review, 2026-10-14

Baseline: c389976. This file records static source observations and concrete
implementation handoffs. No build, test, service, SQL query, extraction, migration
or runtime verification has been performed in this phase. The analyst owns only
this new file; production code and shared status documents belong to the other
agents. The full modernization scope is retained.

## Transport and original wire contract

The original client chooses `RelaySession()` in
`Client/TClient/TClientGame.cpp:26621–26634`: BOW/BR uses the Map session;
otherwise a present Relay session wins, with Map as fallback. Thus completing
Map/World chat proves an implemented source Map path, not the normal connected
Relay path. No original Relay server implementation was found in the repository
source inventory. This dependency must remain open. TMS is independent:
`TMessengerCommander.cpp:765–774,801–810,826–838` uses `GetSession()`.

`TRelaySender.cpp:72–102` writes CS_CHAT_REQ as STRING sender, BYTE group,
DWORD target, STRING target-name, STRING talk. CHAT_NEAR additionally appends
WORD recipient-count and that many DWORD IDs. Original Map does not consume
this tail and does not trust these IDs: it chooses `GetNeighbor()` itself.
The native parser must accept the actual client tail while checking its shape.

Source opcodes use CS_MAP=0x5280 and MW_BASE=0x9001
(`ProtocolBase.h:7,10`), not inferred modern aliases:

| Message | Source value |
| --- | --- |
| CS_CHAT_REQ / ACK | 0x52F4 / 0x52F5 |
| MW_CHAT_REQ / ACK | 0x903E / 0x903F |
| CS_TMSSEND_REQ / TMSRECV_ACK | 0x534A / 0x534B |
| CS_TMSINVITE_REQ / ACK | 0x534C / 0x534D |
| CS_TMSOUT_REQ / ACK | 0x5350 / 0x5351 |
| MW_TMSSEND_ACK / TMSRECV_REQ | 0x9077 / 0x9078 |
| MW_TMSINVITE_REQ / ACK | 0x9079 / 0x907A |
| MW_TMSOUT_REQ / ACK | 0x907B / 0x907C |
| MW_TMSINVITEASK_REQ / ACK | 0x90CD / 0x90CE |

Definitions: `CSProtocol.h:1150–1155,1687–1712` and
`MWProtocol.h:184–185,242–247,328–329`.

Map→World CHAT_ACK is BYTE channel, DWORD sender-id/key, STRING sender-name,
BYTE type/group, DWORD target, STRING target-name, STRING talk
(`Server/TMapSvr/SSSender.cpp:3333–3356`). World→Map CHAT_REQ is DWORD recipient
id/key, BYTE channel, DWORD sender-id, STRING sender-name, BYTE country/aid,
BYTE type/group, DWORD target, STRING talk
(`Server/TWorldSvr/SSSender.cpp:1641–1669`). Client CHAT_ACK is BYTE group,
DWORD sender-id, STRING display-name, STRING talk
(`TMapSvr/CSSender.cpp:2621–2640`, client `TRelayHandler.cpp:87–190`). Strings
carry original source bytes; localized NetString bodies retain their source
eight hexadecimal length digits and raw payload.
Every STRING has a signed INT32 byte-length prefix, followed by its raw bytes
(`Lib/Own/TNetLib/TNetLib/Packet.cpp:518–548`, native wire_codec29–36,64–78).
NetString's two four-digit hexadecimal lengths are separate from these packet
string prefixes. Native receive/send enforces the existing strict total frame
bound, not just a message-body length (`asio_session.cpp:249,355–357`).

## Original Map chat gates and recipients

`CSHandler.cpp:5206–5526` first requires a map and primary authority. It rejects
client groups WORLD3 and OPERATOR7. Group/type are different packet fields,
but the source marks an operator's type as CHAT_OPERATOR7, not CHAT_OP1 from the
separate CHAT_TYPE enum (`NetCode.h:2009–2030`). `strTalk.Left(1024)` has no
assignment and does not truncate the original message. The special hardcoded
`localservicetest` call is modified-source diagnostic behavior, not a production
authorization mechanism.

Raw sender-name mismatch requests an automatic 30000-minute ban and returns.
SHOW9 requires the sender's own raw name inside talk and target equal to the
raw talk length. Other groups call CheckChat. The caller ignores CheckChat's
boolean return: triggering an asynchronous five-minute ban need not stop the
current message before the World ban response arrives. A subsequent active
ban (`m_timeCurrent < ban_time`) emits SYSTEMMSG(SM_CHAT_BAN) with remaining
seconds and then returns. Do not describe an immediate anti-spam drop as exact
source parity.

`TPlayer.cpp:6336–6408` parses the first eight talk characters as hexadecimal
and uses its low WORD as new-size. Within ten seconds, accumulated size above100
requests the five-minute ban and resets the size/tick. Otherwise it records
message ticks, clears a sequence with interarrival differences above1000ms, and
requests a ban after more than eight similarly spaced entries. This is a
process-local producer; there is no durable source spam log contract here.

Ordinary-profile NEAR uses the surrounding3×3 cells of size64
(`TMap.cpp:770–807`, `NetCode.h:44`), not Euclidean radius or client target IDs. Meeting maps
1100..1105 waive the country check (`TMapSvr.cpp:11727`, NetCode139–140).
WHISPER resolves local ID first, then source case-insensitive name; on success
it delivers to the target and echoes the target name to sender. Other groups
relay through World. PARTY replaces target with own party ID, GUILD with own
guild ID, TACTICS with own effective guild; FORCE uses World corps state.

Country values are D0/C1/B2/N3/PEACE4 (`NetCode.h:1090–1098`).
`TPlayer.cpp:6294–6332` permits either peace country or incoming N3 immediately;
PARTY/GUILD/TACTICS/FORCE always pass. NEAR additionally permits B-country
interaction in source unit3. Other permitted groups compare WarCountry, defined
as aid when aid!=N3, else country (`TObjBase.cpp:4962`,
`TWorldSvr.cpp:7714`, `TMapType.h:149`).
The local NEAR gate evaluates the sender's position/country function, whereas
the incoming MW relay evaluates the recipient's function. The special unit3
position must follow that direction. Local NEAR keeps group1 even for an operator;
targeted MW delivery uses group for sender echo and type for other recipients.

Every CHAT_ACK, including local, relay, self echo and operator/system sender0,
goes through recipient `CheckProtected(sender-id,PROTECTED_CHAT1)` in
CSSender2621–2640. The protected map rejects when the stored option includes the
bit (`TPlayer.cpp:5472–5480`). This final delivery rule is required even where
upstream routing already checked an actor. TMS does not inherit it generally.

BOW and BR compile-time branches are distinct source profiles, not ordinary
chat with one extra gate. BR permits team-local WHISPER/PARTY and discards the
other listed groups. BOW local NEAR/GUILD/TACTICS/SHOW/MAP fans out by equal raw
country or peace BOW status; PARTY relays, WHISPER uses CanTalk, FORCE/WORLD is
silent. Native profile availability and actual BOW/BR producers remain open.

## World routing and operator authority

`SSHandler.cpp:5248–5600` routes TACTICS to online guild members plus tactics
members, GUILD to online guild members, PARTY to a WORD party ID, FORCE to the
sender's corps parties, and MAP/SHOW/WORLD to every Map peer. Most original
membership checks are delegated to Map; the native World must not trust a
client-selected target or stale Map sender. At baseline the existing native handlers
lacked sender-key/current-main/peer-generation gates and broad typed peer filtering.
Their apparent routing coverage is not evidence of native authority.

Operator whisper compares raw requested target-name exactly to published
NAME_OPERATOR6. It visits online operator character IDs whose raw country equals
the sender's country, not effective WarCountry (`SSHandler.cpp:5463–5512`). It
then echoes to the sender with operator display-name. With no delivered operator,
the echo has sender-id0, type OPERATOR7 and
NetString(empty,MSG_CHAR_LOGOUT7). Ordinary whisper resolves ID first then name,
requires equal WarCountry unless either raw country is PEACE, and echoes only
after target Map routing succeeds. Name fallback echo uses requested name; ID
lookup echo uses the target's actual name.

TOPERATORTABLE is TGAME dbo with one NOTNULL INT primary-key dwOperatorID
(`TGAME_RAGEZONE.tables.sql:1675–1678`, schema csv1126, archive002:3009–3016).
The reader interprets this as DWORD (`World DBAccess.h:605–615`,
`TWorldSvr.cpp:1224–1231`; Map DBAccess2854–2864 and bootstrap3866).
Pinned backup profile `data-profiles.json:1822–1825` gives exact_count0.
The actual data hash/import manifest was not observed or extracted. Mapping
`database/postgresql/mapping.json:16294` explicitly disallows reference import;
use the complete pinned historical manifest with verified checkpoint/owner
barrier. Empty imported data is valid source evidence and must not turn into
invented GM IDs or an arbitrary configurable bypass.

`TOperatorReset.sql:6–7` deletes IDs and inserts character IDs whose names match
`%GM%`; this is a historical administrative procedure, not a native bootstrap
heuristic. This character authority differs from Control login credentials in
`app_control.operators`. Neither may impersonate the other. This independent
bootstrap handoff became the written069 exact native TOPERATORTABLE publication,
World+Map loading, actual RW operator vector and source operator-whisper branch
described in the continuation review below.
Mutable operator administration needs its own source-defined authority later.

## Chat bans: exact source persistence boundary

No original SQL/table for chat bans was found in reviewed World/Map/Control
handlers, DBAccess/procs or backup table schema. World AddChatBan and
CheckChatBan (`TWorldSvr.cpp:5605–5641`) use m_mapBanChar by raw name. It restores
an active ban on char load (`SSHandler.cpp:1479`) and expires an old entry lazily.
This survives character relogin inside the process, not a World restart.

MW_CHATBAN_ACK (`SSHandler.cpp:13098–13146`) finds an online target by name,
stacks positive WORD minutes onto an active ban or starts from now; zero clears.
It sends Map enforcement, Relay RW_CHATBAN_ACK and optional GM result. CT ban
(`SSHandler.cpp:73–108`) instead sets now+minutes, does not stack, and acknowledges
the Control request. Source Control stores request/list state in m_mapBanInfo
(`Handler.cpp:1150–1238,1394–1438`); that is not World ban SQL authority.

Map CS_CHATBAN requires map+primary and character operator status before decode
(`CSHandler.cpp:16518–16540`); a non-operator receives result1. Source MW ban
enforcement uppercases the target name, checks target primary, writes local ban
time and responds to GM (`SSHandler.cpp:2222–2254`). Its GM lookup ignores key
and may dereference missing character; native implementation must retain the
wire shape while adding exact current actor/session guards. New PG ban durability
would be a deliberate extension, not a copy of a fictional source ban table.
The active-ban notification is CS_SYSTEMMSG_ACK with BYTE SM_CHAT_BAN10 and
only DWORD remaining-seconds (`NetCode.h:2277–2289`, CSSender5056–5077,5147–5150).
There are no generic local-id/string/hour tails. No source CHATBAN_MAXMIN limit
was found in the CS handler despite that result enum existing.

## TMS Map/client contract and World state transitions

Map CS SEND/INVITE/OUT (`CSHandler.cpp:8562–8634`) gates only map+primary, without
chat-ban/CheckChat. SEND is DWORD tms+STRING message; INVITE DWORD tms+BYTE count
then DWORD targets; OUT DWORD tms. Map prefixes char-id/key to the corresponding
MW request. Source unchanged client producers/consumers are CSSender742–780 and
CSHandler9551–9627.

MW RECV is recipient-id/key+DWORD tms+STRING sender/message. INVITEASK is
recipient-id/key+inviter-id/key+DWORD tms+STRING message; Map checks recipient
CheckProtected(inviter,PROTECTED_INVITE2) and replies with inviter first,
recipient second, BYTE result, tms and message (`SSHandler.cpp:11962–12045`).
MW INVITE is recipient-id/key+DWORD inviter+DWORD tms+BYTE count and rows
(DWORD id, STRING name, BYTE class, BYTE level); Map strips only recipient-id/key
and forwards the complete roster to client. OUT strips id/key and forwards
DWORD tms+STRING departed-name. Exact senders: World2173–2254, Map3645–3673.

World initial INVITE requires requester tms membership when expanding, dedupes
online target IDs and requires equal WarCountry without peace exception. A
single target's solo room whose last-member equals requester-name is reused;
otherwise it creates a new room with requester and targets and publishes roster
(`SSHandler.cpp:7576–7697`). Initial invite does not use protected INVITE checks.
Solo SEND tries to reconnect the last departed member via INVITEASK; absent or
refused target produces localized TMS_NORECEIVER5 NetString to current members
(`7431–7574`). OUT notifies all members including leaver, then removes leaver;
logout removes first, then notifies survivors (`7700–7748`,
`TWorldSvr.cpp:2845–2880`). Source conferences are process-local, not SQL rows.

Source INVITEASK success contains an addressing defect at SSHandler7547:
recipient header is newly added target-id plus each current member's key;
inviter field is that current member's ID. Baseline native handlers_tms copied
it, so Map's exact FindPlayer(id,key) dropped the original member's roster. This
handoff was repaired: recipient now uses its own id/key and inviter is the added
target, with the original packet layout. The source defect is documented instead
of being claimed as functional compatibility.

The source roster map emits rows in unsigned DWORD-ID order. Baseline native
vector insertion order did not reproduce that order; emitted rows are now sorted.
Source and baseline native BYTE(room-size) followed by every row overflow at256
members, including initial255-target invite plus requester. An explicit at-most
255 member/serialization bound is a source packet-overflow repair, not a
source-derived smaller gameplay limit. These issues were handed to the owner.

## Native stale/session and concurrency requirements

Baseline chat/TMS FindMapPeer matched only wid low byte. Native current actor
gates must require exact Map wid/type, exact registered live peer object,
same current char object/key, primary connection ready+valid, and no closing or
logout. PostgreSQL operations also need the live World/Map owner and actual
current primary claim, including unsigned INT bits, instead of cache-only trust.
The existing soulmate CurrentWriter and friend authority transaction are useful
patterns, not automatic proof for chat/TMS.

Baseline TMS membership/back-links mutated under separate short mutexes while
await fanout yielded. OUT removed after fanout and initial INVITE could add
concurrently; logout shared no operation gate. Source dispatcher serialization
was missing. The written gate now covers room/backlink state
transitions and logout cleanup while avoiding recursive acquisition through
CloseChar. Native CloseChar654–720 marks closing, awaits DEL notifications,
removes char, then awaits friend/soulmate before TMS cleanup. A room containing
only char IDs can therefore temporarily resolve an old participant to a new
TChar/key. Every delivery needs generation-aware membership plus current
back-link/current-primary checks; a current route alone cannot authorize an old
conference for a replacement login. Packet-await revalidation and counterpart
departure/rejoin remain later acceptance cases.
INVITEASK_ACK comes from the target's Map but puts the inviter's pair first;
authorizing only the first pair trusts the wrong peer. Verify the responding
target generation and track one-use pending asks bound to room object and both
actor generations/message. Equal new legitimate SEND invocations can require
multiple asks; do not suppress those by packet bytes alone. Logout/room removal
must retire expectations before a successor can reuse a route.

## Static review of the written communication continuation

The newly written069 operator-release view verifies the archival checkpoint and
current native ID count/checksum, preserving unsigned ID order. Runtime grants
allow only SELECT of operator IDs; World may write the explicit modern ban
ledger and mutation history. The inspected World reader uses a repeatable owner
snapshot. This resolves a preliminary concern that a truncated operator table
could be mistaken for a verified empty source collection: the release view
itself excludes that state.

`import_world_operators.py` requires the complete historical manifest, both
pinned backup fingerprints, verified source manifest/checkpoint and fresh
archive value hash, explicit injective source→native character mapping, existing
nondeleted native characters and an offline owner barrier. It does not create
GM characters or use the reset procedure as a heuristic. The reused mapping
helper limits native target IDs to1..INT_MAX; full high-bit native identity
admission/service parity remains open. At the inspected stage it did not compare
source count to the backup's known exact_count0: load_snapshot verifies only
manifest-declared counts. This missing provenance gate was handed to the owner
and repaired: both importer and069 now require the pinned source count0.
No importer was executed. Future mutable game-operator administration remains
separate from this exact empty historical publication.

Map TMS now has actual dispatch, source prefix/suffix packets, structural count/
tail parsing, native primary SQL authorization, projection lease for World
responses and protected INVITE2 reply. It explicitly does not add chat-ban gates.
Written World handlers are now wired to native actor authorization and their
independent communication gate. These observations establish inspected production
registrations, not compilation or execution. Original non-ASCII names now need
lossless source encoding on Map load, and both Map/World byte profiles must agree
before claiming client parity.

Final static API review found that Map DeliverChat had treated the client's
`AsioSession::SendPacket` as a boolean delivery result. Its actual declaration
and implementation return `awaitable<void>` (`asio_session.h:127`,
`asio_session.cpp:333–391`); the separate IWorldClient API returns
`awaitable<bool>` (`world_client.h:41–45`). The owner repaired the client call
without changing the valid World transport result check. Current DeliverChat
holds the recipient operation, rechecks native authority/current identity/open
state after its DB await and uses explicit ChatRoute outcomes
(`handlers/communication.cpp:130–165`). Unavailable before an enqueue attempt
can permit the source name/World fallback; a protected recipient is handled and
still permits the independent sender echo. After an attempted client enqueue,
closed/replaced identity produces Stopped, which ends the local whisper branch
instead of replaying it through World (`342–345`). The transport itself checks
open state on its strand and quietly drops a closed/cancelled queue; void is
never interpreted as proof of client delivery. Queue saturation, close and
takeover remain deferred acceptance cases.

All ChatAck outputs now check their complete body against the existing strict
client frame bound before enqueue, including an ID whisper echo whose canonical
target name can be longer than the incoming requested name (`155–158`). With
the original 16-byte client header, body must be less than 65519 bytes. The Map
relay helper separately rejects a known original 8-byte SS envelope overflow
above 65527 body bytes by log/drop, without closing the valid player or replaying
the request (`69–81`); an actual boolean World transport failure still closes
the player. These are source/API inspections, not a compilation claim.

The written TMS continuation adds exact member-key/back-link checks, per-target
vectors of one-use pending asks, responding-target Map authorization, sorted
unsigned roster rows and an explicit255-member cap. It repairs the original
mixed recipient/inviter addressing. Initial invite now resamples sender/target
country and aid after all DB awaits: the country update producer in
handlers_char_base136/145 otherwise could invalidate the source WarCountry gate.
Original multiple legitimate equal-message SENDs can retain distinct asks;
source conferences remain process-local. Admission replacement, same-key Map
handoff and simultaneous metadata changes still need Phase B acceptance.

The written World ban deadline and mutation audit commit together under sorted
actor account/current-primary locks. MW stacks or clears; registered Control
replaces the deadline. The audit captures the actual Map token/connection/epoch
for Map-origin mutations, with explicit NULL Map metadata for Control. The
runtime grant now gives Map SELECT of this receipt while permitting only World
deadline/audit writes. A write exception stops native World for reconciliation
instead of automatically replaying an uncertain ban. New durability is explicit
modernization beyond the original memory map.

Map's first successful-GM confirmation incorrectly compared the GM's character
name with the target name, suppressing success for another target. This was
reported and repaired: target deadline/name is checked independently and the
GM response requires its current claim/operator role plus matching mutation
owner/token/connection/epoch. Local target projection remains separately fenced.
Loaded/admitted primary ban projection is supported so initial source enforcement
does not depend on one pre-ready ACK being accepted. Ordinary chat independently
reads the durable deadline. World entry reloads that deadline with a loading
claim and checks the live typed Map after the DB await.

The final World review also removed an extra active-ban drop in OnChatAck.
Original Map CheckChat ignores its false return and can send the automatic BAN
before the triggering CHAT. World can therefore commit that ban before reading
the same stream's already-authorized CHAT; original World still relays this
message. The written World handler now keeps actor/session/operator checks and
documents that source timing instead of applying the deadline twice
(`handlers/handlers_chat.cpp:84–103`). Native Map reads the durable deadline at
the next client request. Acceptance must distinguish this remote triggering
message from later requests blocked at Map, while retaining source stacking,
unban and active-ban notification behavior. No packet stream was executed.

The World repository's startup constructor now probes the newly introduced
mutation audit columns, and World runtime grants include SELECT of those audit
receipts for startup/reconciliation. This is a static schema/grant alignment
check; migration execution and installed database privileges remain unverified.

World ordinary whisper initially echoed the requested name even after ID lookup.
The implementation owner restored the actual name for the ID branch, requested
name for fallback, and fallback when an ID route is unavailable. Operator whisper
uses published name6, same raw country and source logout7 envelope; normal Relay
ban forwarding remains open. Original RW_CHATBAN_ACK is0x99A9 with STRING name
and INT64 deadline (`RWSender.cpp:199–207`, SSHandler13138). There is no current
native Relay authority slot to complete that producer safely.

No concrete new lock cycle was found in the inspected paths. Entry acquires
friend gate then communication gate; communication handlers do not acquire the
friend gate or call CloseChar while holding their gate. Repository thread mutexes
are released before DB offloads return. Logout cleanup takes only communication
gate. This is a static lock-order conclusion, not executed deadlock proof.

Initial chat/TMS fixtures used untyped peer IDs0x0042/0x0043 instead of Map
0x0400|sid and did not complete ready+valid primary state; the TMS acceptance
fixture checked member IDs but not recipient-id/key/inviter and missed the
addressing defect. The owner repaired typed IDs/readiness seeds and added the
actual recipient/key/inviter assertions. The original ban fixture did not even
register its issuing GM. It now explicitly registers a ready actor and names
its operator cache synthetic and non-PG. Native production always supplies the
SQL repository and cannot use that test adapter after a SQL failure. These
changes are statically inspected fixtures, never test execution evidence.

## Addressed administrative notice: source and written continuation

The independently mapped producer is original Happy→Control→World→Map→client
notice. Happy `CharMsg.cpp:37–62` trims target spaces and requires nonempty
target/message; `Sender.cpp:299–310` writes two STRINGs to CT_CHARMSG_REQ0x933D.
Source Control `Handler.cpp:1062–1114` requires MANAGER_GMLEVEL2, prefers each
group's connected Relay, and falls back to that group's World. Native Control's
existing handlers_admin279–313 follows this grouping/role path and audits the
request; normal connected-Relay service remains an independent open dependency.

Control→World CT_CHARMSG_ACK0x933E carries STRING target-name/plain-message.
World `SSHandler.cpp:111–132` resolves the online target and sends identical
strings to its main Map as MW_CHARMSG_REQ0x911F (SSSender3008–3020).
`strMsg.Left(ONE_KBYTE)` discards its return: baseline native resize1024 was
a source mismatch, now removed. There is no source
application success ACK for this notice; unavailable targets are silent.

Map `SSHandler.cpp:15524–15541` resolves the name and emits CS_CHAT_ACK0x52F5
with group OPERATOR7, sender-id0, published NAME_OPERATOR6 and
NetString(empty,plain-message). CSSender's protected CHAT1 rule still applies
to sender0; a recipient's chat ban does not block received admin notices.
The handoff required reuse of immutable059 raw message6 with verified publication
checks as in postgres_map_item_mail35–56, actual Map MW dispatch and native actor/
protection authorization, plus current registered Control/target checks around
World awaits. These production registrations are now written as described below.

This slice needs no invented historical message table, no game-GM actor seed
and no native guild/party membership. Administrative Control credentials and
game TOPERATORTABLE remain distinct authorities. It is a written World fallback
continuation, not completion of all original operational tools/Relay.

The continuation now implements this World fallback. World consumes the exact
two strings under communication gate, verifies the current registered Control
before/after native target authorization, then relays unchanged source bytes to
the target's current main Map. The incorrect resize1024 has been removed. Map
has actual MW_CHARMSG dispatch, resolves a ready primary, holds its projection
operation, reads the existing verified message6 original bytes and checks
recipient protection for sender0 before the client packet. It formats the whole
plain body into the source eight-digit NetString and adds no ban/spam gate.
No new message table/publication was introduced. These are inspected production
source registrations; Control→client execution is still unverified.
The Map caption reader retains exact published059 message6 bytes, checks the
original256-byte width and validates lossless decoding under the configured
client profile without substituting the decoded text on the wire
(`services/postgresql_map_communication.cpp:90–111`). This avoids treating a
configured modern name codec as authority to rewrite the source caption.

## Event start/end text notification: source and written continuation

The original Happy/manager request CT_EVENTMSG_REQ0x9366 carries DWORD event
index and BYTE message-type. Control `Handler.cpp:1786–1857` looks up its event
registry, uses start text for0 and end text otherwise, and sends a different
source service body using the same opcode: BYTE event-kind, BYTE message-type,
STRING plain-message. A global World event selector (group0, World type,
server0) prefers each connected Relay and falls back to the group's World.
Other selectors choose their group/type/server destinations. Source selector
`(!bSvrID || bSvrID==bSvrID)` is a defect; existing native AnnouncementPeers
intentionally honors the actual selected server.

Existing native Control handlers_event280–299 reads the event from its real
registry under mutation/owner checks and forwards the source text. Its GMLevel3
MayPushEvent requirement is intentional hardening; no per-action CheckAuthority
appears in the original source handler. It does not manufacture event text or
imply that the complete event gameplay producer is implemented.

World `SSHandler.cpp:310–326` relays BYTE kind/type plus STRING unchanged as
MW_EVENTMSG_REQ0x9132 (SSSender3283–3293). Map `SSHandler.cpp:16026–16048` consumes
the two bytes but uses neither to change gameplay state. It sends every primary
client CS_CHAT_ACK(group WORLD3, sender0, published NAME_OPERATOR6,
NetString(empty,plain-message)) through protected CHAT1. Left(1024) is again an
ignored expression, not a real truncation. This consumer can share the addressed
notice's formatting, published operator name and native recipient authority.

The handoff is now written. Map has the actual MW_EVENTMSG consumer and shares
notice formatting, verified original operator text and per-recipient primary/
protection authorization with addressed notices. World holds communication gate,
requires current open registered Control before and between fanout awaits,
checks exact body/tail/NUL and sends only to current typed Map peers. Both BYTE
fields are preserved on the wire without reward/probability/state mutation.
Normal Relay event notifications, direct Map Control transport, event gameplay/
lottery/cashshop and BOW producers remain separate dependencies.

The shared Map formatter preserves full text and checks the complete CS frame
including group/sender, both INT32 string prefixes, operator name and NetString
envelope. An oversized administrative message is skipped without truncating it
or closing a valid recipient. This is explicit bounds handling, not source
proof that the unguarded historical overflow worked. Registered Control→World
→Map→client execution, raw non-ASCII bytes, role takeover during fanout and
protected sender0 remain deferred acceptance.

`test_event_handlers.cpp` still uses Map-role CT senders, untyped peer IDs and
no registered Control setup. These earlier fixtures need a source-role synthetic
setup for Phase B; production guards were retained. No event test was executed
or reported passing in this phase.

Direct CT_ANNOUNCEMENT_ACK is a similar original text producer, but its native
Map Control transport/authentication is still F17 decode-only in handlers/control.
The CS dispatcher exposes those stubs and has no registered Control session
authority there. Do not replace that stub with a broadcast reachable through a
player connection; a real authenticated Control transport/role boundary is
required before closing that original administrative path.

## Retained dependencies and deferred acceptance

The complete chat scope still depends on native mutable guild/tactics, party/
corps membership and source BOW/BR teams/status producers; native PG Map loading
currently refuses guild membership and complete original mutable World bootstrap
remains legacy-dependent. The written operator bootstrap, TMS, scoped ordinary
chat, bans and administrative/event text do not authorize a claim that all
original social functionality, normal Relay transport or full World startup
is implemented.

After the full implementation gate, acceptance must cover every original opcode
and tail, non-ASCII raw names/NetString messages, protected sender0/self/remote,
source ban timing+stacking+relogin, peer takeover during each await, old keys,
malformed count/tails, current member logout/replacement, simultaneous invite/
out/send and room cleanup, source near cells/meeting/unit-country rules and
party/guild/corps recipient membership. The real unchanged client, pinned
operator import/publication and runtime grants need independent acceptance.
These are future checks, not executed evidence in Phase A.
