#include "handlers.h"

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
    // when a duplicate connect raced an outstanding session.
    if (result == ConnectResult::Ok && ctx.session_reg)
        ctx.session_reg->Bind(dwID, sess);

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

    default:
        spdlog::debug("map_server: unhandled wId=0x{:04X} body={} bytes",
            wId, body.size());
        break;
    }
}

} // namespace tmapsvr
