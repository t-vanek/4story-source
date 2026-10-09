#pragma once
#include <boost/asio/steady_timer.hpp>
#include <cstdint>
#include <memory>

namespace tworldsvr {
class PeerSession;
enum class MainHandoffPhase { Release, Enter, Confirm };
// World coordination only. This does not grant mutable PostgreSQL ownership.
// Exact socket objects fence replies against replacement peers with reused IDs.
struct MainHandoff {
    MainHandoffPhase phase{MainHandoffPhase::Release};
    std::uint8_t source_id{}, target_id{};
    std::weak_ptr<PeerSession> source, target;
    std::shared_ptr<boost::asio::steady_timer> deadline;
};
}
