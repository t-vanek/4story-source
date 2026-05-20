// F1 — minimal wire dispatch.
//
// Implements the single happy-path handler `OnCS_CONNECT_REQ`:
//   1. Parse the legacy body (WORD wVersion, BYTE bChannel, DWORD
//      dwUserID, DWORD dwID, DWORD dwKEY, DWORD dwIPAddr, WORD wPort,
//      INT64 llChecksum).
//   2. Reject anything where wVersion isn't in `accepted_versions` —
//      reply CS_CONNECT_ACK { CN_INVALIDVER, 0 } and close. Matches
//      legacy CSHandler.cpp:258-262.
//   3. Validate the (dwUserID, dwID, dwKEY) token against the SOCI-
//      backed TCURRENTUSER row via IMapSessionValidator (or the
//      configured fake). On miss → CN_INTERNAL, close.
//   4. Stash the validated (user, char, channel) on the session state
//      and reply CS_CONNECT_ACK { CN_SUCCESS, 0 } (empty server-id
//      list — gameplay-tier server-id push lands in later phases).
//
// Every other packet id is logged at debug level and dropped — the
// 620 remaining handlers ship in F2..Fn (see README roadmap).
//
// Wire shape references:
//   * Server/TMapSvr/CSHandler.cpp:249  — OnCS_CONNECT_REQ
//   * Server/TMapSvr/CSSender.cpp:78    — SendCS_CONNECT_ACK
//   * Lib/Own/TProtocol/include/NetCode.h:319-328 — CN_* enum

#include "handlers.h"
#include "handlers_combat.h"
#include "wire_codec.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>

