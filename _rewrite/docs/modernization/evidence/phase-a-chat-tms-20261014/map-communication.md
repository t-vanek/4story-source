# Map communication continuation — 2026-10-14

Status: **IMPLEMENTED / INTEGRATED_UNVERIFIED**. This record is a static source
comparison and a later acceptance plan. No build, test, server, client, migration,
import, extraction, or runtime verification was run. `git diff --check` passed.

## Implemented entry and response paths

| Original contract | Native Map continuation |
| --- | --- |
| [CS chat request](../../../../../Server/TMapSvr/CSHandler.cpp), lines5206–5526; [client sender](../../../../../Client/TClient/TRelaySender.cpp), lines72–102 | [communication handler](../../../../../Server/TMapSvrAsio/handlers/communication.cpp), `OnChatReq` and `DeliverChat`; actual CS/MW dispatch, local primary/replica delivery, World relay and response |
| [TMS requests](../../../../../Server/TMapSvr/CSHandler.cpp), lines8562–8634; [client fields](../../../../../Client/TClient/CSSender.cpp), lines742–778 | Same native handler, `OnTmsReq`: SEND/INVITE/OUT preserve the original char/key prefix and exact client suffix |
| [World TMS responses](../../../../../Server/TMapSvr/SSHandler.cpp), lines11962–12045; [client decoding](../../../../../Client/TClient/CSHandler.cpp), lines9551–9627 | `OnWorldCommunication`: RECV/INVITE/OUT strip only the two DWORD identity fields; INVITEASK preserves the reversed identity pair and SQL protected INVITE2 result |
| [World chat fanout](../../../../../Server/TMapSvr/SSHandler.cpp), lines9618–9711; [CS chat sender](../../../../../Server/TMapSvr/CSSender.cpp), lines2621–2640 | `WorldChat`: primary recipients, original CanTalk direction, operator/normal display type and recipient protected CHAT1 |
| [Operator chat-ban request](../../../../../Server/TMapSvr/CSHandler.cpp), lines16518–16540; [ban projection](../../../../../Server/TMapSvr/SSHandler.cpp), lines2222–2255 | `OnChatBanReq` and `WorldChatBan`: verified operator gate before decoding, all WORD minute values, source response bytes, current SQL deadline and exact issuer receipt |
| [Targeted administrative notice](../../../../../Server/TMapSvr/SSHandler.cpp), lines15524–15541 | `WorldNotice`/`DeliverNotice`: primary target, group7/sender0, original published NAME_OPERATOR6 and BuildNetString(empty, full message) |
| [Event notice](../../../../../Server/TMapSvr/SSHandler.cpp), lines16026–16048 | `WorldEventNotice`/shared `DeliverNotice`: BYTE event/type plus STRING, primary broadcast as group3/sender0; the BYTE fields do not mutate gameplay events |

Additional source semantics: [TPlayer.cpp](../../../../../Server/TMapSvr/TPlayer.cpp)
lines5472–5481 protected bits, 6294–6332 CanTalk, 6336–6412 CheckChat;
[TMap.cpp](../../../../../Server/TMapSvr/TMap.cpp) lines770–807 neighboring 3x3
cells; [NetCode.h](../../../../../Lib/Own/TProtocol/include/NetCode.h)
country D0/C1/B2/N3/PEACE4, CELL_SIZE64, UNIT_SIZE1024 and meeting maps1100..1105.

## Authority, persistence and intentional repairs

[PostgreSQL communication authority](../../../../../Server/TMapSvrAsio/services/postgresql_map_communication.cpp)
fences the current Map owner, account, exact primary or recipient replica claim,
current-user key/channel and ready live primary owner. Client chat/TMS requests
and World targeted fanout remain primary-only. A local neighboring replica uses
its real replica claim and hydrated cluster country; no complete payload or
protected list is invented. Both CHAT1 and INVITE2 are read from their native
SQL rows while that authority is held, rather than trusting a cache that may
still be publishing a committed protected-list change.

Chat requires the verified operator publication introduced by written migration069.
An explicit verified empty archive is valid; a missing publication or an altered
native operator projection is rejected. Source operator display type is7. Native
fresh character names now use [lossless explicit wire encoding](../../../../../Server/TMapSvrAsio/services/native_wire_text.h)
instead of sending PostgreSQL UTF8 directly as an inferred ANSI name.

CheckChat retains DWORD timing, the low WORD of the first hexadecimal text
prefix, ten-second aggregation and the original regular-message history rule.
Its FALSE result is ignored by the original caller: a five-minute async ban
request alone does not drop the current message. Sender spoofing requests30000
minutes. Active ban output is only BYTE10+DWORD seconds. TMS does not inherit
chat-ban or CheckChat gates. The discarded `Left(1024)` return value does not
truncate any forwarded text.

Durable bans are a deliberate modernization of the source World memory map.
Incoming target projections compare the exact currently stored SQL deadline,
including expired deadlines and an absent/unbanned zero. They can update loaded,
admitted or ready primary snapshots. [CONREADY](../../../../../Server/TMapSvrAsio/handlers/session.cpp)
now acquires its exclusive primary admission operation before snapshot sampling
and MarkReady, closing the old wait/MarkReady/later-checkpoint race with a late
World projection. Unchanged ban data is not a replay of a database write.

