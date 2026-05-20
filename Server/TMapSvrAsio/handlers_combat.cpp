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

    // Apply stub damage if target is a monster (obj_type == OT_MON = 2).
    constexpr std::uint8_t OT_MON = 2;
    if (obj_type == OT_MON && ctx.monster_registry && ctx.session_registry)
    {
        const std::int64_t dmg =
            static_cast<std::int64_t>(state.snapshot->level) * 10 + 25;

        const auto new_hp = ctx.monster_registry->ApplyHpDelta(obj_id, -dmg);
        const auto* mon = ctx.monster_registry->Get(obj_id);
        if (!mon) co_return;

        // Broadcast CS_HPMP_ACK to all AOI players
        const auto aoi = ctx.monster_registry->GetNeighborIds(
            mon->pos_x, mon->pos_z);
        for (std::uint32_t pid : aoi)
        {
            auto nbr = ctx.session_registry->Get(pid);
            if (nbr)
                co_await SendHpMpAck(nbr, obj_id, OT_MON,
                    mon->max_hp, new_hp, mon->max_mp, mon->mp);
        }
        // Also send to the attacker's own session
        co_await SendHpMpAck(sess, obj_id, OT_MON,
            mon->max_hp, new_hp, mon->max_mp, mon->mp);

        // Monster death → CS_DELMON_ACK broadcast
        if (new_hp == 0)
        {
            spdlog::info("CS_ACTION_REQ: monster {} killed by uid={}",
                obj_id, state.user_id);
            for (std::uint32_t pid : aoi)
            {
                auto nbr = ctx.session_registry->Get(pid);
                if (nbr)
                    co_await SendDelMonAck(nbr, obj_id, 0u);
            }
            co_await SendDelMonAck(sess, obj_id, 0u);
            ctx.monster_registry->Remove(obj_id);
        }
    }
    // PvP damage is F4 Part 2 (CS_DEFEND_REQ flow)
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

} // namespace tmapsvr
