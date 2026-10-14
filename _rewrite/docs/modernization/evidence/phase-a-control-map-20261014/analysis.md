# Phase A: original Control→Map and party topology review, 2026-10-14

Baseline: 0b97c18 on local main. This audit uses source reads only. No build,
test, server, client, container, SQL query, migration, extraction or runtime
verification was performed. The analyst owns only this new evidence file;
production edits and shared status documents belong to the implementation
owners. Preexisting `_rewrite/docs/audits/` remains untouched. The bounded
five-operation continuation is now written and integrated, unverified; the
baseline observations below are retained as source evidence and issue history.

## Selected independent production continuation

Control→Map is the nearest independently implementable source slice. It can
reuse the already written raw message059 caption, protected CHAT1 recipient
authorization and native Map lifecycle rather than waiting for guild/party/
corps, monster rewards or BOW peace producers. The selected continuation is the
real Control transport and authority boundary, source ANNOUNCEMENT, source
monitor counts/active-user reset, and ready-player character-name KICK through
the ordinary fenced close/save/release path. Root owns Control edits;
development owns Map. The final production source paths were read as described
below. Compilation and execution remain unverified.

Basic party formation is mapped below as the next independent social topology
candidate. Its larger unclosed actor/generation, membership, replica and
logout graph makes it a separate continuation. It must remain in the full scope.

## Correct original framing: actual library, not rewrite comments

Root identified the actual original packet header while reviewing the handoff.
The analyst's preliminary eight-byte assumption came from modern ControlSession
comments, not the linked original library, and is withdrawn. Both original
Control and Map include `Own/TNetLib/TNetLib` and link TNetLib.Lib/d.Lib in their
vcxproj (Control72,83,117,128; Map72,83,118,128). Both stdafx headers include
TNetLib.h. `Lib/Own/TNetLib/TNetLib/Packet.h:36–43` defines WORD size, WORD opcode,
DWORD number, INT64 checksum: the original header is16 bytes. The old comment
at Packet.h13–16 does not describe this actual structure.

The complete original Control→Map send/receive chain is:

1. CTServer inherits CTControlSession (`TServer.h:3`), whose constructor sets
   SESSION_SERVER (`TControlSession.cpp:4–7`). CSession starts with
   m_bUseCrypt=FALSE and SESSION_SERVER (`Session.cpp:40–44`).
2. Each CTServer sender allocates CPacket, SetID and writes its source body,
   then calls Say (`TServer.cpp:16–42,121–126,148–153`). CPacket's constructor
   calls Clear (`Packet.cpp:21–29`); Clear zeroes the full buffer, sets size to
   PACKET_HEADER_SIZE and body offset to that size (`177–185`). A fresh frame's
   number and checksum are therefore zero.
3. CSession::Say calls Encrypt then queues/Post (`Session.cpp:337–349`).
   Encrypt returns immediately when !m_bUseCrypt (`69–77`). Post copies the
   raw GetBuffer/GetSize into its output buffer (`362–374`). There is no
   replacement eight-byte header, sequence increment or checksum calculation
   on this plaintext path.
4. Original Map accepts the configured Control IPv4 as SESSION_SERVER while
   keeping crypt disabled (`TMapSvr.cpp:1021–1037`). CheckMessage requires the
   full PACKET_HEADER_SIZE and total size less than65535; only SESSION_CLIENT
   calls Decrypt (`Session.cpp:454–470`). Thus Control→Map is plaintext16 with
   zero number/checksum on these fresh source producers.

Current native ControlSession instead declares8 bytes (`control_session.h:30–42`)
and frames from that layout (`control_session.cpp:205–214,239–273`). That is a
rewrite divergence, not source provenance. Its other native endpoints/operator
plane must be kept explicit when adding an original Plain16 profile for Map.
Wider original service/operator framing acceptance is still unclosed; correcting
the Map branch cannot claim it fixed all other native endpoints.
The earlier committed chat/TMS evidence called the native Map→World8-byte
envelope "original" when discussing its65527-byte body cap. That historical
wording is superseded by this direct linked-library review: the cap describes
the existing modern8 transport, not proven original server framing. Payload
layout work and the client16 bounds remain separately reviewable.

