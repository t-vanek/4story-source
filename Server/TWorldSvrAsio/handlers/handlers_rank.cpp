#include "handlers.h"
#include "../senders/senders.h"
#include "../wire_codec.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <string>

namespace tworldsvr::handlers {

boost::asio::awaitable<void>
OnFameRankUpdateAck(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte>       body,
                    const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers)
    {
        spdlog::warn("OnFameRankUpdateAck[{}]: peers not wired", ip);
        co_return;
    }
    // Opaque fame-ranking table — forwarded verbatim to every peer.
    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwFameRankUpdateReq(p, body);
    co_return;
}

boost::asio::awaitable<void>
OnHeroSelectAck(std::shared_ptr<PeerSession> peer,
                std::vector<std::byte>       body,
                const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers)
    {
        spdlog::warn("OnHeroSelectAck[{}]: peers not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint16_t battle_zone = 0;
    std::string   hero_name;
    std::int64_t  time_hero = 0;
    if (!r.Read(battle_zone) || !r.ReadString(hero_name) || !r.Read(time_hero))
    {
        spdlog::warn("OnHeroSelectAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwHeroSelectReq(p, battle_zone, hero_name,
            time_hero);
    co_return;
}

} // namespace tworldsvr::handlers
