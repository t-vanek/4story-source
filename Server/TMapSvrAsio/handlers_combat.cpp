// Combat wire helpers and CS_ACTION_REQ / CS_SKILLUSE_REQ handlers.
//
// F4 Part 1: wire serializers (AddMon, DelMon, HpMp) + action/skill
// request parsers with stub damage. Actual damage formulas land in
// F4 Part 2 when stat lookup tables are ported.
//
// Legacy references:
//   CSHandler.cpp:1248  — OnCS_ACTION_REQ
//   CSHandler.cpp:2459  — OnCS_SKILLUSE_REQ
//   CSSender.cpp:928    — SendCS_ADDMON_ACK
//   CSSender.cpp:1016   — SendCS_DELMON_ACK
//   CSSender.cpp:1325   — SendCS_HPMP_ACK
//   CSSender.cpp:1196   — SendCS_ACTION_ACK
//   CSSender.cpp:1552   — SendCS_SKILLUSE_ACK

#include "handlers_combat.h"
#include "handlers.h"
#include "spawn_manager.h"
#include "services/session_registry.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

namespace tmapsvr {

using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

// ---------------------------------------------------------------------------
// CS_ADDMON_ACK serializer
// ---------------------------------------------------------------------------
//
// Wire order: CSSender.cpp:928-973 — SendCS_ADDMON_ACK
// F4 Part 1 stubs: maintained skills (count=0), region=0.

boost::asio::awaitable<void>
SendAddMonAck(std::shared_ptr<tnetlib::AsioSession> sess,
              const MonsterState&                   mon,
              bool                                  new_member)
{
    std::vector<std::byte> body;
    body.reserve(64);

    wire::WritePOD<std::uint32_t>(body, mon.instance_id); // dwMonID
    wire::WritePOD<std::uint16_t>(body, mon.template_id); // wMonTemplateID
    wire::WritePOD<std::uint8_t> (body, mon.level);
    wire::WritePOD<std::uint32_t>(body, mon.max_hp);
    wire::WritePOD<std::uint32_t>(body, mon.hp);
    wire::WritePOD<std::uint32_t>(body, mon.max_mp);
    wire::WritePOD<std::uint32_t>(body, mon.mp);
    wire::WritePOD<float>        (body, mon.pos_x);
    wire::WritePOD<float>        (body, mon.pos_y);
    wire::WritePOD<float>        (body, mon.pos_z);
    wire::WritePOD<std::uint16_t>(body, 0u);              // wPitch
    wire::WritePOD<std::uint16_t>(body, mon.dir);
    wire::WritePOD<std::uint8_t> (body, 0u);              // bMouseDIR
    wire::WritePOD<std::uint8_t> (body, 0u);              // bKeyDIR
    wire::WritePOD<std::uint8_t> (body, mon.action);
    wire::WritePOD<std::uint8_t> (body, mon.mode);
    wire::WritePOD<std::uint8_t> (body, new_member ? 1u : 0u);
    wire::WritePOD<std::uint8_t> (body, mon.country);
    wire::WritePOD<std::uint8_t> (body, 0u);              // bColor
    wire::WritePOD<std::uint32_t>(body, 0u);              // dwRegion (F4b)

    // Maintained skills — F4b (count=0)
    wire::WritePOD<std::uint8_t>(body, 0u);

    co_await sess->SendPacket(
        ToUint16(MessageId::CS_ADDMON_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// ---------------------------------------------------------------------------
// CS_DELMON_ACK serializer
// ---------------------------------------------------------------------------

boost::asio::awaitable<void>
SendDelMonAck(std::shared_ptr<tnetlib::AsioSession> sess,
              std::uint32_t                         instance_id,
              std::uint8_t                          exit_map)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint32_t>(body, instance_id);
    wire::WritePOD<std::uint8_t> (body, exit_map);
    co_await sess->SendPacket(
        ToUint16(MessageId::CS_DELMON_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// ---------------------------------------------------------------------------
// CS_HPMP_ACK serializer
// ---------------------------------------------------------------------------

boost::asio::awaitable<void>
SendHpMpAck(std::shared_ptr<tnetlib::AsioSession> sess,
            std::uint32_t char_or_mon_id,
            std::uint8_t  type,
            std::uint32_t max_hp, std::uint32_t hp,
            std::uint32_t max_mp, std::uint32_t mp)
{
    std::vector<std::byte> body;
    body.reserve(20);
    wire::WritePOD<std::uint32_t>(body, char_or_mon_id);
    wire::WritePOD<std::uint8_t> (body, type);
    wire::WritePOD<std::uint32_t>(body, max_hp);
    wire::WritePOD<std::uint32_t>(body, hp);
    wire::WritePOD<std::uint32_t>(body, max_mp);
    wire::WritePOD<std::uint32_t>(body, mp);
    co_await sess->SendPacket(
        ToUint16(MessageId::CS_HPMP_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// ---------------------------------------------------------------------------
// CS_ACTION_REQ handler
// ---------------------------------------------------------------------------
//
// Wire body: DWORD dwObjID, BYTE bObjType, BYTE bActionID, DWORD dwActID,
//            DWORD dwAniID, BYTE bChannel, WORD wMapID, WORD wSkillID
// Source: CSHandler.cpp:1248-1256
//
// F4 Part 1:
//   §1 malformed body → drop
//   §2 not connected / no snapshot → drop
//   §3 monster target + damage calc STUB → CS_ACTION_ACK + CS_HPMP_ACK
//   §4 player target → CS_ACTION_ACK (PvP damage calc is F4 Part 2)
//
// Damage stub: level × 10 + 25 (placeholder until stat tables are ported).

boost::asio::awaitable<void>
OnActionReq(std::shared_ptr<tnetlib::AsioSession> sess,
            MapSessionState&                     state,
            const tnetlib::DecodedPacket&        packet,
            const HandlerContext&                ctx)
{
    if (!state.connected || !state.snapshot) co_return;

    wire::Reader r(packet.body.data(), packet.body.size());
    std::uint32_t obj_id  = 0;
    std::uint8_t  obj_type = 0, action_id = 0;
    std::uint32_t act_id  = 0, ani_id = 0;
    std::uint8_t  channel = 0;
    std::uint16_t map_id  = 0, skill_id = 0;

    if (!r.Read(obj_id)   || !r.Read(obj_type) || !r.Read(action_id) ||
        !r.Read(act_id)   || !r.Read(ani_id)   || !r.Read(channel)   ||
        !r.Read(map_id)   || !r.Read(skill_id))
    {
        spdlog::warn("CS_ACTION_REQ malformed uid={}", state.user_id);
        co_return;
    }

    spdlog::debug("CS_ACTION_REQ uid={} → obj_id={} type={} action={}",
        state.user_id, obj_id, obj_type, action_id);

    // Send CS_ACTION_ACK back to the attacker.
    // Wire: BYTE bResult, DWORD dwObjID, BYTE bObjType, BYTE bActionID,
    //       DWORD dwActID, DWORD dwAniID, WORD wSkillID
    // Source: CSSender.cpp:1196
    {
        std::vector<std::byte> ack;
        ack.reserve(14);
        wire::WritePOD<std::uint8_t> (ack, 0u);         // SKILL_SUCCESS
        wire::WritePOD<std::uint32_t>(ack, obj_id);
        wire::WritePOD<std::uint8_t> (ack, obj_type);
        wire::WritePOD<std::uint8_t> (ack, action_id);
        wire::WritePOD<std::uint32_t>(ack, act_id);
        wire::WritePOD<std::uint32_t>(ack, ani_id);
        wire::WritePOD<std::uint16_t>(ack, skill_id);
        co_await sess->SendPacket(
            ToUint16(MessageId::CS_ACTION_ACK),
            std::span<const std::byte>(ack.data(), ack.size()));
    }

    // Apply damage if target is a monster (obj_type == OT_MON = 2).
    constexpr std::uint8_t OT_MON = 2;
    if (obj_type == OT_MON && ctx.monster_registry && ctx.session_registry)
    {
        // Damage formula: use level chart if wired, else CalcBaseDamage stub
        const std::uint32_t dmg = ctx.level_chart
            ? CalcBaseDamage(state.snapshot->level)
            : (static_cast<std::uint32_t>(state.snapshot->level) * 10 + 25);

        const auto new_hp = ctx.monster_registry->ApplyHpDelta(
            obj_id, -static_cast<std::int64_t>(dmg));
        const auto* mon = ctx.monster_registry->Get(obj_id);
        if (!mon) co_return;

        // Broadcast CS_HPMP_ACK to AOI players
        const auto aoi = ctx.monster_registry->GetNeighborIds(
            mon->pos_x, mon->pos_z);
        for (std::uint32_t pid : aoi)
        {
            auto nbr = ctx.session_registry->Get(pid);
            if (nbr)
                co_await SendHpMpAck(nbr, obj_id, OT_MON,
                    mon->max_hp, new_hp, mon->max_mp, mon->mp);
        }
        co_await SendHpMpAck(sess, obj_id, OT_MON,
            mon->max_hp, new_hp, mon->max_mp, mon->mp);

        // Monster death
        if (new_hp == 0)
        {
            const std::uint16_t spawn_id = mon->spawn_id;
            spdlog::info("CS_ACTION_REQ: monster {} (spawn={}) killed by uid={}",
                obj_id, spawn_id, state.user_id);

            for (std::uint32_t pid : aoi)
            {
                auto nbr = ctx.session_registry->Get(pid);
                if (nbr) co_await SendDelMonAck(nbr, obj_id, 0u);
            }
            co_await SendDelMonAck(sess, obj_id, 0u);

            ctx.monster_registry->Remove(obj_id);

            // Notify spawn manager for respawn scheduling
            if (ctx.spawn_manager)
                ctx.spawn_manager->OnMonsterDied(obj_id, spawn_id);
        }
    }
    // PvP damage (CS_DEFEND_REQ flow) is F4 Part 3
}

// ---------------------------------------------------------------------------
// CS_SKILLUSE_REQ handler — F4 Part 1: parse + log only
// ---------------------------------------------------------------------------
//
// Wire body: DWORD dwAttackID, BYTE bAttackType, BYTE bChannel, WORD wMapID,
//            WORD wSkillID, BYTE bActionID, DWORD dwActID, DWORD dwAniID,
//            FLOAT fPosX/Y/Z, BYTE bCount, [DWORD+BYTE+BYTE per target]
// Source: CSHandler.cpp:2459-2471

boost::asio::awaitable<void>
OnSkillUseReq(std::shared_ptr<tnetlib::AsioSession> /*sess*/,
              MapSessionState&                     state,
              const tnetlib::DecodedPacket&        packet,
              const HandlerContext&                /*ctx*/)
{
    if (!state.connected || !state.snapshot) co_return;

    wire::Reader r(packet.body.data(), packet.body.size());
    std::uint32_t attack_id = 0;
    std::uint8_t  attack_type = 0, channel = 0;
    std::uint16_t map_id = 0, skill_id = 0;
    std::uint8_t  action_id = 0;
    std::uint32_t act_id = 0, ani_id = 0;
    float         px = 0, py = 0, pz = 0;
    std::uint8_t  count = 0;

    if (!r.Read(attack_id) || !r.Read(attack_type) || !r.Read(channel) ||
        !r.Read(map_id)    || !r.Read(skill_id)    || !r.Read(action_id) ||
        !r.Read(act_id)    || !r.Read(ani_id)      ||
        !r.Read(px)        || !r.Read(py)           || !r.Read(pz)  ||
        !r.Read(count))
    {
        spdlog::warn("CS_SKILLUSE_REQ malformed uid={}", state.user_id);
        co_return;
    }

    spdlog::debug("CS_SKILLUSE_REQ uid={} skill_id={} target_count={}",
        state.user_id, skill_id, count);

    // Full skill execution (CS_SKILLUSE_ACK + CS_DEFEND_REQ simulation)
    // is PENDING F4 Part 2. The 30+ field skill ACK requires the skill
    // data chart (TSKILLCHART) and stat-based damage formulas.
}

// ---------------------------------------------------------------------------
// CS_DEFEND_ACK serializer
// ---------------------------------------------------------------------------
//
// Wire order: CSSender.cpp:1262-1294 — SendCS_DEFEND_ACK
// F4 Part 3: skill damage map always sent as count=0 (full calc F4 Part 4).

boost::asio::awaitable<void>
SendDefendAck(std::shared_ptr<tnetlib::AsioSession> sess,
              const DefendAckParams&               p)
{
    std::vector<std::byte> body;
    body.reserve(72);

    wire::WritePOD<std::uint32_t>(body, p.attack_id);
    wire::WritePOD<std::uint32_t>(body, p.target_id);
    wire::WritePOD<std::uint8_t> (body, p.attack_type);
    wire::WritePOD<std::uint8_t> (body, p.target_type);
    wire::WritePOD<std::uint32_t>(body, p.host_id);
    wire::WritePOD<std::uint8_t> (body, p.host_type);
    wire::WritePOD<std::uint32_t>(body, p.act_id);
    wire::WritePOD<std::uint32_t>(body, p.ani_id);
    wire::WritePOD<std::uint8_t> (body, 0u);  // bIsMaintain
    wire::WritePOD<std::uint32_t>(body, 0u);  // dwMaintainTick
    wire::WritePOD<std::uint8_t> (body, p.hit);
    wire::WritePOD<std::uint8_t> (body, p.atk_hit);
    wire::WritePOD<std::uint16_t>(body, p.attack_level);
    wire::WritePOD<std::uint8_t> (body, p.attacker_level);
    wire::WritePOD<std::uint32_t>(body, p.pys_min);
    wire::WritePOD<std::uint32_t>(body, p.pys_max);
    wire::WritePOD<std::uint32_t>(body, p.mg_min);
    wire::WritePOD<std::uint32_t>(body, p.mg_max);
    wire::WritePOD<std::uint8_t> (body, p.can_select);
    wire::WritePOD<std::uint8_t> (body, 0u);  // bCancelCharge
    wire::WritePOD<std::uint8_t> (body, p.attack_country);
    wire::WritePOD<std::uint8_t> (body, p.attack_aid);
    wire::WritePOD<std::uint16_t>(body, p.skill_id);
    wire::WritePOD<std::uint8_t> (body, p.skill_level);
    wire::WritePOD<std::uint16_t>(body, 0u);  // wBackSkillID
    wire::WritePOD<std::uint8_t> (body, p.perform);
    wire::WritePOD<float>        (body, p.atk_pos_x);
    wire::WritePOD<float>        (body, p.atk_pos_y);
    wire::WritePOD<float>        (body, p.atk_pos_z);
    wire::WritePOD<float>        (body, p.def_pos_x);
    wire::WritePOD<float>        (body, p.def_pos_y);
    wire::WritePOD<float>        (body, p.def_pos_z);
    wire::WritePOD<std::uint8_t> (body, 0u);  // skill damage map count = 0

    co_await sess->SendPacket(
        ToUint16(MessageId::CS_DEFEND_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// ---------------------------------------------------------------------------
// CS_MONATTACK_ACK serializer
// ---------------------------------------------------------------------------
//
// Monster broadcasts its attack intent. Wire: CSSender.cpp:1208.

boost::asio::awaitable<void>
SendMonAttackAck(std::shared_ptr<tnetlib::AsioSession> sess,
                 std::uint32_t attacker_id,
                 std::uint32_t target_id,
                 std::uint8_t  attacker_type,
                 std::uint8_t  target_type,
                 std::uint16_t skill_id)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint32_t>(body, attacker_id);
    wire::WritePOD<std::uint32_t>(body, target_id);
    wire::WritePOD<std::uint8_t> (body, attacker_type);
    wire::WritePOD<std::uint8_t> (body, target_type);
    wire::WritePOD<std::uint16_t>(body, skill_id);
    co_await sess->SendPacket(
        ToUint16(MessageId::CS_MONATTACK_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// ---------------------------------------------------------------------------
// CS_DEFEND_REQ handler
// ---------------------------------------------------------------------------
//
// Wire body (33 fields, 65+ bytes):
//   DWORD dwHostID, dwAttackID, dwTargetID, BYTE bAttackType, bTargetType,
//   WORD wAttackPartyID, DWORD dwActID, dwAniID, BYTE bChannel,
//   WORD wMapID, BYTE bAttackerLevel, DWORD ×4 damage, WORD ×2 trans,
//   BYTE ×6 flags, WORD wAttackLevel, BYTE bCP, WORD wSkillID, BYTE bSkillLevel,
//   FLOAT ×3 atk pos, FLOAT ×3 def pos, DWORD dwRemainTick
//
// F4 Part 3: validates that attacker and target exist, then broadcasts
// CS_DEFEND_ACK to AOI. Full anti-cheat damage capping is F4 Part 4.
//
// Source: CSHandler.cpp:1485-1518

boost::asio::awaitable<void>
OnDefendReq(std::shared_ptr<tnetlib::AsioSession> sess,
            MapSessionState&                     state,
            const tnetlib::DecodedPacket&        packet,
            const HandlerContext&                ctx)
{
    if (!state.connected || !state.snapshot) co_return;

    wire::Reader r(packet.body.data(), packet.body.size());
    std::uint32_t host_id = 0, attack_id = 0, target_id = 0;
    std::uint8_t  attack_type = 0, target_type = 0;
    std::uint16_t attack_party_id = 0;
    std::uint32_t act_id = 0, ani_id = 0;
    std::uint8_t  channel = 0;
    std::uint16_t map_id = 0;
    std::uint8_t  attacker_level = 0;
    std::uint32_t pys_min = 0, pys_max = 0, mg_min = 0, mg_max = 0;
    std::uint16_t trans_hp = 0, trans_mp = 0;
    std::uint8_t  curse_prob = 0, equip_special = 0, can_select = 0;
    std::uint8_t  atk_country = 0, atk_aid = 0;
    std::uint16_t attack_level = 0;
    std::uint8_t  cp = 0;
    std::uint16_t skill_id = 0;
    std::uint8_t  skill_level = 0;
    float         atk_px = 0, atk_py = 0, atk_pz = 0;
    float         def_px = 0, def_py = 0, def_pz = 0;
    std::uint32_t remain_tick = 0;

    if (!r.Read(host_id)     || !r.Read(attack_id)    || !r.Read(target_id) ||
        !r.Read(attack_type) || !r.Read(target_type)  ||
        !r.Read(attack_party_id) || !r.Read(act_id)   || !r.Read(ani_id) ||
        !r.Read(channel)     || !r.Read(map_id)       || !r.Read(attacker_level) ||
        !r.Read(pys_min)     || !r.Read(pys_max)      ||
        !r.Read(mg_min)      || !r.Read(mg_max)       ||
        !r.Read(trans_hp)    || !r.Read(trans_mp)     ||
        !r.Read(curse_prob)  || !r.Read(equip_special) || !r.Read(can_select) ||
        !r.Read(atk_country) || !r.Read(atk_aid)      ||
        !r.Read(attack_level)|| !r.Read(cp)            ||
        !r.Read(skill_id)    || !r.Read(skill_level)   ||
        !r.Read(atk_px)      || !r.Read(atk_py)        || !r.Read(atk_pz) ||
        !r.Read(def_px)      || !r.Read(def_py)        || !r.Read(def_pz) ||
        !r.Read(remain_tick))
    {
        spdlog::warn("CS_DEFEND_REQ malformed uid={}", state.user_id);
        co_return;
    }

    spdlog::debug("CS_DEFEND_REQ uid={} attacker={} target={} skill={}",
        state.user_id, attack_id, target_id, skill_id);

    // Build defend ACK from parsed fields
    DefendAckParams dp{};
    dp.attack_id      = attack_id;
    dp.target_id      = target_id;
    dp.attack_type    = attack_type;
    dp.target_type    = target_type;
    dp.host_id        = host_id;
    dp.act_id         = act_id;
    dp.ani_id         = ani_id;
    dp.hit            = 1;
    dp.atk_hit        = 1;
    dp.attack_level   = attack_level;
    dp.attacker_level = attacker_level;
    dp.pys_min        = pys_min;
    dp.pys_max        = pys_max;
    dp.mg_min         = mg_min;
    dp.mg_max         = mg_max;
    dp.can_select     = can_select;
    dp.attack_country = atk_country;
    dp.attack_aid     = atk_aid;
    dp.skill_id       = skill_id;
    dp.skill_level    = skill_level;
    dp.perform        = 1;
    dp.atk_pos_x = atk_px; dp.atk_pos_y = atk_py; dp.atk_pos_z = atk_pz;
    dp.def_pos_x = def_px; dp.def_pos_y = def_py; dp.def_pos_z = def_pz;

    // Broadcast CS_DEFEND_ACK to all AOI players (attacker's AOI)
    if (ctx.session_registry && ctx.map_state && state.snapshot)
    {
        const auto aoi = ctx.map_state->GetNeighborIds(
            state.snapshot->position.pos_x,
            state.snapshot->position.pos_z);
        for (std::uint32_t pid : aoi)
        {
            auto nbr = ctx.session_registry->Get(pid);
            if (nbr) co_await SendDefendAck(nbr, dp);
        }
    }
    co_await SendDefendAck(sess, dp);

    // Apply damage to target if it is a player (OT_PC = 1)
    constexpr std::uint8_t OT_PC = 1;
    if (target_type == OT_PC && ctx.player_hp)
    {
        const std::int64_t dmg = static_cast<std::int64_t>(pys_max);
        const std::uint32_t new_hp =
            ctx.player_hp->ApplyHpDelta(target_id, -dmg);
        const auto* v = ctx.player_hp->Get(target_id);
        if (v && ctx.session_registry && ctx.map_state)
        {
            auto target_sess = ctx.session_registry->Get(target_id);
            if (target_sess)
                co_await SendHpMpAck(target_sess, target_id, OT_PC,
                    v->max_hp, new_hp, v->max_mp, v->mp);
        }
    }
}

} // namespace tmapsvr