Current AsioSession always reads16 bytes (`asio_session.cpp:234–252`) but also
always increments sequence, decrypts the header and checks checksum
(`289–310`); output always increments sequence/encrypts body/header (`406–418`).
Merely choosing PeerType::Server or omitting RC4 does not implement original
plaintext Control. A real explicit Plain16 mode must bypass those crypt/sequence
operations on that transport while leaving original client crypto behavior
intact. The owner received this API handoff. Neither wire was executed.
The first written transport snapshot has an explicit SourcePlain16 profile in
native ControlSession and a separate Map ControlSession that directly reads/
writes the full raw source header. PeerDialer selects it only for Map services
and rejects a requested TLS Map profile instead of downgrading. Other native8
endpoints retain their distinct current profile. At that first snapshot,
registration, administration and counter integration were still separate
handoffs; their final production source paths are reviewed below. Transport
classes alone do not establish completion.

## Original Control transport and topology authority

Original Control `Handler.cpp:170–218` resolves a service ID, dials the machine's
private address/service port, associates that specific CTServer with the
service, begins reading, sends empty CT_CTRLSVR_REQ, then sends active events.
It binds its configured local address (`Session.cpp:481–504`). This is an
outbound Control connection to the existing Map listener, not a player request.

Original Map gets Control address from TLoadService(world0,type1)
(`TMapSvr.cpp:1839–1847`, `CTProtocol.h:26–30`). CSPLoadService has BYTE world/type,
output TCHAR[MAX_NAME+1] address and WORD port (`DBAccess.h:5882–5897`). The TGAME
procedure delegates to TGLOBAL_GSP; the actual TGLOBAL procedure selects TSERVER
by group/type then TIPADDR by machine ID, without ORDER BY or bActive filter
(`_rewrite/docs/schema/procs/{TGAME_RAGEZONE,TGLOBAL_RAGEZONE}/TLoadService.sql`).
Backup schema retains TSERVER tinyint group/server/type/machine, signed smallint
wPort carrying WORD bits and varchar50 name (TGLOBAL tables747–754), and TIPADDR
machine/public/private varchar50/bActive with machine/public PK (511–516).
Ambiguous rows are not authority to guess a missing endpoint.

The accepted source Control address consumes the single Control slot
(`TMapSvr.cpp:1021–1032`); ordinary client connections are crypt-enabled in the
else branch. CloseSession releases that slot only for the actual server/address
(`1752–1759`). CT_CTRLSVR_REQ itself is empty and is a no-op
(`CSHandler.cpp:230–233`): it supplies neither an authentication token nor an
application ACK. Its modern counterpart must rely on explicit transport trust,
current connection identity and retirement checks. An empty configured trust
list must not grant player-origin CT authority.

Native Control already has the imported app_control inventory049 and an actual
single writer owner in native_control.cpp backed by an advisory lock/generation;
no fake Control service row or credential is needed. The existing dialer uses
inventory snapshots, private addresses, current service identity and optional
TLS identity checks (peer_dialer33–45,99–175; handlers_service243–311). A selected
original plaintext Map profile must be documented separately from TLS and the
retained native8 profile. Native Map owner/session SQL authority remains
necessary around effects; the original empty body cannot be expanded with an
invented historical ownership token.

## Exact five-message contract

CT_CONTROL is0x9301 (`ProtocolBase.h`). Strings use the actual CPacket signed
INT32 byte-length plus raw bytes, not a WORD length. No client changes are
authorized.

| Message | Opcode | Source service body / effect |
| --- | --- | --- |
| CT_CTRLSVR_REQ | 0x9359 | empty; Map no-op, no application reply |
| CT_SERVICEMONITOR_ACK | 0x931F | DWORD Control tick; Map replies with four DWORDs |
| CT_SERVICEMONITOR_REQ | 0x931E | echoed tick, sessions, players, historical unique accounts |
| CT_ANNOUNCEMENT_ACK | 0x932B | STRING plain message; primary client CHAT_WORLD |
| CT_USERKICKOUT_ACK | 0x932D | STRING character name; close matching named Map player |
| CT_SERVICEDATACLEAR_ACK | 0x9350 | empty; reset/reseed active-account statistics |

Definitions: CTProtocol117–123,135–139,190–200. Source Map registrations are
TMapSvr5333–5343. CTServer producers are TServer16–42,121–126,148–153; Map monitor
response is CSSender8–21. There are no invented generic success packets or
extra name/account/key fields in these source bodies.

