# Basic native parties — Phase A, 2026-10-14

Working baseline: `2e406dc`, local `main`. The owner renewed **local Git only**:
no GitHub push or deployment. This slice is **INTEGRATED_UNVERIFIED**, based on
static source/SQL-text/API/diff review. No build, test, executable, server, client,
container, SQL, migration, importer or runtime acceptance was executed. Applied
001–043 and written044–069 are unchanged; next unused migration is070. Original
server/client/libraries/protocol and backups remain read-only. Pre-existing
`_rewrite/docs/audits/` remains outside Git. Full Phase A is still incomplete.

## Actual implementation

- Map's real CS dispatcher now relays ADD/JOIN/DEL through original MW packets;
  World uses its process-local source party registry (maximum seven members).
  The actual World-to-Map dispatcher consumes ADD, JOIN, DEL, ATTR and MANSTAT,
  projects primary/replica state and sends original client bodies.
- World preserves country11, chief/full/arena refusal, ordered roster fan-out,
  first-other chief succession and two-member recursive dissolution. CloseChar
  removes party membership before removing the character. Backlinks and membership
  decisions precede suspended network writes; source packet order is retained.
- Invitations are bound to exact current character objects, session keys, peers,
  Map owner tokens, connections, authority epochs and original origin/obtain.
  One current invitation is consumed once; mismatched origin replies cannot consume
  another pending invite. Close and handoff invalidate affected invitations.
- Native actor reads prove live World/Map ownership, account/claim/current-login,
  ready primary, canonical lossless wire name and actual guildlessness. Each
  existing member is preflighted before membership mutation. Pairwise proofs lock
  both Map/account/claim participants in one sorted transaction; both actors are
  rechecked after SQL suspension and again between paired JOIN writes. A stopped
  database path never falls back to adapter success or replays a write.
- Native Map projection serializes against admission/checkpoint/gameplay/transfer
  and teardown. It re-reads phase after acquiring the lease. ATTR retains actual
  internal membership and masks SOLO1 getters to party/chief/commander0 for clients.
  DEL uses the source's old cached client tail until the later ATTR. Neighbor marks
  require actual current claim, same map/channel and source adjacent cells.
- Native own-PC HPMP enqueues and independent level-up producers send source
  MANSTAT with actual packet/intermediate bars. World validates the subject and
  recipients and keeps level owned by LEVELUP. Source normal/BOW party chat group4
  preserves full text, source type/operator/flood/ban/protectedCHAT1 behavior and
  derives its target from the effective Map getter. World serializes the audience
  with join/leave/close and authorizes both sender and recipient. BR's independent
  local team-chat route remains OPEN.
- The final CHECKMAIN winner marks a server-only pending party refresh. After the
  actual client CONREADY SQL commit and local Ready transition, Map sends an
  existing MANSTAT with real supported bars, even when its cached effective party
  is zero. World first proves ready authority, then republishes actual registry
  ATTR to primary/ready replicas, before accepting status only for the real party.
  No party field, new opcode or fabricated ready ACK is added to transfer.
  This is an explicit modern status probe: source HPMP only sent MANSTAT for a
  nonzero effective party, and original CONREADY did not emit this probe.
- Native dispatch rejects the still-unported party/corps/solo-instance/arena
  mutators, preventing them from bypassing the new membership gate. Their legacy
  adapters and all original responsibilities remain in scope.
- World monitoring now retains the historical unique admitted-account set through
  logout, matching source `MW_CHARINFO_ACK` insertion and `CT_DATACLEAR_ACK` reset.
  This corrects the prior CloseChar erase; the existing logout fixture expectation
  is statically aligned, never executed.

## Source proof

Detailed source mappings and review findings are in [analysis.md](analysis.md)
and [map-party.md](map-party.md). Key authorities:

