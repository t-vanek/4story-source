#pragma once

// Map-state handlers and AOI wire helpers.
//
// These functions operate on live player sessions via IMapState +
// ISessionRegistry. They are called from:
//   * handlers.cpp — Dispatch routes CS_MOVE_REQ → OnMoveReq
//   * map_server.cpp — HandleConnection teardown sends CS_LEAVE_ACK
//
// Wire format references:
//   CS_ENTER_ACK  — CSSender.cpp:395  SendCS_ENTER_ACK
//   CS_MOVE_ACK   — CSSender.cpp:599  SendCS_MOVE_ACK
//   CS_LEAVE_ACK  — CSSender.cpp (character departure broadcast)

#include "asio_session.h"
#include "legacy_port/types_layer2.h"

#include <boost/asio/awaitable.hpp>
#include <cstdint>
#include <memory>

namespace tmapsvr {

// Send CS_ENTER_ACK — broadcast one player's visual state to a receiver.
// `new_member` = TRUE (legacy bNewMember) for brand-new connections.
// Source: CSSender.cpp:395 — SendCS_ENTER_ACK
boost::asio::awaitable<void> SendEnterAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    const legacy::PlayerPresence&         presence,
    bool                                  new_member);

// Send CS_MOVE_ACK — broadcast a movement update to one receiver.
// Source: CSSender.cpp:599 — SendCS_MOVE_ACK
boost::asio::awaitable<void> SendMoveAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::uint32_t char_id,
    float pos_x, float pos_y, float pos_z,
    std::uint16_t pitch, std::uint16_t dir,
    std::uint8_t  mouse_dir, std::uint8_t key_dir,
    std::uint8_t  action,    float speed);

// Send CS_LEAVE_ACK — tells receiver that char_id has left their AOI.
boost::asio::awaitable<void> SendLeaveAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::uint32_t char_id);

} // namespace tmapsvr
