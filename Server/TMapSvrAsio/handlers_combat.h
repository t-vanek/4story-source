#pragma once

// Combat handlers and wire helpers.
//
// F4 Part 1 scope:
//   * CS_ACTION_REQ  — parse, stub damage, CS_ACTION_ACK + CS_HPMP_ACK
//   * CS_SKILLUSE_REQ — parse + log (full skill-calc is F4 Part 2)
//   * SendAddMonAck  — CS_ADDMON_ACK (monster appears in AOI)
//   * SendDelMonAck  — CS_DELMON_ACK (monster disappears)
//   * SendHpMpAck    — CS_HPMP_ACK  (HP/MP update after damage)
//
// Wire format references:
//   CS_ACTION_REQ/ACK — CSHandler.cpp:1248 / CSSender.cpp:1196
//   CS_SKILLUSE_REQ/ACK — CSHandler.cpp:2459 / CSSender.cpp:1552
//   CS_ADDMON_ACK     — CSSender.cpp:928
//   CS_DELMON_ACK     — CSSender.cpp:1016
//   CS_HPMP_ACK       — CSSender.cpp:1325

#include "asio_session.h"
#include "monster_state.h"

#include <boost/asio/awaitable.hpp>
#include <cstdint>
#include <memory>

namespace tmapsvr {

// Send CS_ADDMON_ACK — broadcast monster appearance to one receiver.
// Called from OnConReadyReq (monsters in AOI when entering map) and
// from monster spawn (notifies all AOI players).
// Source: CSSender.cpp:928 — SendCS_ADDMON_ACK
boost::asio::awaitable<void> SendAddMonAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    const MonsterState&                   mon,
    bool                                  new_member);

// Send CS_DELMON_ACK — broadcast monster departure to one receiver.
// Called when a monster dies, despawns, or leaves AOI.
// Source: CSSender.cpp:1016 — SendCS_DELMON_ACK
boost::asio::awaitable<void> SendDelMonAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::uint32_t                         instance_id,
    std::uint8_t                          exit_map);

// Send CS_HPMP_ACK — HP/MP update broadcast to one receiver.
// Sent after any damage or heal event.
// Source: CSSender.cpp:1325 — SendCS_HPMP_ACK
boost::asio::awaitable<void> SendHpMpAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::uint32_t char_or_mon_id,
    std::uint8_t  type,           // 0 = player, 1 = monster
    std::uint32_t max_hp,
    std::uint32_t hp,
    std::uint32_t max_mp,
    std::uint32_t mp);

} // namespace tmapsvr