### Announcement and original client

Original manager sends DWORD group filter plus STRING text; Control requires
MANAGER_GMLEVEL1 (`Handler.cpp:708–720`). It prefers a connected Relay per group
and falls back to all connected Maps of missing-Relay groups (`722–755`). Missing
Relay inventory is not equivalent to a documented disconnected Relay row;
normal Relay transport remains open.

Map CSHandler72–88 reads STRING and broadcasts to m_bMain players using
CS_CHAT_ACK0x52F5, group WORLD3, sender0, GetSvrMsg(NAME_OPERATOR6),
BuildNetString(NAME_NULL,full plain message). `Left(1024)` discards its return and
does not truncate. CSSender2621–2640 still applies protected CHAT1, including
sender0; no recipient ban, spam or country gate is added. The original client
TRelayHandler87–190 consumes that chat/NetString layout. The already written
World event notice Map consumer is therefore a reusable packet/protection path,
not a new message table or localized caption substitute.

Native full-frame bounds must count both INT32 string prefixes, actual raw059
caption6,8 NetString hex bytes, group/sender and16 header bytes. Oversized text
must not silently become1024 bytes or close a valid recipient. Existing shared
notice code uses verified immutable059 publication and lossless configured
profile validation while preserving raw caption bytes. Registered Control/current
peer and Map owner guards are needed before and after suspended authorization
and for each fanout recipient. Attempted client enqueue has no delivery receipt.

### Monitor and active-account reset

Map CSHandler27–67 answers each Control DWORD tick with echoed tick,
DWORD(m_mapSESSION.size()), DWORD(m_mapPLAYER.size()),
DWORD(m_mapACTIVEUSER.size()). The source stale-client housekeeping runs at most
every five seconds, closes client sessions with accept/close ticks older than
ten seconds, and preserves delayed deletion/save work. This is separate from
the modern preauth watchdog/normal close drain; do not reset gameplay cache.

Source SESSION insertion includes accepted Control and preauth sockets
(`TMapSvr.cpp:1078–1080`), deletion occurs at1544. Source PLAYER insertion occurs
on accepted CS identity before World load (`CSHandler.cpp:364–380`), and removes
at TMapSvr1571–1575; it includes replicas, not only current ready primaries.
ACTIVEUSER adds user ID at that insertion, does not erase on ordinary logout,
and is a historical unique-account set until reset. DATACLEAR clears it and
reseeds from every m_mapPLAYER's user ID, without a primary filter
(`CSHandler.cpp:214–225`). It does not clear inventories, actor state, sessions,
catalogs, persistence receipts or bans. Modern counter milestones that differ
from the original insecure pre-load insertion must be explicit.
The source accepted timeout marker is cleared at that accepted CS identity
insertion (`CSHandler.cpp:376`), before later World load/ready stages. Applying
the ten-second timeout to every not-yet-Ready loaded/admitted actor would change
this contract and could kill legitimate DB admission work. Source delayed
closing deletion additionally waits for m_bCanDelete; native retained dirty
reservations must never be discarded by this sweep.

Original Control Handler250–277 sends periodic monitor ACK with its own current
DWORD tick; Handler299–336 consumes the Map response, updates receive/max/pick
time and sends manager data without another monitor echo. Source elapsed tick
arithmetic is wrapping DWORD. An RTT must use the echoed Control-origin tick,
not a fresh Map clock.
TimerThread1412 uses CTRL_TIMER=1000ms (`TControlType.h:11`); the source timer
enqueues CT_TIMER_REQ once per interval. Equal peaks refresh source pick time
because its comparison is `m_dwMaxUser <= dwUser` (`Handler.cpp:320–324`).

Two baseline native producer gaps were handed to root: OnServiceMonitorReq
handlers_service375 immediately sends another monitor ACK, which would create
an unlimited echo cycle with the original Map response, while
ControlServer::PeerKeepaliveLoop235–303 never sends a live-peer periodic ACK.
The repaired producer must poll periodically under current owner/current peer
identity, consume responses from its current registered peer and publish real
counters without another monitor echo.
The current native Map LiveSessions atomic, bound registry and new accumulated
active-account set need meaningful distinct counts; hardcoded zeros are not
source monitor parity.

### Character kick and lifecycle

