#pragma once

// BroadcastToGuildMembers — common path used by OnGuildFameAck +
// OnGuildContributionAck + any future cluster-wide guild event.
// Walks every char_id passed in, looks up the matching TChar (to
// learn the main_server_id), routes the packet to the peer whose
// wID's LOBYTE matches.
//
// Pulled out of OnGuildFameAck so the same routing logic doesn't
// get re-implemented in each handler. The legacy module wraps this
// in `for(it=m_mapTMember.begin(); it!=m_mapTMember.end(); …)` at
// every broadcast site; we centralise here.
//
// Each per-member send is co_await'd serially. Parallel co_spawn
// would race on the same peer socket for siblings on the same map
// server — serial is the right shape until we add a per-peer
// outbound queue (W3a-5+).

#include "../peer_session.h"
#include "../services/char_registry.h"
#include "../services/peer_registry.h"

#include <boost/asio/awaitable.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace tworldsvr {

// Coroutine factory the broadcast invokes per online recipient.
// `peer` is the recipient's main-map peer session, `char_id` the
// member's id, `key` their session key (so the sender can route
// the packet on the wire).
using GuildPacketBuilder = std::function<
    boost::asio::awaitable<void>(std::shared_ptr<PeerSession> peer,
                                  std::uint32_t char_id,
                                  std::uint32_t key)>;

// Returns the number of recipients the packet actually reached
// (offline members + peers not in PeerRegistry are skipped, not
// errors). The function is in the services namespace so handler
// files can include it via "../services/guild_broadcast.h"
// alongside their other registry includes.
boost::asio::awaitable<std::size_t> BroadcastToGuildMembers(
    CharRegistry&                       chars,
    PeerRegistry&                       peers,
    const std::vector<std::uint32_t>&   member_char_ids,
    GuildPacketBuilder                  build_and_send);

} // namespace tworldsvr
