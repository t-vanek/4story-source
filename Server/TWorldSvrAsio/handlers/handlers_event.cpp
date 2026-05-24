#include "handlers.h"
#include "../senders/senders.h"
#include "../services/chat_constants.h"
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

} // namespace tworldsvr::handlers
