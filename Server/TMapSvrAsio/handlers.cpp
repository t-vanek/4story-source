#include "handlers.h"

#include "services/channel_presence.h"
#include "services/npc_service.h"
#include "services/session_registry.h"
#include "services/session_validator.h"
#include "services/world_client.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstring>
#include <utility>
#include <vector>

namespace tmapsvr {

namespace {

// Numeric result codes the legacy CS_CONNECT_ACK sends back. Names
// match the CN_* constants the legacy CSHandler.cpp uses; the values
// are operator-controlled until the legacy header turns up — keeping
// 0 = OK so a default-zero byte means success matches the convention
// used elsewhere on the wire.
enum class ConnectResult : std::uint8_t
{
    Ok          = 0,
    InvalidVer  = 1,
    InvalidChar = 2,
    Internal    = 3,
    NoChannel   = 4,
    Duplicate   = 5,
};

const char* ConnectResultName(ConnectResult r)
{
    switch (r) {
        case ConnectResult::Ok:          return "OK";
        case ConnectResult::InvalidVer:  return "INVALID_VER";
        case ConnectResult::InvalidChar: return "INVALID_CHAR";
        case ConnectResult::Internal:    return "INTERNAL";
        case ConnectResult::NoChannel:   return "NO_CHANNEL";
        case ConnectResult::Duplicate:   return "DUPLICATE";
    }
    return "?";
}

// CS_CONNECT_ACK body — single result byte followed by an empty
// vServerID vector (length 0). Mirrors CTPlayer::SendCS_CONNECT_ACK
// in CSSender.cpp:78. The vServerID list is populated by the World
// peer in later phases; F4/F5 always send an empty list.
std::vector<std::byte> EncodeConnectAck(ConnectResult r)
{
    std::vector<std::byte> body;
    body.reserve(2);
    wire::WritePOD<std::uint8_t>(body, static_cast<std::uint8_t>(r));
    wire::WritePOD<std::uint8_t>(body, 0); // vServerID.size()
    return body;
}

// MW_ADDCHAR_ACK body — 18 bytes, mirrors
// CTMapSvrModule::SendMW_ADDCHAR_ACK in SSSender.cpp:237. Sent on the
// map↔world peer after a CS_CONNECT_REQ clears so the World server
// knows where to route DM_* traffic for this character.
std::vector<std::byte> EncodeAddCharAck(std::uint32_t dwCharID,
                                        std::uint32_t dwKEY,
                                        std::uint32_t dwIPAddr,
                                        std::uint16_t wPort,
                                        std::uint32_t dwUserID)
{
    std::vector<std::byte> body;
    body.reserve(18);
    wire::WritePOD<std::uint32_t>(body, dwCharID);
    wire::WritePOD<std::uint32_t>(body, dwKEY);
    wire::WritePOD<std::uint32_t>(body, dwIPAddr);
    wire::WritePOD<std::uint16_t>(body, wPort);
    wire::WritePOD<std::uint32_t>(body, dwUserID);
    return body;
}

} // namespace

boost::asio::awaitable<void>
OnConnectReq(std::shared_ptr<tnetlib::AsioSession> sess,
             std::vector<std::byte>                body,
             const HandlerContext&                 ctx)
{
    using tnetlib::protocol::MessageId;

    // Wire layout from legacy CSHandler.cpp::OnCS_CONNECT_REQ (decode
    // order matches CPacket::operator>>):
    //   WORD  wVersion
    //   BYTE  bChannel
    //   DWORD dwUserID
    //   DWORD dwID         (char id)
    //   DWORD dwKEY        (session token)
    //   DWORD dwIPAddr
    //   WORD  wPort
    //   INT64 llChecksum1  (validated against a derived value;
    //                       version + checksum checks land in a
    //                       follow-up commit — for F4 we trust the
    //                       framing and only verify the DB lookup)
    wire::Reader r(body.data(), body.size());

    std::uint16_t wVersion = 0;
    std::uint8_t  bChannel = 0;
    std::uint32_t dwUserID = 0;
    std::uint32_t dwID     = 0;
    std::uint32_t dwKEY    = 0;
    std::uint32_t dwIPAddr = 0;
    std::uint16_t wPort    = 0;
    std::int64_t  llChecksum1 = 0;

    if (!r.Read(wVersion) ||
        !r.Read(bChannel) ||
        !r.Read(dwUserID) ||
        !r.Read(dwID)     ||
        !r.Read(dwKEY)    ||
        !r.Read(dwIPAddr) ||
        !r.Read(wPort)    ||
        !r.Read(llChecksum1))
    {
        spdlog::warn("CS_CONNECT_REQ: short body ({} bytes) — dropping",
            body.size());
        co_await sess->SendPacket(
            static_cast<std::uint16_t>(MessageId::CS_CONNECT_ACK),
            EncodeConnectAck(ConnectResult::Internal));
        co_return;
    }

    // Run the validator. No DB pool → no validator → handshake fails
    // (the F3 boot log already warned the operator).
    if (!ctx.validator)
    {
        spdlog::warn("CS_CONNECT_REQ uid={} key={}: validator not configured "
                     "(no [database] in TOML) — refusing",
            dwUserID, dwKEY);
        co_await sess->SendPacket(
            static_cast<std::uint16_t>(MessageId::CS_CONNECT_ACK),
            EncodeConnectAck(ConnectResult::Internal));
        co_return;
    }

    const auto row = ctx.validator->LookupSession(dwUserID, dwKEY);
    ConnectResult result = ConnectResult::Ok;
    if (!row)
        result = ConnectResult::InvalidChar;
    else if (row->bLocked)
        result = ConnectResult::InvalidChar;
    else if (ctx.expected_group != 0 && row->bGroupID != ctx.expected_group)
        result = ConnectResult::NoChannel;
    else if (row->bChannel != bChannel)
        result = ConnectResult::NoChannel;

    spdlog::info("CS_CONNECT_REQ uid={} key={} char={} ch={} ver={} -> {}",
        dwUserID, dwKEY, dwID, bChannel, wVersion,
        ConnectResultName(result));

    // Register the session under its char id BEFORE the ack flushes
    // so a fast world reply (DM_LOADCHAR_REQ on this same char) can
    // resolve back to this socket. Bind overwrites any stale entry —
    // the legacy m_mapPLAYER had the same "last-write-wins" behavior
    // when a duplicate connect raced an outstanding session. Also
    // pre-binds the per-channel presence entry so subsequent
    // CS_MOVE_REQ broadcasts know which channel the client is on.
    if (result == ConnectResult::Ok && ctx.session_reg)
        ctx.session_reg->Bind(dwID, sess);
    if (result == ConnectResult::Ok && ctx.presence)
        ctx.presence->Bind(dwID, bChannel, sess);

    co_await sess->SendPacket(
        static_cast<std::uint16_t>(MessageId::CS_CONNECT_ACK),
        EncodeConnectAck(result));

    // On success, announce the new char to the World peer so it can
    // route MW_/DM_ traffic our way. Mirrors the legacy
    // CSHandler.cpp::OnCS_CONNECT_REQ tail-call into
    // SendMW_ADDCHAR_ACK (SSSender.cpp:237). The send is fire-and-
    // forget — IsConnected gate prevents queueing into a dead peer
    // (F5 doesn't buffer; F5+ phases may add a retry queue).
    if (result == ConnectResult::Ok && ctx.world_client &&
        ctx.world_client->IsConnected())
    {
        const bool sent = co_await ctx.world_client->SendPacket(
            static_cast<std::uint16_t>(MessageId::MW_ADDCHAR_ACK),
            EncodeAddCharAck(dwID, dwKEY, dwIPAddr, wPort, dwUserID));
        if (!sent)
            spdlog::warn("MW_ADDCHAR_ACK uid={} char={}: world send returned "
                         "false (peer dropped between IsConnected and send)",
                dwUserID, dwID);
    }
    else if (result == ConnectResult::Ok && !ctx.world_client)
    {
        spdlog::debug("MW_ADDCHAR_ACK uid={} char={}: world peer not "
                      "configured — char registration skipped",
            dwUserID, dwID);
    }
    else if (result == ConnectResult::Ok)
    {
        spdlog::warn("MW_ADDCHAR_ACK uid={} char={}: world peer disconnected "
                     "— char registration deferred",
            dwUserID, dwID);
    }
}

// CS_MOVE_ACK body — 27 bytes broadcast to other channel members
// after a CS_MOVE_REQ. Mirrors CTPlayer::SendCS_MOVE_ACK in legacy
// CSSender.cpp:599. The full field set lets each client interpolate
// the moving player's position, facing, action, and speed.
namespace {
std::vector<std::byte> EncodeMoveAck(std::uint32_t dwCharID,
                                     float fPosX, float fPosY, float fPosZ,
                                     std::uint16_t wPitch, std::uint16_t wDIR,
                                     std::uint8_t bMouseDIR, std::uint8_t bKeyDIR,
                                     std::uint8_t bAction, float fSpeed)
{
    std::vector<std::byte> body;
    body.reserve(27);
    wire::WritePOD<std::uint32_t>(body, dwCharID);
    wire::WritePOD<float>        (body, fPosX);
    wire::WritePOD<float>        (body, fPosY);
    wire::WritePOD<float>        (body, fPosZ);
    wire::WritePOD<std::uint16_t>(body, wPitch);
    wire::WritePOD<std::uint16_t>(body, wDIR);
    wire::WritePOD<std::uint8_t> (body, bMouseDIR);
    wire::WritePOD<std::uint8_t> (body, bKeyDIR);
    wire::WritePOD<std::uint8_t> (body, bAction);
    wire::WritePOD<float>        (body, fSpeed);
    return body;
}
} // namespace

boost::asio::awaitable<void>
OnConReadyReq(std::shared_ptr<tnetlib::AsioSession> sess,
              std::vector<std::byte>                body,
              const HandlerContext&                 ctx)
{
    // CS_CONREADY_REQ has no body fields — it's a one-shot signal
    // from the client that it's done loading the local scene and is
    // ready to receive game state. Legacy CSHandler.cpp:402 calls
    // InitMap() / EnterMAP() here. F7 doesn't have the MapState yet,
    // so we just log and let CS_MOVE_REQ start exercising the
    // broadcast path. The CHARINFO_ACK / surrounding-entities flood
    // lands with the F8 player service.
    (void)sess;
    (void)body;
    if (ctx.session_reg)
    {
        const auto cid = ctx.session_reg->FindCharIdBySession(sess.get());
        spdlog::info("CS_CONREADY_REQ from char={} — F7 stub (full enter-map "
                     "broadcast lands with F8)",
            cid ? *cid : 0);
    }
    co_return;
}

boost::asio::awaitable<void>
OnMoveReq(std::shared_ptr<tnetlib::AsioSession> sess,
          std::vector<std::byte>                body,
          const HandlerContext&                 ctx)
{
    using tnetlib::protocol::MessageId;

    // Wire layout from legacy CSHandler.cpp::OnCS_MOVE_REQ (26 bytes):
    //   WORD  wMapID
    //   FLOAT fPosX  fPosY  fPosZ
    //   WORD  wPitch wDIR
    //   BYTE  bMouseDIR bKeyDIR bAction bGhost
    //   FLOAT fSpeed
    // The legacy code also does speed-hack detection (fSpeed > 3.40)
    // and ghost-mode handling — both are gameplay-policy work that
    // wraps the simple broadcast in this phase; folding them in
    // here would couple F7 to features the player-state service
    // (F8) hasn't materialized yet. TODO once F8 lands.
    wire::Reader r(body.data(), body.size());
    std::uint16_t wMapID    = 0;
    float fPosX = 0.f, fPosY = 0.f, fPosZ = 0.f;
    std::uint16_t wPitch    = 0;
    std::uint16_t wDIR      = 0;
    std::uint8_t  bMouseDIR = 0, bKeyDIR = 0, bAction = 0, bGhost = 0;
    float fSpeed = 0.f;

    if (!r.Read(wMapID) ||
        !r.Read(fPosX)  || !r.Read(fPosY) || !r.Read(fPosZ) ||
        !r.Read(wPitch) || !r.Read(wDIR)  ||
        !r.Read(bMouseDIR) || !r.Read(bKeyDIR) || !r.Read(bAction) ||
        !r.Read(bGhost) || !r.Read(fSpeed))
    {
        spdlog::warn("CS_MOVE_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    if (!ctx.session_reg || !ctx.presence)
    {
        spdlog::debug("CS_MOVE_REQ: presence/session_reg not configured — "
                      "dropping move (no broadcast destination)");
        co_return;
    }

    const auto cid_opt = ctx.session_reg->FindCharIdBySession(sess.get());
    if (!cid_opt)
    {
        spdlog::debug("CS_MOVE_REQ from unbound session — dropping "
                      "(client must complete CS_CONNECT_REQ first)");
        co_return;
    }
    const std::uint32_t dwCharID = *cid_opt;

    const auto entry = ctx.presence->FindEntry(dwCharID);
    if (!entry)
    {
        spdlog::debug("CS_MOVE_REQ char={} not in presence map — dropping",
            dwCharID);
        co_return;
    }
    const std::uint8_t mover_channel = entry->channel;

    // Persist the new position so the next ForEachInChannel snapshot
    // (or a later F8/F9 spatial query) reads the up-to-date value.
    ctx.presence->UpdatePosition(dwCharID, wMapID, Position{fPosX, fPosY, fPosZ});

    // Collect every other in-channel session, then broadcast on the
    // same coroutine. The visit snapshot inside InMemoryChannelPresence
    // is done under the lock so the visitor itself runs lock-free —
    // safe to co_await SendPacket here without the mutex held.
    std::vector<std::shared_ptr<tnetlib::AsioSession>> recipients;
    ctx.presence->ForEachInChannel(mover_channel, dwCharID,
        [&recipients](const ChannelPresenceEntry&,
                      std::shared_ptr<tnetlib::AsioSession> sp) {
            recipients.push_back(std::move(sp));
        });

    spdlog::debug("CS_MOVE_REQ char={} ch={} map={} pos=({:.1f},{:.1f},{:.1f}) "
                  "-> broadcast {} peer(s)",
        dwCharID, mover_channel, wMapID, fPosX, fPosY, fPosZ,
        recipients.size());

    auto ack = EncodeMoveAck(dwCharID, fPosX, fPosY, fPosZ,
                             wPitch, wDIR, bMouseDIR, bKeyDIR, bAction,
                             fSpeed);
    for (auto& peer : recipients)
    {
        co_await peer->SendPacket(
            static_cast<std::uint16_t>(MessageId::CS_MOVE_ACK),
            std::span<const std::byte>(ack.data(), ack.size()));
    }
}

boost::asio::awaitable<void>
OnNpcTalkReq(std::shared_ptr<tnetlib::AsioSession> sess,
             std::vector<std::byte>                body,
             const HandlerContext&                 ctx)
{
    using tnetlib::protocol::MessageId;

    // CS_NPCTALK_REQ body — WORD wNpcID (2 bytes). Legacy decoder is
    // in CSHandler.cpp:3506.
    wire::Reader r(body.data(), body.size());
    std::uint16_t wNpcID = 0;
    if (!r.Read(wNpcID))
    {
        spdlog::warn("CS_NPCTALK_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    if (!ctx.npc_service)
    {
        spdlog::debug("CS_NPCTALK_REQ npc={}: no NPC service configured — "
                      "dropping", wNpcID);
        co_return;
    }

    const auto npc = ctx.npc_service->FindNpc(wNpcID);
    if (!npc)
    {
        // Legacy returns EC_NOERROR with no reply when FindTNpc misses
        // (CSHandler.cpp:3517). Same behavior here — client just sees
        // a silent no-op, which is what 4Story's UI handles.
        spdlog::debug("CS_NPCTALK_REQ npc={}: not in chart — dropping",
            wNpcID);
        co_return;
    }

    // Legacy quest-trigger check (CheckQuest with QTT_TALK) is F12
    // work. For now we always reply dwQuestID = 0 — UI shows the
    // NPC's default chat / shop window.
    const std::uint32_t dwQuestID = 0;

    spdlog::info("CS_NPCTALK_REQ npc={} ({} type={}) -> CS_NPCTALK_ACK "
                 "questID={} (quest-trigger check lands in F12)",
        wNpcID, npc->szName, npc->bType, dwQuestID);

    // CS_NPCTALK_ACK body — DWORD dwQuestID + WORD wNpcID (6 bytes).
    std::vector<std::byte> ack;
    ack.reserve(6);
    wire::WritePOD<std::uint32_t>(ack, dwQuestID);
    wire::WritePOD<std::uint16_t>(ack, wNpcID);

    co_await sess->SendPacket(
        static_cast<std::uint16_t>(MessageId::CS_NPCTALK_ACK),
        std::span<const std::byte>(ack.data(), ack.size()));
}

boost::asio::awaitable<void>
OnSkillUseReq(std::shared_ptr<tnetlib::AsioSession> sess,
              std::vector<std::byte>                body,
              const HandlerContext&                 ctx)
{
    // CS_SKILLUSE_REQ header (legacy CSHandler.cpp:2429) — 31 bytes
    // before the variable defender list. F11 decodes the header,
    // logs the skill cast intent, and drops the request. Damage
    // calculation, MP consumption, defender targeting, and the
    // CS_SKILLUSE_ACK broadcast (25+ fields of damage info) are
    // gameplay-policy work that needs the skill template chart
    // (TSKILLCHART) and equipment / class state — those land in a
    // later phase.
    wire::Reader r(body.data(), body.size());

    std::uint32_t dwAttackID  = 0;
    std::uint8_t  bAttackType = 0;
    std::uint8_t  bChannel    = 0;
    std::uint16_t wMapID      = 0;
    std::uint16_t wSkillID    = 0;
    std::uint8_t  bActionID   = 0;
    std::uint32_t dwActID     = 0;
    std::uint32_t dwAniID     = 0;
    float fPosX = 0.f, fPosY = 0.f, fPosZ = 0.f;

    if (!r.Read(dwAttackID)  || !r.Read(bAttackType) ||
        !r.Read(bChannel)    || !r.Read(wMapID)      ||
        !r.Read(wSkillID)    || !r.Read(bActionID)   ||
        !r.Read(dwActID)     || !r.Read(dwAniID)     ||
        !r.Read(fPosX) || !r.Read(fPosY) || !r.Read(fPosZ))
    {
        spdlog::warn("CS_SKILLUSE_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    std::uint32_t cid = 0;
    if (ctx.session_reg)
    {
        if (const auto found = ctx.session_reg->FindCharIdBySession(sess.get()))
            cid = *found;
    }

    spdlog::info("CS_SKILLUSE_REQ char={} skill={} action={} target={} type={} "
                 "ch={} map={} pos=({:.1f},{:.1f},{:.1f}) — F11 stub (damage / "
                 "ack broadcast lands with skill-template phase)",
        cid, wSkillID, bActionID, dwAttackID, bAttackType,
        bChannel, wMapID, fPosX, fPosY, fPosZ);

    co_return;
}

boost::asio::awaitable<void>
OnQuestExecReq(std::shared_ptr<tnetlib::AsioSession> sess,
               std::vector<std::byte>                body,
               const HandlerContext&                 ctx)
{
    // CS_QUESTEXEC_REQ body (legacy CSHandler.cpp:3536):
    //   DWORD dwQuestID
    //   BYTE  bRewardType
    //   DWORD dwRewardID
    // F12 decodes + logs. The real engine — quest template lookup,
    // term-state advance, reward award, MW_/DM_ broadcasts — needs
    // the QuestEngine layer that lives over the F11 skill /
    // template / item charts; lands after the scaffolding sweep.
    wire::Reader r(body.data(), body.size());
    std::uint32_t dwQuestID = 0, dwRewardID = 0;
    std::uint8_t  bRewardType = 0;
    if (!r.Read(dwQuestID) || !r.Read(bRewardType) || !r.Read(dwRewardID))
    {
        spdlog::warn("CS_QUESTEXEC_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    std::uint32_t cid = 0;
    if (ctx.session_reg)
        if (const auto found = ctx.session_reg->FindCharIdBySession(sess.get()))
            cid = *found;

    spdlog::info("CS_QUESTEXEC_REQ char={} quest={} rewardType={} reward={} "
                 "— F12 stub (quest engine evaluation lands after "
                 "scaffolding sweep)",
        cid, dwQuestID, bRewardType, dwRewardID);
    co_return;
}

boost::asio::awaitable<void>
OnQuestDropReq(std::shared_ptr<tnetlib::AsioSession> sess,
               std::vector<std::byte>                body,
               const HandlerContext&                 ctx)
{
    // CS_QUESTDROP_REQ body (legacy CSHandler.cpp:3590):
    //   DWORD dwQuestID
    // F12 decodes + logs. The real DropQuest path (rewrite term
    // rows + emit CS_QUESTCOMPLETE_ACK(QR_DROP) + audit log) lands
    // with the engine.
    wire::Reader r(body.data(), body.size());
    std::uint32_t dwQuestID = 0;
    if (!r.Read(dwQuestID))
    {
        spdlog::warn("CS_QUESTDROP_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    std::uint32_t cid = 0;
    if (ctx.session_reg)
        if (const auto found = ctx.session_reg->FindCharIdBySession(sess.get()))
            cid = *found;

    spdlog::info("CS_QUESTDROP_REQ char={} quest={} — F12 stub",
        cid, dwQuestID);
    co_return;
}

boost::asio::awaitable<void>
Dispatch(std::shared_ptr<tnetlib::AsioSession> sess,
         std::uint16_t                         wId,
         std::vector<std::byte>                body,
         const HandlerContext&                 ctx)
{
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToMessageId;

    const auto id = ToMessageId(wId);
    switch (id)
    {
    case MessageId::CS_CONNECT_REQ:
        co_await OnConnectReq(sess, std::move(body), ctx);
        break;
    case MessageId::CS_CONREADY_REQ:
        co_await OnConReadyReq(sess, std::move(body), ctx);
        break;
    case MessageId::CS_MOVE_REQ:
        co_await OnMoveReq(sess, std::move(body), ctx);
        break;
    case MessageId::CS_NPCTALK_REQ:
        co_await OnNpcTalkReq(sess, std::move(body), ctx);
        break;
    case MessageId::CS_SKILLUSE_REQ:
        co_await OnSkillUseReq(sess, std::move(body), ctx);
        break;
    case MessageId::CS_QUESTEXEC_REQ:
        co_await OnQuestExecReq(sess, std::move(body), ctx);
        break;
    case MessageId::CS_QUESTDROP_REQ:
        co_await OnQuestDropReq(sess, std::move(body), ctx);
        break;

    default:
        spdlog::debug("map_server: unhandled wId=0x{:04X} body={} bytes",
            wId, body.size());
        break;
    }
}

} // namespace tmapsvr