An operator-success reply additionally requires an audit row for the same issuer,
session key, source server, Map owner token, connection generation, authority
epoch, target wire name and deadline under the current World owner. Stale replies
cannot create a replacement success. Source's unsafe GM lookup that ignored the
key and could dereference a missing character is replaced with the current claim.
Ban state is compared against the target name, not the issuing GM's own name.
Actual failed World writes or unavailable registered links close the requesting
session; no write is replayed. Known SS envelope overflow is checked before a
write: the original eight-byte header permits a complete frame of at most65535
bytes, so body65527 is allowed and body65528 is logged and discarded without
closing the client. A valid near-limit CS chat can gain ten body bytes in the MW
relay and exceed this limit; no1024-byte truncation substitutes for rejection.

Both notice paths read original NAME_OPERATOR6 bytes from the existing verified
059 server-message publication. No translated/default operator name is supplied.
The caption must fit the original256-byte width and pass lossless validation
under the explicit client encoding profile; validation does not rewrite its bytes.
They format an empty-header net string with eight uppercase hex length bytes and
the full original message. Sender0 still checks protected CHAT1, while recipient
chat bans, country filters and CheckChat are intentionally absent.

Every `DeliverChat` output checks its complete CS envelope before queuing: BYTE
group, DWORD sender, both INT32 string lengths and their actual string bytes.
This includes a whisper echo whose actual target name may be longer than the
requested name. Notices additionally count eight net-string bytes and the full
plain message. Each output must satisfy `body < kMaxPacketSize - kPacketHeaderSize`
(65535 minus the16-byte header). A message that cannot fit is logged and not
delivered; it is neither truncated nor treated as a successful delivery, and the
recipient is not closed merely for this known codec limit.

[AsioSession::SendPacket](../../../../../Lib/Own/TNetLib/TNetLib/asio_session.h)
returns `awaitable<void>` after attempting to enqueue, not an application delivery
receipt. Local routing separately records a pre-enqueue unavailable target,
handled routing (including source protected-recipient suppression), or a stopped
route. A closed/replaced target after enqueue cannot trigger a World fallback or
repeat the message. Source whisper echo remains independent of protected-target
suppression; a stopped route does not create a substitute success.

## Required later acceptance — not performed

1. Verify boot/schema and permissions for069, then verify a missing or drifted
   operator publication rejects chat. The actual verified empty publication must
   still allow ordinary chat and return source NOTOPERATOR1 before decoding a
   malformed GM request. Do not manufacture an operator by a name heuristic.
2. Compare original client bytes for each chat/TMS request and response, including
   the near WORD count tail, variable strings, empty TMS invitation count and
   complete BYTE-counted rosters. Observe no invented client ACK for silent
   unavailable/stale recipients.
3. Check near cell boundaries and the3x3 selection independently of client-provided
   IDs, including current native replicas, meeting maps1100..1105, source unit3
   countryB exception, aid/neutral/peace country directions and cross-channel
   local whispers. Verify local protected-target suppression still permits the
   sender echo and does not produce a second World relay. Close or replace the
   target while enqueue awaits; confirm that the attempted local write is never
   repeated through World. Verify complete-frame bounds when an actual whisper
   target name grows the sender echo, and World SS body65527/body65528 boundaries.
4. Exercise CheckChat aggregation and regular-history bans with controlled DWORD
   observations; verify the async request does not independently discard the
   initiating message. Verify active ban BYTE10+DWORD seconds and spoof30000.
   Confirm TMS still works while the participant is chat-banned.
5. Race protected CHAT1/INVITE2 commits against inbound/outbound communication.
   Verify SQL-authoritative suppression before delayed cache publication, exact
   current key/owner/epoch checks, and same-login Map transfer/replica retirement.
6. Exercise TMS creation, deduplication, invitation, same-country membership,
   one-member rejoin protection, source message5 fallback, OUT and logout while
   invite/fanout waits. Verify World recipient headers and pending reply guards;
   no stale roster may recreate a logged-out participant.
7. Exercise ban/unban accumulation, explicit all-WORD minute values, relogin and
   World restart restoration, loaded/admitted CONREADY races, old deadline
   callbacks and old issuer connection/epoch receipts. After an unknown COMMIT,
   reconcile the receipt without repeating the write.
8. Deliver targeted and event notices with long text above1024 bytes, original
   caption6, empty text and protected sender0; compare complete group7/group3
   frames. For actual caption length C and plain-message length M, body length
   is21+C+M. Verify65518 bytes are admitted and65519 bytes are rejected without
   truncation or closing the target. Event BYTE changes must not mutate gameplay
   event bonuses or catalogs.

## Explicitly open scope

Normal external TRelay transport/registries, native mutable party/corps/guild/
tactics request targets, BOW peace-state cross-country broadcast, BR team-local
chat, remaining administrative/control paths and general event gameplay remain
open. Available independent local/World chat and TMS branches do not establish
these registries with zero defaults. Direct client Control announcement dispatch
is not promoted into an authenticated native administrative producer here.

The existing generic Map LockAccount/LockClaim positive-bigint bindings still
prevent a claim of full high-bit archival INT/DWORD ID parity. Exact packet,
concurrency, failure and stock-client acceptance remain REQUIRED_VERIFICATION.
