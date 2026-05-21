#pragma once

// MessageRouter — routing layer on top of PeerRegistry. Hands every
// caller (existing CT_* handlers, admin shell, future orchestration
// commands) a single, well-typed surface for "send this wire frame
// to that peer / that type / those groups", instead of each call site
// open-coding `peers.FindByType(...)` + `for (auto& p : ...) co_await
// p->Wire()->SendPacket(...)` and accidentally diverging in details
// like skipping closed sockets, round-robin state, or aggregating
// fanout counts.
//
// Routing primitives:
//
//   SendToService(sid, wId, body)
//       → one peer by service_id. Returns true on hit, false if the
//         service_id isn't dialed or its socket is closed.
//
//   SendToType(group, type, wId, body)
//       → round-robin across the (group, type) bucket. Cursor is
//         per-bucket so map-1 → map-2 → map-3 → map-1 in the
//         steady-state. Returns the chosen service_id (0 = none live).
//
//   BroadcastToGroupType(group, type, wId, body)
//       → fan-out to every live peer in the (group, type) bucket.
//         Returns the fanout count.
//
//   BroadcastToType(type, wId, body)
//       → fan-out across every group with that type. Mirrors the
//         existing PeersByTypeAll() pattern used by the legacy
//         kick / announce handlers.
//
// Thread model: the cursor map is protected by a mutex because in
// production main.cpp is a single io_context but tests sometimes
// drive routing from helper threads. The hot path (send + await
// completion) doesn't hold the mutex.

#include "control_session.h"

#include <boost/asio/awaitable.hpp>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace tcontrolsvr {

class PeerRegistry;

class MessageRouter
{
public:
    explicit MessageRouter(PeerRegistry& peers);

    boost::asio::awaitable<bool>
        SendToService(std::uint32_t service_id,
                      std::uint16_t wId,
                      std::vector<std::byte> body);

    boost::asio::awaitable<std::uint32_t>
        SendToType(std::uint8_t group_id,
                   std::uint8_t type_id,
                   std::uint16_t wId,
                   std::vector<std::byte> body);

    boost::asio::awaitable<std::size_t>
        BroadcastToGroupType(std::uint8_t group_id,
                             std::uint8_t type_id,
                             std::uint16_t wId,
                             std::vector<std::byte> body);

    boost::asio::awaitable<std::size_t>
        BroadcastToType(std::uint8_t type_id,
                        std::uint16_t wId,
                        std::vector<std::byte> body);

private:
    static std::uint16_t BucketKey(std::uint8_t group_id,
                                   std::uint8_t type_id)
    {
        return static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(group_id) << 8) | type_id);
    }

    PeerRegistry&                                          m_peers;
    std::mutex                                             m_rr_mutex;
    std::unordered_map<std::uint16_t, std::size_t>         m_rr_cursor;
};

} // namespace tcontrolsvr
