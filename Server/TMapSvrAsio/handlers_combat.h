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
#include <vector>

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

// Send CS_DEFEND_ACK — broadcasts the outcome of one attack to all
// receivers in AOI. Contains full combat result (hit, damage, positions).
// Source: CSSender.cpp:1262 — SendCS_DEFEND_ACK
//
// F4 Part 3: dynamic skill-damage map is always sent as count=0
// (stub until full skill-calc lands in F4 Part 4).
struct DefendAckParams
{
    std::uint32_t attack_id      = 0;
    std::uint32_t target_id      = 0;
    std::uint8_t  attack_type    = 0;
    std::uint8_t  target_type    = 0;
    std::uint32_t host_id        = 0;
    std::uint8_t  host_type      = 0;
    std::uint32_t act_id         = 0;
    std::uint32_t ani_id         = 0;
    std::uint8_t  hit            = 1;   // 1 = hit
    std::uint8_t  atk_hit        = 1;
    std::uint16_t attack_level   = 0;
    std::uint8_t  attacker_level = 0;
    std::uint32_t pys_min        = 0;
    std::uint32_t pys_max        = 0;
    std::uint32_t mg_min         = 0;
    std::uint32_t mg_max         = 0;
    std::uint8_t  can_select     = 1;
    std::uint8_t  attack_country = 0;
    std::uint8_t  attack_aid     = 0;
    std::uint16_t skill_id       = 0;
    std::uint8_t  skill_level    = 0;
    std::uint8_t  perform        = 1;   // 1 = success
    float         atk_pos_x      = 0.0f, atk_pos_y = 0.0f, atk_pos_z = 0.0f;
    float         def_pos_x      = 0.0f, def_pos_y = 0.0f, def_pos_z = 0.0f;
};

boost::asio::awaitable<void> SendDefendAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    const DefendAckParams&               p);

// Send CS_MONATTACK_ACK — monster is attacking with a skill.
// Simple 5-field broadcast; clients render the attack animation.
// Source: CSSender.cpp:1208 — SendCS_MONATTACK_ACK
boost::asio::awaitable<void> SendMonAttackAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::uint32_t attacker_id,
    std::uint32_t target_id,
    std::uint8_t  attacker_type,   // OT_MON = 2
    std::uint8_t  target_type,     // OT_PC = 1
    std::uint16_t skill_id);

// Send CS_SKILLUSE_ACK — broadcast skill-use result to one receiver.
// Contains attacker stats + ground position + target list.
// Source: CSSender.cpp:1518 — SendCS_SKILLUSE_ACK
struct SkillUseAckParams
{
    std::uint8_t  result         = 0;   // SKILL_SUCCESS=0
    std::uint32_t attack_id      = 0;
    std::uint8_t  attack_type    = 0;
    std::uint16_t skill_id       = 0;
    std::uint16_t back_skill     = 0;
    std::uint8_t  action_id      = 0;
    std::uint32_t act_id         = 0;
    std::uint32_t ani_id         = 0;
    std::uint8_t  skill_level    = 0;
    std::uint16_t attack_level   = 0;
    std::uint8_t  attacker_level = 0;
    std::uint32_t pys_min        = 0;
    std::uint32_t pys_max        = 0;
    std::uint32_t mg_min         = 0;
    std::uint32_t mg_max         = 0;
    std::uint16_t trans_hp       = 0;
    std::uint16_t trans_mp       = 0;
    std::uint8_t  curse_prob     = 0;
    std::uint8_t  equip_special  = 0;
    std::uint8_t  can_select     = 1;
    std::uint8_t  attack_country = 0;
    std::uint8_t  attack_aid     = 0;
    std::uint8_t  cp             = 0;
    float         gnd_px = 0.0f, gnd_py = 0.0f, gnd_pz = 0.0f;
    std::vector<std::uint32_t> target_ids;
    std::vector<std::uint8_t>  target_types;
};

boost::asio::awaitable<void> SendSkillUseAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    const SkillUseAckParams&             p);

// Send CS_EXP_ACK — experience gain notification.
// Source: CSSender.cpp:1375
boost::asio::awaitable<void> SendExpAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::uint32_t current_exp,
    std::uint32_t prev_level_exp,
    std::uint32_t next_level_exp,
    std::uint32_t soul_exp = 0);

// Send CS_REVIVAL_ACK — confirms revival to the revived player and AOI.
// Source: CSSender.cpp:1404
boost::asio::awaitable<void> SendRevivalAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::uint32_t char_id,
    float px, float py, float pz);

} // namespace tmapsvr
