#include "message_router.h"

#include "peer_session.h"
#include "services/peer_registry.h"

#include <spdlog/spdlog.h>

#include <algorithm>

namespace tcontrolsvr {

namespace {

bool IsLive(const std::shared_ptr<PeerSession>& peer)
{
    return peer && peer->Wire() && peer->Wire()->IsOpen();
}

} // namespace

MessageRouter::MessageRouter(PeerRegistry& peers) : m_peers(peers) {}

boost::asio::awaitable<bool>
MessageRouter::SendToService(std::uint32_t service_id,
                             std::uint16_t wId,
                             std::vector<std::byte> body)
{
    auto peer = m_peers.Connection(service_id);
    if (!IsLive(peer)) co_return false;
    co_await peer->Wire()->SendPacket(wId, std::move(body));
    co_return true;
}

boost::asio::awaitable<std::uint32_t>
MessageRouter::SendToType(std::uint8_t group_id,
                          std::uint8_t type_id,
                          std::uint16_t wId,
                          std::vector<std::byte> body)
{
    // Collect live peers in the bucket — skipping offline ones lets
    // round-robin keep moving even when one of N peers is down. Order
    // matches PeerRegistry::FindByType (insertion order in the inventory).
    auto candidates = m_peers.FindByType(group_id, type_id);
    candidates.erase(
        std::remove_if(candidates.begin(), candidates.end(),
            [](const auto& p) { return !IsLive(p); }),
        candidates.end());
    if (candidates.empty()) co_return std::uint32_t{0};

    std::size_t pick = 0;
    {
        std::lock_guard<std::mutex> lk(m_rr_mutex);
        auto& cursor = m_rr_cursor[BucketKey(group_id, type_id)];
        pick = cursor % candidates.size();
        cursor = pick + 1;  // store the NEXT slot — survives bucket resize
    }
    auto& peer = candidates[pick];
    const auto sid = peer->ServiceId();
    co_await peer->Wire()->SendPacket(wId, std::move(body));
    co_return sid;
}

boost::asio::awaitable<std::size_t>
MessageRouter::BroadcastToGroupType(std::uint8_t group_id,
                                    std::uint8_t type_id,
                                    std::uint16_t wId,
                                    std::vector<std::byte> body)
{
    auto candidates = m_peers.FindByType(group_id, type_id);
    std::size_t fanout = 0;
    for (auto& peer : candidates)
    {
        if (!IsLive(peer)) continue;
        // Copy body per peer — SendPacket consumes it by value.
        std::vector<std::byte> copy = body;
        co_await peer->Wire()->SendPacket(wId, std::move(copy));
        ++fanout;
    }
    co_return fanout;
}

boost::asio::awaitable<std::size_t>
MessageRouter::BroadcastToType(std::uint8_t type_id,
                               std::uint16_t wId,
                               std::vector<std::byte> body)
{
    std::size_t fanout = 0;
    for (const auto& svc : m_peers.Services())
    {
        if (svc.type_id != type_id) continue;
        auto peer = m_peers.Connection(svc.service_id);
        if (!IsLive(peer)) continue;
        std::vector<std::byte> copy = body;
        co_await peer->Wire()->SendPacket(wId, std::move(copy));
        ++fanout;
    }
    co_return fanout;
}

} // namespace tcontrolsvr
