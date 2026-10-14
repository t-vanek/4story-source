# Phase A: original World peer framing review, 2026-10-14

Baseline: acbcaaf, local main. The analyst owns only this new evidence file.
All production code, tests and shared documents are read-only to the analyst.
No build, test, server, client, service, container, SQL query, import, migration,
extraction or runtime verification was performed. Preexisting audits are untouched.

The selected continuation is original Plain16 framing on native Map↔World and
Control↔World, preserving the existing native ownership/lifecycle paths. It does
not complete original Map admission or World bootstrap. The previously used
native8 framing and RW-as-Map admission were identified as distinct rewrite
extensions, not source compatibility evidence. Production source for the new
framing, DWORD catalog parser, Control type constants and World service monitor
was read; the new implementation remains integrated and unverified.

## Direct original Map and World library chain

Original Map's connection is `CTMapSession m_world`
(`Server/TMapSvr/TMapSvrModule.h:247`), rather than a separate client framer.
CTMapSession inherits CSession (`TMapSession.h:4`) and its constructor only
initializes CanDelete/CheckedSession (`TMapSession.cpp:4–8`). CSession defaults
to m_bUseCrypt=FALSE and SESSION_SERVER (`Session.cpp:40–44`). Map startup
explicitly keeps m_world crypt disabled, connects, registers the socket for IO,
begins reading and sends its Map registration (`TMapSvr.cpp:946–962`).

World CTServer inherits CTWorldSession (`TServer.h:5`), which inherits CSession
(`TWorldSession.h:4`) and explicitly selects SESSION_SERVER in its constructor
(`TWorldSession.cpp:4–7`). CTServer initializes ID0/channel set/cash-sale flag
without enabling crypt (`TServer.cpp:4–8`). World Accept creates that CTServer,
opens the accepted socket, expands receive storage and puts it into the raw
session map before reading (`TWorldSvr.cpp:552–584`). No World code sets
m_bUseCrypt=TRUE in the reviewed original source. The same CTServer represents
Map, Relay or Control according to subsequent protocol/body state; their frame
layout is not selected by role.

World's original vcxproj links the actual Own/TNetLib library
(`TWorldSvr.vcxproj:71,82,116,126`); its stdafx includes TNetLib and protocol
headers. Map and Control link the same library, as established in the preceding
Control→Map evidence. The actual CPacket header is WORD size, WORD opcode,
DWORD number and INT64 checksum (`Lib/Own/TNetLib/TNetLib/Packet.h:36–43`):16 bytes.
Old comments describing an8-byte header are contradicted by this structure.

Fresh CPacket construction calls Clear, which zeroes its buffer and sets size/
body offset to the full header (`Packet.cpp:21–29,177–185`). CSession::Say calls
Encrypt then queues/Post (`Session.cpp:337–349`); Encrypt returns immediately
when crypt is disabled (`69–77`), and Post copies raw packet bytes (`362–374`).
Receive requires the full header and total size strictly below65535; only
SESSION_CLIENT invokes Decrypt (`454–470`). Thus these service peers use plain
header/body with no RC4, XOR, sequence or checksum validation. Game-client crypto
is a separate existing contract.

The exact source bound is total16..65534 and body0..65518. DWORD and INT64 header
fields are not an actor identity, request nonce or replay fence. A source-style
production parser must not apply the native client's cipher merely because it
also has a16-byte header. Size/opcode and body fields remain their original
little-endian widths. STRING bodies carry signed INT32 byte lengths plus raw
bytes; header conversion does not authorize changes to those fields.

## Nonzero copied metadata: source exception to fresh-zero output

Fresh producers send zero number/checksum, but that is not a general receive
requirement. CPacket::Copy copies the whole source header and zeroes only its
checksum (`Packet.cpp:321–337`), leaving DWORD number intact. Source CSHandler
1732 and20593 pass a decrypted client packet to SendMW_MAGICMIRROR_ACK;
SSSender4110–4120 copies it, changes opcode/appends fields and sends through
m_world. A client sequence can therefore survive onto the plaintext World link.
World forwarding senders also Copy packets instead of always rebuilding them.

The source server receiver ignores both metadata fields. The new World and Map
World framers and Control's SourcePlain16 reader now reflect that rule while
writing fresh zero metadata. Earlier strict-zero assumptions from the preceding
Control→Map snapshot describe an explicitly narrower modern profile, not proof
that all source server packets have zero number. Invalid role/body authorization
must remain independent of these unused fields. Acceptance must include a copied
nonzero number and arbitrary unused metadata without interpreting it as crypto.

## Original roles and admission are distinct from framing

Original Map sends MW_CONNECT_ACK0x9002, not RW_RELAYSVR_REQ
(`TMapSvr.cpp:962`, SSSender210–222, MWProtocol9–18). Its body is WORD
MAKEWORD(server-id,Map type4), BYTE count, then BYTE rows. World receives this at
SSHandler592–617, rejects a duplicate full WORD ID, records channel bytes and
inserts the exact Map server into m_mapSERVER. FindMapSvr uses
MAKEWORD(server-id,SVR_MAP4) (`TWorldSvr.cpp:2705–2712`).

