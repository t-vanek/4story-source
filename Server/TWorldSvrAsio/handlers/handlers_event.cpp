#include "handlers.h"
#include "../senders/senders.h"
#include "../services/chat_constants.h"
#include "../services/event_constants.h"
#include "../services/event_registry.h"
#include "../wire_codec.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <cstdlib>
#include <string>

namespace tworldsvr::handlers {

namespace {

// TCONTRY_N (NetCode.h:1095) — the neutral country the operator
// announcement is tagged with.
constexpr std::uint8_t kCountryNeutral = 3;

} // namespace

boost::asio::awaitable<void>
OnEventQuarterReq(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers)
    {
        spdlog::warn("OnEventQuarterReq[{}]: peers not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint8_t day = 0, hour = 0, minute = 0;
    std::string  present;
    if (!r.Read(day) || !r.Read(hour) || !r.Read(minute) ||
        !r.ReadString(present))
    {
        spdlog::warn("OnEventQuarterReq[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    // Pick the present bucket once so every map shows the same one
    // (legacy rand() % 100).
    const std::uint8_t select = static_cast<std::uint8_t>(std::rand() % 100);

    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwEventQuarterReq(p, day, hour, minute, select,
            present);
    co_return;
}

boost::asio::awaitable<void>
OnEventQuarterNotifyReq(std::shared_ptr<PeerSession> peer,
                        std::vector<std::byte>       body,
                        const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers)
    {
        spdlog::warn("OnEventQuarterNotifyReq[{}]: peers not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::string announce;
    if (!r.ReadString(announce))
    {
        spdlog::warn("OnEventQuarterNotifyReq[{}]: short body", ip);
        co_return;
    }

    // Broadcast the announcement as a world-chat line from the
    // operator. The operator display name (legacy GetSvrMsg(
    // NAME_OPERATOR)) is deferred — same server-message-table gap as
    // the W4-5 operator-whisper — so the sender name is left empty.
    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwChatReq(p, /*char_id=*/0, /*key=*/0,
            /*channel=*/0, /*sender_id=*/0, /*sender_name=*/std::string{},
            kCountryNeutral, kCountryNeutral, /*type=*/chat::kWorld,
            /*group=*/chat::kWorld, /*target_id=*/0, announce);
    co_return;
}

boost::asio::awaitable<void>
OnCtEventMsgReq(std::shared_ptr<PeerSession> peer,
                std::vector<std::byte>       body,
                const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers)
    {
        spdlog::warn("OnCtEventMsgReq[{}]: peers not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint8_t event_id = 0;
    std::uint8_t msg_type = 0;
    std::string  msg;
    if (!r.Read(event_id) || !r.Read(msg_type) || !r.ReadString(msg))
    {
        spdlog::warn("OnCtEventMsgReq[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwEventMsgReq(p, event_id, msg_type, msg);
    co_return;
}

boost::asio::awaitable<void>
OnCtEventUpdateReq(std::shared_ptr<PeerSession> peer,
                   std::vector<std::byte>       body,
                   const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers || !ctx.events)
    {
        spdlog::warn("OnCtEventUpdateReq[{}]: peers/events not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint8_t  event_id = 0;
    std::uint16_t value    = 0;
    std::uint32_t dw_index = 0;
    std::uint8_t  b_id     = 0;
    if (!r.Read(event_id) || !r.Read(value) || !r.Read(dw_index)
        || !r.Read(b_id))
    {
        spdlog::warn("OnCtEventUpdateReq[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    // Legacy SSHandler.cpp:276 — drop event_ids past the table.
    if (event_id > event_type::kCount)
    {
        spdlog::debug("OnCtEventUpdateReq[{}]: event_id={} > kCount={} "
                      "— dropped", ip, event_id, event_type::kCount);
        co_return;
    }

    // Legacy LOTTERY / GIFTTIME short-circuit (SSHandler.cpp:279-292).
    // Both run gameplay reward subsystems on the world server (random
    // char pick + in-game mail via SendPost + MW_EVENTMSGLOTTERY_REQ
    // fan-out). Those helpers are not ported yet — log + drop without
    // storing or broadcasting. The state bit lives at the *start* of
    // the EVENTINFO body (legacy m_bState immediately after m_bID);
    // since we only peeked dw_index + b_id, treat "any LOTTERY /
    // GIFTTIME inbound" as the deferred path regardless of state.
    if (b_id == event_type::kLottery || b_id == event_type::kGiftTime)
    {
        spdlog::info("OnCtEventUpdateReq[{}]: LOTTERY/GIFTTIME path "
                     "(b_id={}) — deferred (reward subsystem absent)", ip,
            b_id);
        co_return;
    }

    // Capture the opaque EVENTINFO body — everything past the outer
    // (event_id, value) pair. The dw_index + b_id we just peeked at
    // belong inside that body too, so we snapshot from offset 3 (the
    // outer header is BYTE + WORD = 3 bytes).
    constexpr std::size_t kOuterHeader =
        sizeof(std::uint8_t) + sizeof(std::uint16_t);
    std::vector<std::byte> event_body(
        body.begin() + kOuterHeader, body.end());

    // Mirror legacy SSHandler.cpp:294-298 — erase any existing entry
    // for this index, insert the new entry only if value != 0
    // (legacy "wValue==0 deactivates"). Always re-broadcast so peers
    // see the deactivation too (legacy broadcasts unconditionally).
    ctx.events->Erase(dw_index);
    if (value != 0)
    {
        TEventInfo info{};
        info.event_id = event_id;
        info.value    = value;
        info.dw_index = dw_index;
        info.b_id     = b_id;
        info.body     = event_body;
        ctx.events->Set(std::move(info));
    }

    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwEventUpdateReq(p, event_id, value,
            event_body);
    co_return;
}

} // namespace tworldsvr::handlers