Control Handler764–785 requires MANAGER_GMLEVEL2 and broadcasts STRING to all
connected Maps. Map CSHandler92–107 uppercases it, finds m_mapPLAYERNAME and
closes that session. Despite the variable strUser, this is character-name lookup,
not account login lookup. Name insertion occurs after EnterMAP in InitMap and
before the !m_bMain return (`TMapSvr.cpp:8131–8142`), so both primary and replica
named players are eligible. Preauth/account-only sessions are not name matches.
Source removal1578 onward protects a suspending replacement; current identity
must similarly keep an old kick from closing a successor by name after an await.

Native Map ordinary connection teardown waits for outstanding operations,
preserves the prior role/phase, saves the full current snapshot when required,
retains reservation on save/release failure, releases the exact PG claim, then
sends MW_CLOSECHAR_ACK for announced presence and removes matching state
(`map_server.cpp:173–244`). Control kick must close the actual captured session
and allow this path to do its work, not directly Unbind/erase/account-unlock.
Replica close must keep its existing independent lifecycle. Owner/claim and
session object/key/connection/authority epoch must be checked before an effect;
no asynchronous name-only stale lookup is sufficient.

Source has no application kick success ACK. Baseline native Control
handlers_admin154 records AdminOutcome::Success before forwarding. This must
not claim the target was closed or persisted; root received the request/dispatch
audit handoff. Startup/role/recovery and installed PG privileges remain later
acceptance, not proven by source reads.

Full original close contains a wider combat/logout dependency: source
SM_DELSESSION marks deletable, sends World CLOSECHAR for m_bCloseAll, otherwise
calls SetEventCloseSession (`SSHandler.cpp:25–43`); World DELCHAR then removes
map visibility and saves/deletes according to its source flags (`1212–1244`).
SetEventCloseSession calls OnDie before save for a non-graceful saved primary
with a recorded attacker inside60 seconds (`TMapSvr.cpp:1667–1676`). Native
Control kick can reuse the existing fenced native close path without claiming
that this full combat penalty and other broader logout gameplay are implemented.
This dependency was handed to root and remains in full-scope acceptance.

## Static review of the written five-operation continuation

MapServer now selects an explicitly configured Control IPv4/CIDR before client
World registration, client cap and client crypto. The default empty allowlist
does not authorize this role. It keeps one current Control session; a duplicate
live connection is rejected and an old completion clears only its own slot.
The source plaintext16 transport has a strict total<65535 and zero reserved
metadata check, complete-write serialization and no reconnect replay. The
zero-field check, required empty first CTRLSVR within five seconds, default
denial and CIDR trust policy are explicit modern transport hardening; the source
empty body itself is still a no-op with no fabricated ACK.

Actual CMake, configuration, MapServer::HandleControl, typed OnControlPacket
and main callbacks are connected. The old AsioSession CT exports now reject
client transport, and the CS dispatcher rejects CT opcodes before gameplay
dispatch. A ready player cannot promote itself by choosing CTRLSVR. Native
Map CheckControl opens a regular service-pool owner transaction and rechecks
the exact current open peer after awaiting it. Main fails closed on a failed
owner check, drains existing client and Control operations, then stops;
the dedicated PostgreSQLMapOwner timer connection is not shared concurrently.
Shutdown/owner-loss drain includes Control operations, while loss of the World
game link retains the independent Control socket. No new migration is required
for these existing owner/claim/message authorities.

Map monitor outputs real transport sessions plus Control operations, accepted
character identities and the accumulated unique user-ID set. MarkAccepted occurs
only after native CS claim/role acceptance, before World loading; provisional
TryBind does not count a failed claim. Both roles count, logout keeps historical
accounts, and DATACLEAR reseeds only that set from accepted identities. Dirty
reservations can remain in native player counts while a transport coroutine
retires, an explicit recovery boundary that is not claimed to reproduce every
historical failed-save counter milestone. The source >5s/>10s sweep closes only
unaccepted transports/repeats a closing socket's Close and never directly
deletes a pending save reservation.

Static review found the existing configurable preauth watchdog still tested
Admitted/Ready and could close a valid accepted Loading/Loaded actor. The owner
repaired it to use the same Accepted marker (`map_server.cpp:173–177`), consistent
with the source accepted tick clear. The configurable watchdog interval remains
an operational policy; no timing run was performed.

