#include "handlers.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

namespace tworldsvr::handlers {

boost::asio::awaitable<void>
Dispatch(std::shared_ptr<WorldSession> sess,
         std::uint16_t                 wId,
         std::vector<std::byte>        body,
         const HandlerContext&         ctx)
{
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToMessageId;
    using tnetlib::protocol::NameOf;

    const auto id = ToMessageId(wId);
    switch (id)
    {
    // ---- W2: char lifecycle (handlers_char.cpp) -------------------
    case MessageId::MW_ADDCHAR_ACK:
        co_await OnAddCharAck(std::move(sess), std::move(body), ctx);
        co_return;
    case MessageId::MW_CLOSECHAR_ACK:
        co_await OnCloseCharAck(std::move(sess), std::move(body), ctx);
        co_return;

    // ---- W3a-1: guild load (handlers_guild.cpp) -------------------
    case MessageId::DM_GUILDLOAD_ACK:
        co_await OnGuildLoadAck(std::move(sess), std::move(body), ctx);
        co_return;

    // W3a-2/-3 fill the remaining 75 GUILD handlers, W3b PARTY_/CORPS_,
    // … see README §4.

    default:
        break;
    }

    // Unrecognised wID — log + drop. The legacy framer keeps the
    // peer link open after an unknown packet, which is the same
    // behaviour the four shipped Asio daemons use.
    const auto name = NameOf(id);
    if (name.empty())
    {
        spdlog::warn("world_dispatch[{}]: unknown wID=0x{:04X} body={} bytes "
                     "— dropped",
            sess->RemoteIPv4(), wId, body.size());
    }
    else
    {
        spdlog::info("world_dispatch[{}]: wID=0x{:04X} ({}) body={} bytes "
                     "— no handler yet (W3+ scope)",
            sess->RemoteIPv4(), wId, name, body.size());
    }
    co_return;
}

} // namespace tworldsvr::handlers