The source registration row producer has a concrete anomaly: m_mapTLOGCHANNEL
uses MAKELONG(map-id,WORD logical-channel) keys (`TMapSvr.cpp:2557–2567`), but
SSSender216/220 casts map size and each key to BYTE. The serialized key byte is
therefore the map ID low byte rather than the logical channel. The count can
wrap while the loop still emits every row, and repeated low bytes collapse in
World's mapCHANNEL. DBAccess17–41 derives these mappings from TCHANNELCHART joined
to TSVRCHART for the actual server; TCHANNEL is a separate list. Neither a
guessed config channel nor an assumed clean distinct channel list reproduces
this producer. This conflict needs explicit source-data reconstruction/modern
repair before completing original admission.

World sends no separate application registration ACK for MW_CONNECT_ACK. It
registers then emits the actual source bootstrap in this order:

1. Local and mission battle state from m_battletime, with SKYGARDEN where compiled
   (`SSHandler.cpp:619–637`). The Map consumers use real status/start/day values
   to change local/mission gameplay (`11194–11233,18596–18629`).
2. BOW/BR module connection state based on server ID (`640–657`).
3. Relay connection notification if a Relay exists (`659–660`).
4. Event rows and cash campaigns (`662–668`).
5. Guild castle applicant counts, first tactics-expiry initialization, monthly
   ranks and tournament info (`670–699`).

These dependencies are not fulfilled by framing or a zero-count substitute.
Native PG guild/war/event/rank/bootstrap remains incomplete; no missing battle
state, source channels or downstream application success is invented here.
A completed TCP write proves frame transport only. It is not a source Map
registration acknowledgement or proof that all bootstrap packets were applied.

Original Relay uses RW_RELAYSVR_REQ0x999A with WORD ID. RWHandler5–19 sets the
separate m_pRelay slot and sends RW_RELAYSVR_ACK0x999B; it does not register the
Relay as a Map in m_mapSERVER. It then notifies Maps (`21–23`). ACK is BYTE nation,
WORD operator count plus DWORD IDs, WORD message count plus rows(DWORD message
key,STRING raw value) (`RWSender.cpp:3–30`). The key is not STRING. Source Control
uses empty CT_CTRLSVR_REQ0x9359 to set m_pCtrlSvr (`SSHandler.cpp:207–214`), with
no ACK. Role slots retire independently on disconnect (`TWorldSvr.cpp:749–758`).

Baseline native Map instead used SetRelayWid(0x0400|sid), sent RW_RELAYSVR_REQ
and waited for its RW_ACK. Native World OnRelaysvrReq registered that typed Map
in its peer registry and added bootstrap replays. This remains an explicit
RW-as-Map extension in this bounded continuation. Source MW_CONNECT_ACK/full
Map bootstrap stays open; changing the header cannot silently close it.
Its original-style Relay ACK payload was already written by World, but native
Map ReadConnection parsed message key as STRING. That was a concrete parser
mismatch even between these native endpoints, now repaired to bounded DWORD
key/STRING value rows and exact tail parsing. The extension's ACK precedes its
event replay; it is not a source ACK preceding original Map bootstrap.

## Control inventory types: direct source constants, no remapping

Root found the native World profile selector depended on an inferred wrong
kWorldSvr5. Actual linked CTProtocol26–35 gives this Control inventory namespace:

| Type | Source ID |
| --- | --- |
| None | 0 |
| Control | 1 |
| Login | 2 |
| World | 3 |
| Map | 4 |
| Patch | 5 |
| FTP | 6 |
| Log | 7 |
| Relay | 8 |
| PreFTP | 9 |

Source Control Handler compares these constants for World/Map/Relay routing and
stop cascades; stdafx links/includes CTProtocol. NetCode TSVR_TYPE912–921 is a
separate peer namespace: None0/Control1/Login2/World3/Map4/Relay5/Kick6. In
particular Relay peer5 is not Control inventory Relay8. BOW and BR are Map modes,
not additional invented Control inventory types.

TSVRTYPE backup schema is tinyint bType PK, varchar50 name and tinyint bControl
(TGLOBAL tables771–775); existing pinned metadata records exact_count8
(`data-profiles.json:370–375`). That count does not prove specific private row
IDs/names. No private published PostgreSQL row values were queried/extracted.
The existing importer preserves raw bType in both TSVRTYPE/TSERVER
(`import_control_metadata.py:29–30,97–100`) and SociServiceInventory reads those
IDs directly (`132–158`). Therefore the correction is runtime constants, not
rewriting the archival/native data to fit guesses. Source excludes type6 in
CTBLServer71; it is FTP, not an inferred message service. Runtime constants now
match source0–9 and remove unused fictional BOW/BR types. Relevant native
Control admin/event/profile selectors consume the corrected names.

## Static review of the new production framing