Announcement invokes shared original chat/NetString formatting and raw059
message6 under primary projection/native SQL/protected CHAT1. The exact current
Control callback is checked before each recipient, during projection waits and
after SQL/caption/protection awaits. Full text and complete client bounds remain
checked, with no ban/spam/country gate and no recipient close on overflow.
The final static review found a second unintended close: the generic projection
helper closed a recipient after waiting three seconds for a slow checkpoint.
The owner added an explicit timeout policy so a Control announcement skips that
recipient instead (`communication.cpp:58,66,228–230`). This bounded delivery
limit is a modern difference; unrelated existing TMS/ban timeout policy is
unchanged. Attempted client enqueue returns void and proves no client delivery.

Kick supports real ready named primary and replica sessions. It holds a player
operation, confirms current native Map owner/account/ready claim and canonical
character/current-user identity, then checks the captured object key, connection,
authority epoch, role, phase, name and current open Control before Close. It
does not write account logout state or erase character/registry data directly.
The normal teardown owns save/release/World close and retained failed-save state.
Existing main transfer first changes phase and waits for other operations
(`handlers_main_transfer.cpp:30–38,87–93`); a kick resuming after that change
fails its Ready/generation check and releases its lease. Other entered or
transferring source kick cases remain explicitly open rather than inventing
authoritative partial-graph close logic. Complete high-bit account/claim binding
and locale-specific ANSI uppercase parity are retained broader limitations.

Control now explicitly chooses SourcePlain16 for typeMap4; requested TLS Map
transport fails without plaintext downgrade. Other native8 endpoints remain
modern profiles, not source16 parity claims. Its periodic monitor sends source
ACK(tick) at the one-second cadence under current owner and exact peer/service
identity, while the response parser consumes four DWORDs with no immediate echo.
Source equal peak refresh uses >=. Owner-check exceptions are caught by the
monitor's fail-stop path. Announcement/KICK/DATACLEAR use service snapshots,
current peer topology, owner checks and an open issuing operator around awaits;
DATACLEAR preserves live cur_users while resetting historic fields. Both wire
and admin-shell kick audit now use Requested, not Success. These are observed
source registrations and declarations, not runtime proof or audit delivery ACKs.

Original SendEventToNewConnect explicitly includes Map type4 and sends matching
active direct CT_EVENTUPDATE_REQ or CT_CASHITEMSALE_REQ
(`TControlSvr.cpp:1838–1860`). No such Map CT consumer/registration was found in
original Map source; its World MW event route is separate. This conflicting
producer/consumer evidence cannot be resolved by fabricated event state or an
empty success. The new endpoint closes unsupported CT opcodes, consistent with
the original unknown-message error path. OnNewConnect now rejects a matching
active direct Map event before dial/publication, under the held event-registry
mutation lease (`handlers_service.cpp:253–263`). Its real source target selectors
are used and no event is silently marked applied. Direct Map event/cash replay
remains an open dependency outside the bounded five operations. Complete
administration and bootstrap are not claimed when real topology/event state
requires an unsupported command.

The existing Control `tests/test_peer_monitor.cpp:181–197` sends unsolicited
REQ(tick42) and expects an immediate echo ACK. That fixture describes the old
native loop, not original source polling. Later acceptance needs periodic
ACK first, then four-DWORD response with no repeated ACK. Map-target fake peer
helpers using native8 must explicitly use the new Plain16 profile. These
fixtures were read only; no test was run or reported passing.

## Basic party topology: mapped next dependency, still open

Original client producers CSSender158–190 send ADD(STRING target,BYTE obtain),
JOIN(STRING inviter,BYTE obtain,BYTE answer), DEL(DWORD member). Original Map
CSHandler3419–3503 adds actual actor name and real max/current HP/MP to World
ADD/JOIN, and uses actual party/chief membership to permit self-leave or chief
kick. ADD requires map+primary, rejects BR and arena; JOIN requires primary and
rejects BR. DEL requires map+primary and a current nonzero party. Chief/type,
move, party HPMP, loot, corps, instance maps and BOW/BR branches are separate
required continuations, not discarded features.

Map→World ADD is STRING origin/target, BYTE obtain, four DWORD maxHP/HP/maxMP/MP
(SSSender2671–2690). JOIN adds BYTE response before those four DWORDs
(2692–2713). DEL is WORD party,DWORD departing member,BYTE kick (2715–2726).
These historical packets do not carry actor id/key as a prefix; native World
must derive and verify the actor from its live Map route/name and DB claim.
In particular a kick names the target, so authorizing only that departing member
cannot prove the issuing peer's chief role. Additional authority must remain an
explicit modern repair, not an invented historical packet field.