namespace tmapsvr {

using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToMessageId;
using tnetlib::protocol::ToUint16;

namespace {

// Legacy NetCode.h::TCONNECT_RESULT — duplicated here as plain
// constants so the modern code doesn't pull the legacy ATL/MFC-tainted
// TProtocol headers into the Asio translation unit.
constexpr std::uint8_t CN_SUCCESS      = 0;
constexpr std::uint8_t CN_NOCHANNEL    = 1;
constexpr std::uint8_t CN_NOCHAR       = 2;
constexpr std::uint8_t CN_ALREADYEXIST = 3;
constexpr std::uint8_t CN_INVALIDVER   = 4;
constexpr std::uint8_t CN_INTERNAL     = 5;

boost::asio::awaitable<void>
SendConnectAck(std::shared_ptr<tnetlib::AsioSession> sess,
               std::uint8_t result)
{
    // Legacy CSSender.cpp:78 — CS_CONNECT_ACK = { bResult, bSvrCount,
    // <bSvrCount * BYTE> }. F1 always sends bSvrCount=0 (no per-tier
    // server-id push); CN_SUCCESS still lets the client transition out
    // of the "connecting" state, which is what gameplay needs.
    std::vector<std::byte> body;
    body.reserve(2);
    wire::WritePOD<std::uint8_t>(body, result);
    wire::WritePOD<std::uint8_t>(body, 0);  // bSvrCount == 0
    co_await sess->SendPacket(
        ToUint16(MessageId::CS_CONNECT_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

} // namespace

boost::asio::awaitable<void>
OnConnectReq(std::shared_ptr<tnetlib::AsioSession> sess,
             MapSessionState&                    state,
             const tnetlib::DecodedPacket&       packet,
             const HandlerContext&               ctx)
{
    wire::Reader r(packet.body.data(), packet.body.size());
    std::uint16_t version = 0;
    if (!r.Read(version))
    {
        spdlog::warn("CS_CONNECT_REQ malformed (no version) from {}",
            sess->RemoteIPv4());
        co_return;  // close-on-malformed: drop the read loop next iter
    }

    const bool version_ok = ctx.accepted_versions &&
        std::find(ctx.accepted_versions->begin(),
                  ctx.accepted_versions->end(),
                  version) != ctx.accepted_versions->end();
    if (!version_ok)
    {
        spdlog::info("CS_CONNECT_REQ rejected version=0x{:04X} from {}",
            version, sess->RemoteIPv4());
        co_await SendConnectAck(sess, CN_INVALIDVER);
        sess->Close();
        co_return;
    }

    std::uint8_t  channel  = 0;
    std::uint32_t user_id  = 0;
    std::uint32_t char_id  = 0;
    std::uint32_t dw_key   = 0;
    std::uint32_t ip_addr  = 0;   // legacy client echoes its own IP — ignored server-side
    std::uint16_t port     = 0;
    std::int64_t  checksum = 0;
    if (!r.Read(channel) || !r.Read(user_id) || !r.Read(char_id) ||
        !r.Read(dw_key)  || !r.Read(ip_addr) || !r.Read(port)   ||
        !r.Read(checksum))
    {
        spdlog::warn("CS_CONNECT_REQ malformed tail from {}",
            sess->RemoteIPv4());
        co_return;
    }
    // Legacy checksum-of-checksum is wire-stamped against a magic
    // constant; F1 trusts the dwKEY → TCURRENTUSER lookup instead of
    // recomputing the legacy obfuscation. Wire parity for the byte
    // count is preserved (we read the field), so a legacy client's
    // packet is still consumed without leaving stray bytes in the body.
    (void)ip_addr;
    (void)port;
    (void)checksum;

    if (state.connected || state.user_id != 0)
    {
        // Duplicate CS_CONNECT_REQ on a session that already passed
        // the handshake — legacy treats this as a malicious client and
        // closes (CSHandler.cpp:301).
        spdlog::warn("CS_CONNECT_REQ duplicate on uid={} from {} — closing",
            state.user_id, sess->RemoteIPv4());
        sess->Close();
        co_return;
    }

    bool ok = false;
    if (ctx.validator)
    {
        MapSessionLookup lookup{};
        lookup.user_id = user_id;
        lookup.char_id = char_id;
        lookup.dw_key  = dw_key;
        lookup.channel = channel;
        ok = ctx.validator->Validate(lookup);
    }
    if (!ok)
    {
        spdlog::info("CS_CONNECT_REQ dwKEY mismatch uid={} char={} from {} "
                     "— sending CN_INTERNAL",
            user_id, char_id, sess->RemoteIPv4());
        co_await SendConnectAck(sess, CN_INTERNAL);
        sess->Close();
        co_return;
    }

    state.user_id   = user_id;
    state.char_id   = char_id;
    state.channel   = channel;
    state.connected = true;
    spdlog::info("CS_CONNECT_REQ accepted uid={} char={} ch={} from {}",
        user_id, char_id, channel, sess->RemoteIPv4());
    co_await SendConnectAck(sess, CN_SUCCESS);

    // Cluster path: send MW_ADDCHAR_ACK + register pending session so
    // MW_CONRESULT_REQ can route CS_CHARINFO_ACK back here (F2b Part 4).
    if (ctx.world_client)
    {
        // Try pre-loaded cache first: DM_LOADCHAR_REQ may have been
        // handled by AsioWorldClient BEFORE the client connected
        // (the normal cluster sequence).
        state.snapshot = ctx.world_client->TakePreloadedChar(
            char_id, user_id, dw_key);

        if (state.snapshot)
        {
            spdlog::info("CS_CONNECT_REQ uid={} char={} → snapshot "
                         "from world pre-load cache", user_id, char_id);
        }
        else
        {
            // Not pre-loaded yet (race or first connect).
            // Register this session so MW_CONRESULT_REQ can route back.
            ctx.world_client->RegisterPendingSession(
                char_id, sess, ctx.session_state_weak);

            // Notify WorldSvr that the client has connected.
            ctx.world_client->SendMwAddCharAck(
                char_id, dw_key,
                ip_addr,
                static_cast<std::uint16_t>(port),
                user_id);

            spdlog::info("CS_CONNECT_REQ uid={} char={} → MW_ADDCHAR_ACK "
                         "sent, waiting for MW_CONRESULT_REQ",
                user_id, char_id);
        }
        co_return;
    }

    // Standalone path: load character data immediately so
    // OnConReadyReq can send CS_CHARINFO_ACK without waiting for
    // the TWorldSvr round-trip.
    if (ctx.player_service)
    {
        state.snapshot = ctx.player_service->LoadChar(char_id, user_id, dw_key);
        if (state.snapshot)
            spdlog::info("player loaded char={} name='{}' map={} (standalone)",
                char_id, state.snapshot->name, state.snapshot->position.map_id);
        else
            spdlog::warn("player_service returned nullopt for "
                         "uid={} char={} — CHARINFO_ACK will be skipped",
                user_id, char_id);
    }
}

// SendCharInfoAck + OnConReadyReq + OnMoveReq are implemented in
// handlers_map.cpp (F3) alongside the AOI wire helpers. This TU only
// provides the Dispatch switch and the remaining non-map handlers.

boost::asio::awaitable<void>
OnWinLdicReq(std::shared_ptr<tnetlib::AsioSession> /*sess*/,
             MapSessionState&                    /*state*/,
             const tnetlib::DecodedPacket&       /*packet*/,
             const HandlerContext&               /*ctx*/)
{
    // CSHandler.cpp:9-25 — entire body commented out in legacy.
    // The wire id is reserved but produces no observable effect.
    co_return;
}

boost::asio::awaitable<void>
OnServiceMonitorAck(std::shared_ptr<tnetlib::AsioSession> sess,
                    MapSessionState&                    /*state*/,
                    const tnetlib::DecodedPacket&       packet,
                    const HandlerContext&               ctx)
{
    // CSHandler.cpp:31-35 — inbound is DWORD dwTick, outbound is
    // CT_SERVICEMONITOR_REQ with the four-DWORD count tuple. F2
    // collapses session/player/active-user to a single number
    // (matches the TLoginSvrAsio shape — the GUI mostly cares about
    // "how many users are connected right now"). The 5-second GC
    // sweep (CSHandler.cpp:37-65) is intentionally not ported here —
    // the per-session pre-auth watchdog + RAII teardown covers the
    // F2 equivalent.
    std::uint32_t tick = 0;
    if (packet.body.size() >= 4)
        std::memcpy(&tick, packet.body.data(), 4);

    const std::uint32_t live = ctx.live_session_count
        ? ctx.live_session_count()
        : 0u;

    std::vector<std::byte> reply;
    reply.reserve(16);
    wire::WritePOD<std::uint32_t>(reply, tick);
    wire::WritePOD<std::uint32_t>(reply, live);
    wire::WritePOD<std::uint32_t>(reply, live);
    wire::WritePOD<std::uint32_t>(reply, live);

    spdlog::info("CT_SERVICEMONITOR_ACK tick={} → CT_SERVICEMONITOR_REQ (live={})",
        tick, live);
    co_await sess->SendPacket(
        ToUint16(MessageId::CT_SERVICEMONITOR_REQ),
        std::span<const std::byte>(reply.data(), reply.size()));
}

boost::asio::awaitable<void>
OnQuitServiceReq(std::shared_ptr<tnetlib::AsioSession> /*sess*/,
                 MapSessionState&                    /*state*/,
                 const tnetlib::DecodedPacket&       /*packet*/,
                 const HandlerContext&               ctx)
{
    // SSHandler.cpp:14-17 — log the transition, optionally hand off
    // to the supplied shutdown callback (main wires this to
    // io.stop()). The Windows-only SetServiceStatus + WM_QUIT branch
    // collapses into a single callback — Linux services receive the
    // same effect via SIGTERM from systemd.
    spdlog::warn("SM_QUITSERVICE_REQ detected — initiating shutdown");
    if (ctx.on_quit_request)
        ctx.on_quit_request();
    co_return;
}

boost::asio::awaitable<void>
OnTerminateReq(std::shared_ptr<tnetlib::AsioSession> sess,
               MapSessionState&                    /*state*/,
               const tnetlib::DecodedPacket&       /*packet*/,
               const HandlerContext&               /*ctx*/)
{
    // CSHandler.cpp:14882 — legacy semantic is purely a telemetry log.
    // The original handler had a TLogoutAll magic-key check that an
    // attacker probing for backdoors would trigger; the shipped code
    // stripped the body, leaving just the log line + EC_NOERROR (no
    // ack, session stays alive). We keep the same shape.
    spdlog::warn("CS_TERMINATE_REQ (backdoor probe) from {}",
        sess ? sess->RemoteIPv4() : std::string{"<no-session>"});
    co_return;
}

boost::asio::awaitable<void>
OnCtrlSvrReq(std::shared_ptr<tnetlib::AsioSession> /*sess*/,
             MapSessionState&                    /*state*/,
             const tnetlib::DecodedPacket&       /*packet*/,
             const HandlerContext&               /*ctx*/)
{
    // CSHandler.cpp:230 — empty heartbeat from TControlSvr after a
    // successful dial. The IP-based authentication gate is upstream;
    // by the time we see this packet, the peer is already trusted.
    co_return;
}

boost::asio::awaitable<void>
OnServiceDataClearAck(std::shared_ptr<tnetlib::AsioSession> /*sess*/,
                      MapSessionState&                    /*state*/,
                      const tnetlib::DecodedPacket&       /*packet*/,
                      const HandlerContext&               /*ctx*/)
{
    // CSHandler.cpp:214 — rebuild m_mapACTIVEUSER from m_mapPLAYER.
    // F2 doesn't carry the derived active-user index (modern session
    // registry tracks one count); rebuild semantics ship with F3.
    spdlog::info("CT_SERVICEDATACLEAR_ACK received "
                 "(active-user rebuild pending F3)");
    co_return;
}

boost::asio::awaitable<void>
OnAnnouncementAck(std::shared_ptr<tnetlib::AsioSession> /*sess*/,
                  MapSessionState&                    /*state*/,
                  const tnetlib::DecodedPacket&       packet,
                  const HandlerContext&               /*ctx*/)
{
    // CSHandler.cpp:72-89 — GM global announcement. F2 reads + logs
    // the message (mirroring legacy's `strAnnounce.Left(ONE_KBYTE)`
    // truncation defensive). Broadcast to every in-game player is F3.
    wire::Reader r(packet.body.data(), packet.body.size());
    std::string announcement;
    if (!r.ReadString(announcement))
    {
        spdlog::warn("CT_ANNOUNCEMENT_ACK malformed body");
        co_return;
    }
    if (announcement.size() > 1024)
        announcement.resize(1024);
    spdlog::info("CT_ANNOUNCEMENT_ACK: '{}' (broadcast pending F3)",
                 announcement);
    co_return;
}

boost::asio::awaitable<void>
OnUserKickoutAck(std::shared_ptr<tnetlib::AsioSession> /*sess*/,
                 MapSessionState&                    /*state*/,
                 const tnetlib::DecodedPacket&       packet,
                 const HandlerContext&               /*ctx*/)
{
    // CSHandler.cpp:92-110 — GM kick by name. F2 reads + logs the
    // target name (matches the legacy LogEvent "TCONTROL - FUNCTION
    // DEBUG: %s"). By-name player lookup + actual kick is F3.
    wire::Reader r(packet.body.data(), packet.body.size());
    std::string user_name;
    if (!r.ReadString(user_name))
    {
        spdlog::warn("CT_USERKICKOUT_ACK malformed body");
        co_return;
    }
    // Legacy upper-cases the string before the lookup; the same
    // canonicalization belongs in the future by-name index.
    for (auto& ch : user_name)
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    spdlog::info("CT_USERKICKOUT_ACK target='{}' (kick pending F3)",
                 user_name);
    co_return;
}

boost::asio::awaitable<void>
OnArenaRankingReq(std::shared_ptr<tnetlib::AsioSession> /*sess*/,
                  MapSessionState&                    state,
                  const tnetlib::DecodedPacket&       /*packet*/,
                  const HandlerContext&               /*ctx*/)
{
    // CSHandler.cpp:21106 — gated on m_pMAP/m_bMain. Modern F2
    // doesn't have those yet, so the gate trips for every request
    // (matches legacy's pre-CONNECT behavior: silent EC_NOERROR).
    // F3 swaps the gate to `state.connected` once IMapState lands.
    if (!state.connected)
        co_return;
    spdlog::debug("CS_ARENARANKING_REQ from uid={} (in-game ranking pending F3)",
        state.user_id);
    co_return;
}

boost::asio::awaitable<void>
OnDisconnectReq(std::shared_ptr<tnetlib::AsioSession> sess,
                MapSessionState&                    /*state*/,
                const tnetlib::DecodedPacket&       /*packet*/,
                const HandlerContext&               /*ctx*/)
{
    // CSHandler.cpp:12138 — client says goodbye, server closes the
    // socket. The read loop sees the close on its next iteration
    // and tears the session down through the same path as peer-EOF.
    spdlog::info("CS_DISCONNECT_REQ — closing session from {}",
        sess ? sess->RemoteIPv4() : std::string{"<no-session>"});
    if (sess) sess->Close();
    co_return;
}

boost::asio::awaitable<void>
OnVerifySessionAck(std::shared_ptr<tnetlib::AsioSession> /*sess*/,
                   MapSessionState&                    state,
                   const tnetlib::DecodedPacket&       packet,
                   const HandlerContext&               /*ctx*/)
{
    // CSHandler.cpp:19649-19659 — read dwCharID (legacy ignores it
    // server-side, just logs "Verified"), flip the per-session bit.
    // The bit gates GM-protected actions in legacy; F2 just tracks
    // it for parity, the gates land with the GM admin handlers.
    wire::Reader r(packet.body.data(), packet.body.size());
    std::uint32_t echoed_char_id = 0;
    (void)r.Read(echoed_char_id);
    state.session_verified = true;
    spdlog::info("CS_VERIFYSESSION_ACK uid={} char={} → session_verified=true",
        state.user_id, echoed_char_id);
    co_return;
}

boost::asio::awaitable<void>
OnPingMeasurementReq(std::shared_ptr<tnetlib::AsioSession> sess,
                     MapSessionState&                    state,
                     const tnetlib::DecodedPacket&       packet,
                     const HandlerContext&               /*ctx*/)
{
    // CSHandler.cpp:19662 — gated on m_pMAP (in-game only). Reads
    // DWORD dwTick, echoes back CS_PINGMEASUREMENT_ACK { dwTick }.
    // F2 honors the gate via state.connected.
    if (!state.connected)
        co_return;
    std::uint32_t tick = 0;
    if (packet.body.size() >= 4)
        std::memcpy(&tick, packet.body.data(), 4);

    std::vector<std::byte> reply;
    wire::WritePOD<std::uint32_t>(reply, tick);
    co_await sess->SendPacket(
        ToUint16(MessageId::CS_PINGMEASUREMENT_ACK),
        std::span<const std::byte>(reply.data(), reply.size()));
}

boost::asio::awaitable<void>
OnRegisterBowReq(std::shared_ptr<tnetlib::AsioSession> /*sess*/,
                 MapSessionState&                    state,
                 const tnetlib::DecodedPacket&       /*packet*/,
                 const HandlerContext&               /*ctx*/)
{
    // CSHandler.cpp:19678 — gated on m_pMAP/m_bMain/!IsTournamentPlayer.
    // Forwards MW_ADDTOBOWQUEUE_REQ(dwID, dwKEY) to World. F2's gate
    // matches `state.connected`; the World forward is PENDING F2b.
    if (!state.connected)
        co_return;
    spdlog::debug("CS_REGISTERBOW_REQ uid={} (MW forward pending F2b)",
        state.user_id);
    co_return;
}

boost::asio::awaitable<void>
OnCancelBowQueueReq(std::shared_ptr<tnetlib::AsioSession> /*sess*/,
                    MapSessionState&                    state,
                    const tnetlib::DecodedPacket&       /*packet*/,
                    const HandlerContext&               /*ctx*/)
{
    // CSHandler.cpp:19691 — twin of REGISTERBOW; forwards
    // MW_CANCELBOWQUEUE_REQ(dwID, dwKEY) to World. F2 gates locally,
    // forward is PENDING F2b.
    if (!state.connected)
        co_return;
    spdlog::debug("CS_CANCELBOWQUEUE_REQ uid={} (MW forward pending F2b)",
        state.user_id);
    co_return;
}

boost::asio::awaitable<void>
OnChgModeReq(std::shared_ptr<tnetlib::AsioSession> /*sess*/,
             MapSessionState&                    state,
             const tnetlib::DecodedPacket&       packet,
             const HandlerContext&               /*ctx*/)
{
    // CSHandler.cpp:1220 — gate, then read BYTE bMode + call
    // pPlayer->ChgMode (which broadcasts the mode change to AOI).
    // F2 reads + logs; the broadcast lands with F3 cell-grid.
    if (!state.connected)
        co_return;
    std::uint8_t mode = 0;
    if (packet.body.size() >= 1)
        mode = static_cast<std::uint8_t>(packet.body[0]);
    spdlog::debug("CS_CHGMODE_REQ uid={} mode={} (AOI broadcast pending F3)",
        state.user_id, mode);
    co_return;
}

boost::asio::awaitable<void>
OnChgChannelReq(std::shared_ptr<tnetlib::AsioSession> /*sess*/,
                MapSessionState&                    state,
                const tnetlib::DecodedPacket&       packet,
                const HandlerContext&               /*ctx*/)
{
    // CSHandler.cpp:12145 — gate, read BYTE bChannel, no-op if same,
    // otherwise initiate channel-switch flow (which involves a World
    // round-trip + AOI cleanup). F2 reads + logs the no-op short-
    // circuit; full flow PENDING F2b+F3.
    if (!state.connected)
        co_return;
    std::uint8_t target_channel = 0;
    if (packet.body.size() >= 1)
        target_channel = static_cast<std::uint8_t>(packet.body[0]);
    if (target_channel == state.channel)
    {
        spdlog::debug("CS_CHGCHANNEL_REQ uid={} same channel ({}) — no-op",
            state.user_id, target_channel);
        co_return;
    }
    spdlog::debug("CS_CHGCHANNEL_REQ uid={} {}→{} (switch pending F2b)",
        state.user_id, state.channel, target_channel);
    co_return;
}

boost::asio::awaitable<void>
OnCorpsAskReq(std::shared_ptr<tnetlib::AsioSession> /*sess*/,
              MapSessionState&                    state,
              const tnetlib::DecodedPacket&       packet,
              const HandlerContext&               /*ctx*/)
{
    // CSHandler.cpp:8092 — gate, read CString strName, forward
    // MW_CORPSASK_ACK(dwID, dwKEY, strName) to World. F2 reads the
    // target name + logs; forward PENDING F2b.
    if (!state.connected)
        co_return;
    wire::Reader r(packet.body.data(), packet.body.size());
    std::string target_name;
    if (!r.ReadString(target_name))
    {
        spdlog::warn("CS_CORPSASK_REQ malformed body");
        co_return;
    }
    spdlog::debug("CS_CORPSASK_REQ uid={} target='{}' (MW forward pending F2b)",
        state.user_id, target_name);
    co_return;
}

boost::asio::awaitable<void>
OnHeroListReq(std::shared_ptr<tnetlib::AsioSession> /*sess*/,
              MapSessionState&                    state,
              const tnetlib::DecodedPacket&       /*packet*/,
              const HandlerContext&               /*ctx*/)
{
    // CSHandler.cpp:14862 — gate, then SendCS_HEROLIST_ACK (the
    // sender pulls per-player hero data). F2 trips the gate; full
    // hero-list ACK requires F3 player state.
    if (!state.connected)
        co_return;
    spdlog::debug("CS_HEROLIST_REQ uid={} (hero-list ack pending F3)",
        state.user_id);
    co_return;
}

boost::asio::awaitable<void>
OnPetCancelReq(std::shared_ptr<tnetlib::AsioSession> /*sess*/,
               MapSessionState&                    state,
               const tnetlib::DecodedPacket&       /*packet*/,
               const HandlerContext&               /*ctx*/)
{
    // CSHandler.cpp:15443 — NOT gated on m_pMAP in legacy, but
    // FindRecallPet() returns null for un-initialized sessions
    // (which is functionally the same as the gate). Sub-branches:
    //   * pet recall record exists  → MW_RECALLMONDEL_ACK to World
    //   * none found                → CS_PETRECALL_ACK(PET_FAIL)
    // F2 logs only — both branches need F3 pet state + F2b world
    // forward.
    if (!state.connected)
        co_return;
    spdlog::debug("CS_PETCANCEL_REQ uid={} (pet state pending F3)",
        state.user_id);
    co_return;
}

boost::asio::awaitable<void>
Dispatch(std::shared_ptr<tnetlib::AsioSession> sess,
         MapSessionState&                    state,
         const tnetlib::DecodedPacket&       packet,
         const HandlerContext&               ctx)
{
    const auto id = ToMessageId(packet.wId);
    switch (id)
    {
    case MessageId::CS_CONNECT_REQ:
        co_await OnConnectReq(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CS_CONREADY_REQ:
        co_await OnConReadyReq(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CS_MOVE_REQ:
        co_await OnMoveReq(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CS_ACTION_REQ:
        co_await OnActionReq(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CS_DEFEND_REQ:
        co_await OnDefendReq(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CS_SKILLUSE_REQ:
        co_await OnSkillUseReq(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CS_TERMINATE_REQ:
        co_await OnTerminateReq(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CS_WINLDIC_REQ:
        co_await OnWinLdicReq(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CT_SERVICEMONITOR_ACK:
        co_await OnServiceMonitorAck(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CT_CTRLSVR_REQ:
        co_await OnCtrlSvrReq(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CT_SERVICEDATACLEAR_ACK:
        co_await OnServiceDataClearAck(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CT_ANNOUNCEMENT_ACK:
        co_await OnAnnouncementAck(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CT_USERKICKOUT_ACK:
        co_await OnUserKickoutAck(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CS_ARENARANKING_REQ:
        co_await OnArenaRankingReq(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CS_DISCONNECT_REQ:
        co_await OnDisconnectReq(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CS_VERIFYSESSION_ACK:
        co_await OnVerifySessionAck(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CS_PINGMEASUREMENT_REQ:
        co_await OnPingMeasurementReq(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CS_REGISTERBOW_REQ:
        co_await OnRegisterBowReq(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CS_CANCELBOWQUEUE_REQ:
        co_await OnCancelBowQueueReq(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CS_CHGMODE_REQ:
        co_await OnChgModeReq(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CS_CHGCHANNEL_REQ:
        co_await OnChgChannelReq(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CS_CORPSASK_REQ:
        co_await OnCorpsAskReq(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CS_HEROLIST_REQ:
        co_await OnHeroListReq(std::move(sess), state, packet, ctx);
        break;
    case MessageId::CS_PETCANCEL_REQ:
        co_await OnPetCancelReq(std::move(sess), state, packet, ctx);
        break;
    case MessageId::SM_QUITSERVICE_REQ:
        co_await OnQuitServiceReq(std::move(sess), state, packet, ctx);
        break;
    default:
        // F1 doesn't implement the rest of the 621-wire surface. Log
        // the gap at debug level so bring-up notes can surface what a
        // real client + cluster actually sends, but don't tear down
        // the connection — the codec stays in sync, and later phases
        // fill in the handlers behind the same dispatch table.
        spdlog::debug("tmapsvr: unhandled id=0x{:04X} body={} bytes "
                      "(uid={} char={})",
            packet.wId, packet.body.size(), state.user_id, state.char_id);
        break;
    }
}

} // namespace tmapsvr
