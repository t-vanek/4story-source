#include "handlers_world.h"

#include "services/world_client.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <utility>

namespace tmapsvr {

namespace {

// CN_-style result codes for DM_LOADCHAR_ACK error paths. Same values
// as the CS_CONNECT_ACK enum (internal-only constants — legacy header
// not yet recovered; safe to keep them aligned).
constexpr std::uint8_t CnInternal = 3;

// DM_LOADCHAR_ACK error body — 9 bytes (dwCharID + dwKEY + result).
// Mirrors the error path in legacy SSHandler.cpp:3379 where the char
// row is missing or the DB query fails. The success path carries the
// full character snapshot and lands with the F8 player service.
std::vector<std::byte> EncodeLoadCharAckError(std::uint32_t dwCharID,
                                              std::uint32_t dwKEY,
                                              std::uint8_t  result)
{
    std::vector<std::byte> body;
    body.reserve(9);
    wire::WritePOD<std::uint32_t>(body, dwCharID);
    wire::WritePOD<std::uint32_t>(body, dwKEY);
    wire::WritePOD<std::uint8_t> (body, result);
    return body;
}

} // namespace

boost::asio::awaitable<void>
OnDMLoadCharReq(std::vector<std::byte> body, const HandlerContext& ctx)
{
    using tnetlib::protocol::MessageId;

    // Wire layout from legacy SSHandler.cpp:3311 (12 bytes):
    //   DWORD dwCharID
    //   DWORD dwKEY
    //   DWORD dwUserID
    wire::Reader r(body.data(), body.size());
    std::uint32_t dwCharID = 0, dwKEY = 0, dwUserID = 0;
    if (!r.Read(dwCharID) || !r.Read(dwKEY) || !r.Read(dwUserID))
    {
        spdlog::warn("DM_LOADCHAR_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    spdlog::info("DM_LOADCHAR_REQ: char={} key={} user={} — F6 stub (player "
                 "load service lands in F8); replying with INTERNAL",
        dwCharID, dwKEY, dwUserID);

    // F6 always replies with the error variant so the world knows
    // not to wait. F8 replaces this branch with the real DB-backed
    // snapshot load + DM_LOADCHAR_ACK success encoding (~70 fields
    // off TCHARTABLE per legacy CTBLChar query at SSHandler.cpp:3329).
    if (ctx.world_client && ctx.world_client->IsConnected())
    {
        co_await ctx.world_client->SendPacket(
            static_cast<std::uint16_t>(MessageId::DM_LOADCHAR_ACK),
            EncodeLoadCharAckError(dwCharID, dwKEY, CnInternal));
    }
    else
    {
        spdlog::warn("DM_LOADCHAR_REQ: world peer not connected — ack "
                     "dropped (char={})", dwCharID);
    }
}

boost::asio::awaitable<void>
DispatchWorld(std::uint16_t          wId,
              std::vector<std::byte> body,
              const HandlerContext&  ctx)
{
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToMessageId;

    const auto id = ToMessageId(wId);
    switch (id)
    {
    case MessageId::DM_LOADCHAR_REQ:
        co_await OnDMLoadCharReq(std::move(body), ctx);
        break;

    default:
        spdlog::debug("world_client: unhandled wId=0x{:04X} body={} bytes",
            wId, body.size());
        break;
    }
}

} // namespace tmapsvr
