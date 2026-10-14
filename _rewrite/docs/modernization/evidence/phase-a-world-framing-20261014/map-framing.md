# Map to World plaintext framing — 2026-10-14

Status: **IMPLEMENTED / INTEGRATED_UNVERIFIED**. Static source/API comparison;
no build, test, service, client, SQL, migration, import, extraction or runtime
verification was performed. No shared library, original server/client source or
database schema was changed. Exact stock-source Map registration remains OPEN.

## Source contract and implementation

| Source evidence | Native continuation |
| --- | --- |
| [TMapSvrModule.h](../../../../../Server/TMapSvr/TMapSvrModule.h):247, [TMapSession.h](../../../../../Server/TMapSvr/TMapSession.h):4 and [Session.cpp](../../../../../Lib/Own/TNetLib/TNetLib/Session.cpp):40–44 | Actual m_world is CTMapSession:CSession; defaults are SESSION_SERVER and no crypt |
| [TMapSvr.cpp](../../../../../Server/TMapSvr/TMapSvr.cpp):946–962 | Explicit no-crypt World connection, receive startup, then source MW_CONNECT_ACK |
| [Packet.h](../../../../../Lib/Own/TNetLib/TNetLib/Packet.h):36–43 | Packed16 bytes: WORD total size, WORD ID, DWORD number, INT64 checksum |
| [Session.cpp](../../../../../Lib/Own/TNetLib/TNetLib/Session.cpp):68–84,309–374,454–474 | No crypt on Say; server CheckMessage ignores sequence/checksum metadata and rejects sizes below16 or equal65535 |
| [Packet.cpp](../../../../../Lib/Own/TNetLib/TNetLib/Packet.cpp):177–185,321–337 | Fresh Clear zeros metadata; Copy preserves number and resets checksum |
| [Native World client](../../../../../Server/TMapSvrAsio/services/world_client.cpp) and [declarations](../../../../../Server/TMapSvrAsio/services/world_client.h) | Plain16 replaces historical SS8; exact body reads, total<65535, fresh metadata0, incoming metadata deliberately unused |

This is an original server CPacket envelope, independent of the stock client's
CS16 cipher. It applies to every actual AsioWorldClient production send/receive
path. Header layout and native little-endian are statically asserted. The old
SS8 DWORD XOR-fold checksum is removed, with no RC4/XOR or invented sequence
check substituted. Current socket and body/session authority retain their own
guards; an ignored packet number is never a source of authentication or replay
permission.

Source Copy can carry a previous client number into a server frame, for example
the unported CS/MW MAGICMIRROR forwarding chain. Receive accepts those metadata
bytes rather than incorrectly treating fresh-zero output as a rule for every
valid original input. Native sends construct fresh zero-number/checksum headers.
Exact reproduction of unported copied-message producers remains separate work.

## Preserved extension admission and lifecycle

The public IWorldClient/AsioWorldClient method signatures are retained. A write
still captures its original socket and owns the permit across the whole composed
write. Waiting senders recheck that socket after suspension; failed or unknown
writes are never retried on a replacement. Size validation occurs before frame
allocation or writing. A successful bool result indicates completed transport
write only, never application acceptance.

The current RW_RELAYSVR_REQ/ACK-as-Map extension retains typed wid4/server ID,
its registration deadline and full-body acknowledgment gate. TCP connect or a
completed registration write cannot set IsRegistered. The shared RW ACK body is
BYTE nation, WORD operator count, count×DWORD IDs, WORD message count and
count×(DWORD message ID, STRING value). [Original RWSender.cpp](../../../../../Server/TWorldSvr/RWSender.cpp):3–30
proves the message key is DWORD. The previous STRING key parser is corrected;
full WORD counts, minimum remaining field sizes, string lengths and exact tail
are checked before opening admission. This parses the existing catalog without
inventing a fallback table or substituting the framing for source bootstrap.

