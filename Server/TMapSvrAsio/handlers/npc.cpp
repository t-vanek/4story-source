// NPC-interaction handler — CS_NPCTALK_REQ.
//
// Client passes wNpcID, server looks up the NPC in the chart loaded
// at boot (TNPCCHART → SociNpcService) and replies with
// CS_NPCTALK_ACK carrying the quest id the NPC triggers. F10 always
// ships dwQuestID = 0 (default chat / shop window); the QTT_TALK
// quest-trigger scan lands with the quest-engine consolidation
// pass.
//
// Legacy parity: CSHandler.cpp:3506 (OnCS_NPCTALK_REQ) +
// CSSender.cpp:3824 (SendCS_NPCTALK_ACK).

#include "handlers.h"

#include "services/npc_service.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace tmapsvr {

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

    // Legacy quest-trigger check (CheckQuest with QTT_TALK) is quest-
    // engine work. For now we always reply dwQuestID = 0 — UI shows
    // the NPC's default chat / shop window.
    const std::uint32_t dwQuestID = 0;

    spdlog::info("CS_NPCTALK_REQ npc={} ({} type={}) -> CS_NPCTALK_ACK "
                 "questID={} (quest-trigger check lands with quest "
                 "consolidation)",
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

} // namespace tmapsvr
