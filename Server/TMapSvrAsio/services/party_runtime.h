#pragma once
#include "handlers.h"
#include "domain/character.h"
#include <span>

namespace tmapsvr {
// Once after an own-PC HPMP enqueue, with the actual committed/display packet.
boost::asio::awaitable<void> PublishPartyVitals(const std::shared_ptr<tnetlib::AsioSession>&,
    const CharSnapshot&,std::span<const std::byte>,const HandlerContext&);
// After real SQL/local Ready only. Existing MANSTAT bytes also let World
// refresh its authoritative membership projection, including partyless zero.
boost::asio::awaitable<void> RefreshReadyParty(const std::shared_ptr<tnetlib::AsioSession>&,
    const HandlerContext&);
}