Acknowledgment still precedes event replay processing. The event connection
generation advances only on complete registration, resets its values, and then
applies validated incoming updates in receive order. Event/spawn/cash capability
gates remain in place. On link failure, admission closes before the awaited
native client save/release drain, and only then may reconnect create a new socket.
Header/body reads also recheck the captured current/open socket before dispatch.

[Communication Relay](../../../../../Server/TMapSvrAsio/handlers/communication.cpp)
uses the same exported16-byte header/max constants. Body65518 is the largest
permitted payload (frame65534); body65519 is rejected (frame65535). A known
oversized relay is logged/dropped without client closure, truncation, requeue or
substitute success. Actual failed World writes retain their existing close and
unknown-outcome behavior. Generic SendPacket rejects oversize before a write and
returns false; caller-specific durable operation handling is retained.

## Original Map registration/full bootstrap — OPEN

[Original SSSender.cpp](../../../../../Server/TMapSvr/SSSender.cpp):210–222 emits
MW_CONNECT_ACK0x9002 with WORD MAKEWORD(server ID, Map type4), BYTE count of
m_mapTLOGCHANNEL and each sorted key cast to BYTE. The source table is constructed
with MAKELONG(map ID, logical channel), so the emitted low BYTE is not permission
to guess a normalized logical-channel list. Exact configured/raw source roster
needs its own published-source contract.

[World SSHandler.cpp](../../../../../Server/TWorldSvr/SSHandler.cpp):592–699
registers that word and the channel bytes, then sends local/mission state,
conditional SKYGARDEN, BOW/BR state, Relay connectivity, events, cash sale,
castle-applicant, month-rank and tournament bootstrap. It emits no Map-specific
registration ACK. Relay RW acknowledgment is a different source protocol.

Replacing the current extension with only a partial MW_CONNECT request would
hide missing bootstrap/registries and invalidate the native admission proof.
This slice therefore repairs the envelope and shared parser while leaving the
original admission producer, roster, bootstrap consumers and completion proof
explicitly OPEN. Source server framing correctness is not full Map startup or
gameplay parity. Relay/other server transport profiles outside this task remain
separately scoped.

## Required later acceptance — not performed

1. Capture real modern Map↔World and source plaintext frames: header16, fresh
   metadata0, empty and fragmented bodies, total65534 accepted and65535 rejected.
   Nonzero received number/checksum must remain ignored, including source Copy.
2. Verify shared RW catalog fields with DWORD keys0/high-bit values, full WORD
   counts, truncated operator IDs, truncated message IDs/strings, negative or
   overrun lengths, trailing bytes and wrong ACK opcode. No malformed body may
   open IsRegistered or cancel the registration deadline.
3. Observe no admission from connect/completed write, deadline close for missing
   ACK, full ACK before event replay, connection-generation reset and unported
   event rejection. A source MW_CONNECT server must not be presented as already
   compatible with the retained RW extension.
4. Race concurrent large composed writes, queued old-socket sends, loss during
   an awaited handler and replacement registration. Verify intact ordering and
   no bytes/replayed writes on the replacement; native client persistence drain
   must precede redial, including failed-save retention.
5. Check all current production body families: character load/admission, World
   chart/event/catalog startup, movement/replicas, transfer/save/logout,
   social/soulmate, chat/TMS/notice/ban and Control↔World. Their body contracts and
   operation receipts must survive the common header replacement.
6. Exercise a valid near-limit CS request whose constructed MW relay grows beyond
   body65518. Known overflow must drop without truncation/client close/requeue;
   size refusal after a prepared durable operation must follow that operation's
   exact existing failure/reconciliation fence and create no false success.

Map test_world_reconnect historically wrote a STRING message key; the parent
statically aligned it to the actual DWORD key. The parent also aligned the
WorldSession framing used by test_world_handshake and the World raw-frame
fixtures. These edits are static alignment, not verification: no fixture was
executed. Full source registration/bootstrap, existing high-bit
generic Map claim binding and every runtime/stock-client acceptance remain OPEN.