WorldSession now has the full16 header and source maximum65534, receives body
without native8 XOR-fold checks, ignores source unused metadata and sends fresh
zero fields. Compile-time header-size/endian assertions document the actual
layout; no compiler was run. Complete-write serialization/backpressure and
close-on-read/write failure remain. Oversized output retires the peer instead
of silently implying an application response. This also changes native Relay
traffic accepted by the same World listener to source Plain16; the actual Relay
server implementation/functionality remains open.

AsioWorldClient now frames16 bytes, accepts total below65535, ignores number/
checksum, keeps captured current-socket checks around reads/writes and preserves
its existing reconnect drain/registration deadline. It still uses the explicit
RW-as-Map extension. The bounded ACK parser reads actual DWORD keys/counts/tails;
the communication relay helper uses body<65519 so a known codec overflow drops
before a bool transport failure could close/replay a valid player request.
Client AsioSession cipher and original client packets are unchanged.

Control PeerDialer selects SourcePlain16 for source Map4 and World3. Requested
TLS on either source plaintext endpoint fails without downgrade. Other existing
native8 endpoints/operator transport remain separate incomplete original16 ports.
SourcePlain16 receive ignores unused metadata, while fresh output stays zero.
No invented handshake token or result opcode is added. Existing Control owner,
exact current connection/topology and World registered Control/actor gates are
still necessary; valid plaintext bytes alone do not authorize a role.

World's source service monitor body is DWORD tick and response is four DWORDs:
echoed tick, all raw sessions, current characters and historical unique accounts
(`SSHandler.cpp:5–14`, SSSender24–37). DATACLEAR is empty and clears/reseeds only
historical accounts from live chars (`133–146`). Native monitor now requires the
actual current open Control, exact body/tail and a raw_session_count callback
bound by WorldServer to its real m_live count. Anonymous, Map, Relay and Control
accepted sessions are counted independently of registered role/cache size.
HandlerContext copies retain the same WorldServer lifetime dependency as their
existing connection dispatch; the server remains the owner during those awaits.
Historical character/user counts and clear use CharRegistry. Missing wiring must
drop rather than synthesize a successful0 collection. The final source snapshot
explicitly requires both CharRegistry and the listener callback before replying.

Existing World fixture frame helpers were statically changed to header16/fresh
zero output; independent writer fixture and invalid-size cases replaced the
incorrect checksum oracle. Control fixtures now use source World3/Relay8 IDs.
Those edits are source material, never evidence that suites compile or pass.
Service-plane fixture's three-session seed contains two Maps and Control; an
additional anonymous socket is required in later acceptance to distinguish raw
session counting from the old registered-only approximation.
That fixture also predates current Control trust setup: ctx122 sets the slot but
ctx119–125 does not set control_ip_allowlist before CT_CTRLSVR at154. Header
alignment alone cannot make its later Control assertions accepted by the actual
role guard; Phase B must supply explicit synthetic transport trust and retain
production denial. No passing result is inferred from these fixture edits.

## Exact next party prerequisite, not an implemented feature

The preceding Control→Map audit has the original Map/client party bodies and
World process-local max-seven topology. The next handoff is to preserve those
bodies while establishing one operation/generation boundary for invite/join/
chief/out/logout and main↔replica attributes. Native World handlers_party still
routes by low wid byte, uses name-only ADD/JOIN and mutates waiter before current
actor authorization; DEL trusts a party/member target without proving issuer.

Before Map party stubs can be enabled, World needs exact typed current Map peer,
current char object/key/primary valid+ready generation, PG account/current claim
authority under sorted locks and one-use invite expectations bound to both actor
generations. A chief kick packet names the target rather than issuer, so target
claim alone is insufficient. After every DB/send await, recheck actor/room/route
generation and current membership; reconcile handoff/logout cleanup in the same
operation boundary. Map requires actual HPMP/action/arena/protected INVITE2,
primary+replica PARTYATTR publication and source client field/order gates. No
fictional party SQL table is introduced: source CTParty is process-local.
Loot/corps/instance/Relay/BOW/BR and full World bootstrap remain required followups.
No party source was edited in this continuation.

## Deferred verification and remaining boundaries

Later acceptance must independently verify source raw header bytes at16/65534
and reject malformed/truncated/65535 frames; fresh zero output and copied nonzero
number reception; no native8 checksum/client cipher applied; little-endian field
widths/INT32 strings/raw non-ASCII values; bounded count/DWORD message parsing,
partial writes, queued-write saturation, EOF/reset and no old socket replay.
Exercise Plain16 Map↔World/Control↔World with actual native owners/receipts and
current peer takeover through every await, real World anonymous-session counts,
Control tick wrap and active-user clear/relogin, corrected inventory type filters
and requested TLS rejection. Existing captured socket/drain/unknown-commit rules
remain mandatory; successful frame transport is not client/application success.

Original MW_CONNECT_ACK/channel anomaly/full ordered bootstrap, service/operator
native8 framing ports, full Relay, native guild/war/event/rank producers, complete
party authority/topology and the full modernization gate stay open. No build,
test, service, client, SQL execution or real-client acceptance has been performed.
