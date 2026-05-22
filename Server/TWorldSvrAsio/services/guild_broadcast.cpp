#include "services/guild_broadcast.h"

#include <mutex>

namespace tworldsvr {

boost::asio::awaitable<std::size_t>
BroadcastToGuildMembers(CharRegistry&                     chars,
                        PeerRegistry&                     peers,
                        const std::vector<std::uint32_t>& member_char_ids,
                        GuildPacketBuilder                build_and_send)
{
    // Snapshot the peer table once outside the per-member loop.
    // The registry is small (O(10..50)); copying the vector keeps
    // us out of the shared_mutex for the duration of the broadcast.
    auto peer_snapshot = peers.Snapshot();

    std::size_t delivered = 0;
    for (auto mcid : member_char_ids)
    {
        auto tchar = chars.Find(mcid);
        if (!tchar) continue;

        std::uint8_t  main_svr_id = 0;
        std::uint32_t key         = 0;
        {
            std::lock_guard g(tchar->lock);
            main_svr_id = tchar->main_server_id;
            key         = tchar->key;
        }
        if (main_svr_id == 0) continue;

        std::shared_ptr<PeerSession> target;
        for (auto& p : peer_snapshot)
        {
            if (static_cast<std::uint8_t>(p->Wid() & 0xFF) == main_svr_id)
            {
                target = p;
                break;
            }
        }
        if (!target) continue;

        co_await build_and_send(target, mcid, key);
        ++delivered;
    }
    co_return delivered;
}

} // namespace tworldsvr
