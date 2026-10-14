#pragma once
#include "handlers.h"
#include "domain/character.h"
#include <span>

namespace tmapsvr {
// Once after an own-PC HPMP enqueue, with the actual committed/display packet.
boost::asio::awaitable<void> PublishPartyVitals(const std::shared_ptr<tnetlib::AsioSession>&,
    const CharSnapshot&,std::span<const std::byte>,const HandlerContext&);
// After a real SQL/local Ready transition only. Repeat the original entered
// identity ACK; World's native completed-connection branch only refreshes
// current party attributes. This is not an original source Ready ACK.
boost::asio::awaitable<void> RefreshReadyParty(const std::shared_ptr<tnetlib::AsioSession>&,
    const HandlerContext&);
}