World source SSHandler2486–2621 validates inviter/target presence, self, target
waiter, target existing party, exact WarCountry, current chief and seven-member
capacity before setting waiter/relaying invite. JOIN2623–2815 clears waiter,
rechecks availability/answer/country/current party and forms or appends the
party with actual HPMP. TParty.h3 sets MAX_PARTY_MEMBER7. State is process-local
CTParty/m_mapTParty; no party persistence table was identified in the reviewed
World DBAccess and TGAME table schema. Do not fabricate one merely to label this
process-local state PostgreSQL-backed.

World JoinParty2930–2987 sends pairwise real member description packets, then
adds the target/back-link and PartyAttr. It requires current source Map routes
and rejects crossing BOW/BR boundaries. Member order is source vector order.
PartyAttr2882–2926 sends actual party/type/chief/commander to main and valid
replicas. LeaveParty2990–3058 handles next chief, two-member dissolution, corps
and Relay cleanup, emits DEL before removing and finally PartyAttr. Logout and
handoff must share the party operation/generation boundary.

Map SSHandler7839–7921 responds busy when invitee is missing/not main/not in map,
uses actual action-block and protected INVITE2 before client JOINASK, and sends
failure ADD without inventing success. JOIN7923–7991 updates actual cluster
party/chief/commander/type and forwards the exact member description. Source
DEL7994–8028 parses new chief/commander/party but passes the old cached values to
the client: this ordering defect must be documented if repaired, with PartyAttr
subsequently providing the authoritative values. PartyAttr10429–10509 updates
both primary and replica, broadcasts HUD to primary/near neighbors and leaves
instance/arena state where source requires it.

Original client consumes JOIN as WORD party,STRING member-name,DWORD member-id,
DWORD chief,WORD commander,STRING guild-name,BYTE level,four DWORD HPMP,
BYTE race/sex/face/hair/obtain/class (CSHandler4937–4958; CSSender1729–1766).
DEL is DWORD member/chief,WORD commander/party,BYTE kick (4976–5015;
CSSender1771–1784); ATTR is DWORD char,WORD party,DWORD chief,WORD commander
(5021–5034; CSSender3129–3139). It uses these values for actual party/HUD state.

Baseline native Map social.cpp remains three decode/log stubs; no Map World
party response registration was found in the inspected dispatcher. Map cluster
has party/chief/commander/type fields, but existence of zero defaults is not a
completed membership producer. Native World handlers_party has substantial
existing topology but matches routes by wid low byte (24–29), performs name-only
ADD/JOIN, mutates waiter before actor authorization (336–344), and DEL checks
only named party/member (641–682). It lacks the newly added communication actor
SQL/current-primary checks, exact typed peer gates, one-use generation-aware
invite expectations and an operation gate across fanout/member mutation.
JoinPartyFanout95–138 snapshots routes and awaits before committing membership.
The existing code therefore cannot safely become the native production authority
merely by wiring the Map stubs. This is the next concrete Map+World handoff,
not a request for development to edit outside current Control priority.

## Required deferred acceptance

Acceptance after the implementation gate must cover actual original plaintext16
bytes/zero checksum and sequence, original client crypt unchanged, selected
modern profiles/TLS incompatibility handling, unauthorized CS-origin CT, current
Control takeover/disconnect during every await, empty/malformed tails and NUL,
source announcement raw text/protected sender0/frame limit, real monitor cadence
without echo loop, Control DWORD tick wrap, separate pending/session/player/
historical account counts, logout versus DATACLEAR, primary and replica name
kick, same-name successor/transfer/checkpoint races and retained dirty state on
failed save/release. Operator audit must distinguish dispatch from proven target
effect. No service or test was run to establish any of these outcomes.

Party follow-up acceptance must exercise actor spoofing, pending invite replay,
country change and Map takeover between awaits, max seven/dedup/current chief,
multiple invites/answer/out/logout serialization, exact client member/HUD field
order, main+replica attribute propagation and every retained corps/loot/instance/
mode/Relay branch. Full native World bootstrap, Relay implementation and the
complete modernization gate remain open.
