#pragma once

// NPC handlers — F6 Part 2.
//
// Handles NPC dialogue trigger, shop listing, and item purchase.
//
// Wire references:
//   CS_NPCTALK_REQ/ACK    — CSHandler.cpp:3506 / CSSender.cpp:3824
//   CS_NPCITEMLIST_REQ/ACK— CSHandler.cpp:6192 / CSSender.cpp:2756
//   CS_ITEMBUY_REQ/ACK    — CSHandler.cpp:6247 / CSSender.cpp:2942

#include "asio_session.h"
#include <boost/asio/awaitable.hpp>
#include <memory>

namespace tmapsvr {

// F6: open NPC dialogue (trigger quest / shop menu).
// Source: CSHandler.cpp:3506.
boost::asio::awaitable<void> OnNpcTalkReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    struct MapSessionState&              state,
    const tnetlib::DecodedPacket&        packet,
    const struct HandlerContext&         ctx);

// F6: request NPC shop item list.
// Source: CSHandler.cpp:6192.
boost::asio::awaitable<void> OnNpcItemListReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    struct MapSessionState&              state,
    const tnetlib::DecodedPacket&        packet,
    const struct HandlerContext&         ctx);

// F6: buy one item from NPC shop.
// Source: CSHandler.cpp:6247.
boost::asio::awaitable<void> OnItemBuyReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    struct MapSessionState&              state,
    const tnetlib::DecodedPacket&        packet,
    const struct HandlerContext&         ctx);

} // namespace tmapsvr