| Contract | Original source |
| --- | --- |
| CS request parsing and original MW encoding | Map `CSHandler.cpp:3419–3504`, `SSSender.cpp:2671–2753` |
| World ADD/JOIN/DEL/MANSTAT decisions | World `SSHandler.cpp:2486–2899` |
| ATTR, ordered joins, succession/dissolution, close order | World `TWorldSvr.cpp:2882–3084` |
| Map party receipt/client projection | Map `SSHandler.cpp:7839–8075`, `10429–10505` |
| Roster class BYTE, commander WORD and DEL tail | World `SSSender.cpp:716–768`; client `CSHandler.cpp:4937–5019` |
| SOLO effective party0 and chief/commander0 | Map `TObjBase.cpp:2240–2243`, `TPlayer.cpp:1448–1457` |
| HPMP and independent level MANSTAT | Map `CSSender.cpp:1315–1335`, `TPlayer.cpp:1323–1330` |
| Normal World party chat and BR local team exception | World `SSHandler.cpp:5431–5459`; Map `CSHandler.cpp:5304–5325` |
| Native ready boundary without World ready ACK | Map `CSHandler.cpp:402–415`; modern `handlers/session.cpp` |
| Historical World account counter/reset | World `SSHandler.cpp:722–724`, `135–145` |

Parties are process-local source objects, not an imported historical party table.
Only the written World runtime grant adds explicit SELECT on existing
`app_world.guild_membership`; no database/schema/data change was executed.

## Deliberate differences and remaining limits

Strict full-body/EOF/NUL/enum/bar validation and current SQL/socket/generation
proofs reject stale or malformed traffic that source trusted. Source receive
metadata and wire widths remain unchanged. Source's synchronous batch ordering
is retained across DB/network suspensions with coroutine permits; no mutex spans
an await. SQL proofs commit before cache/network work and are repeated across
awaits; they do not establish an atomic database-to-client delivery transaction.

Unsupported native statistics prevent successful ADD/JOIN. An authorized target
can return source BUSY with four zero **absence fields**, consumed only as failure;
those fields never create a successful roster. The post-ready refresh requires
the same supported real statistics. Missing stats are an OPEN dependency, never
a successful substitute. Three-second projection timeout closes via ordinary
teardown instead of leaving stale membership usable; transport failures are not
replayed. Invitation invalidation on close/handoff clears current pending state;
it does not claim a new guaranteed inviter failure notification. If an actor
becomes stale after membership publication but before complete JOIN fan-out, the
joining character undergoes ordinary coordinated close after releasing the party
permit. Existing teardown sends the source leave/dissolution projections; partial
rosters are never logged as successful joins or replayed.

The source JOIN answer has no invitation nonce. A delayed response and a newer
otherwise identical invite in the same live generations are indistinguishable.
One-use fencing therefore does not prove complete invitation replay closure.
Post-ready ATTR repairs membership/type/chief, not a complete replay of the client
roster lost during handoff. Full roster reconciliation remains OPEN. ATTR
received before a secondary reaches actual SQL/local Ready can also be dropped;
secondary post-ready membership reconciliation remains OPEN. This slice proves
projection for already-ready replicas, not complete replica admission recovery.

Original `MW_CONNECT_ACK` Map admission and full World local/mission/BOW/BR/Relay/
event/cash/castle/tactics/monthly-rank/tournament bootstrap remain **OPEN**. Source
has no registration ACK and ambiguous BYTE count/key casts. The existing modern
RW-as-Map extension is retained; native sockets additionally pin their live Map
owner token. Source Relay registration is not accepted as a native Map identity.

Party-aware equipment/stat refresh, loot distribution, XP/monster/reward/soulmate
benefits, guild-bearing actors, manual chief/type/move/corps commands, arenas,
dynamic INDUN exit, complete BOW/BR modes, native monster AI/REVIVAL, high-bit
generic identity parity, complete World bootstrap and all original responsibilities
remain required. Constructor-zero arena state is not evidence of a complete
arena registry. Existing equipment guards are retained. Earlier successful
build/test evidence does not validate these additions.

## Deferred Phase B acceptance

After the implementation gate and later execution authorization, verify original
client ADD→dialog→accept/deny/busy; forged/stale/wrong-origin/repeated replies;
owner loss/replacement/key/connection/epoch changes; country11, capacity and all
six obtain modes; protected2 and dead/TRANS/BLOCK/HOLD; roster field widths/order;
3+ member succession and two-member dissolution; chief kick/self leave/logout;
primary/replica ATTR and source old DEL tail; own/level MANSTAT exact intermediate
values; full-text party chat/protected1/SOLO/partyless/forged target and concurrent
audience changes; admission/checkpoint/transfer/CONREADY/close races; pending
post-ready refresh with missing real statistics; roster reconciliation; nonce
ambiguity, frame caps, uncertain outcomes and World disconnect drain. No such
execution has occurred in this slice.
