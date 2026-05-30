// Combat handlers — the attack → damage → HP / death + EXP slice.
//
// CS_ACTION_REQ covers a player's action on a target. This file ports
// the combat-relevant slice: a normal attack on a monster reduces its
// HP, broadcasts the new health (or its death) to everyone in view, and
// awards the monster's EXP to the killer.
//
// Deliberately bounded: the real damage formula (legacy CalcDamage —
// attacker AP/WAP vs monster DP, crit, attack/defense levels) and the
// skill-execution path (skill data types, recall-mon, PvP) need the
// player combat-stat + skill subsystems, which aren't ported yet. The
// damage applied here is a documented level-scaled placeholder; the
// death + EXP reward are faithful (EXP = MonsterTemplate.wExp).

#include "handlers.h"

#include "domain/character.h"
#include "domain/monster.h"
#include "services/channel_presence.h"
#include "services/char_state_store.h"
#include "services/client_senders.h"
#include "services/monster_chart.h"
#include "services/monster_registry.h"
#include "services/session_registry.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>
#include <exception>
#include <memory>
#include <utility>
#include <vector>

namespace tmapsvr {

namespace {

constexpr std::uint8_t OtMon = 2;   // OBJ_TYPE::OT_MON (NetCode.h:1031)

// Placeholder damage until the real CalcDamage formula lands with the
// player combat-stat layer. Scales with the attacker's level so a
// higher-level player kills faster.
std::uint32_t PlaceholderDamage(std::uint8_t attacker_level)
{
    return 50u + static_cast<std::uint32_t>(attacker_level) * 10u;
}

// Detached coroutine: after a fixed delay, re-insert a fresh copy of a
// killed monster (full HP, the new instance id already assigned) and
// announce it (CS_ADDMON) to everyone in view. Keeps the world from
// emptying out as players grind. The real per-spawn respawn delay (a
// TMONSPAWNCHART column not yet loaded) is a follow-up; a fixed 15s
// stands in. `ctx` outlives this (it's the process-lifetime context).
boost::asio::awaitable<void>
RespawnMonster(MonsterInstance m, const HandlerContext& ctx)
{
    using tnetlib::protocol::MessageId;

    boost::asio::steady_timer t(co_await boost::asio::this_coro::executor);
    t.expires_after(std::chrono::seconds(15));
    co_await t.async_wait(boost::asio::use_awaitable);

    if (!ctx.monster_registry)
        co_return;
    ctx.monster_registry->Insert(m);

    if (!ctx.presence)
        co_return;
    std::uint8_t level = 0;
    if (ctx.monster_chart)
        if (const auto tmpl = ctx.monster_chart->Find(m.wTemplateID))
            level = tmpl->bLevel;

    const auto add = EncodeAddMonAck(m, level, /*country=*/0, /*color=*/0,
                                     /*new_member=*/1);
    std::vector<std::shared_ptr<tnetlib::AsioSession>> watchers;
    ctx.presence->ForEachInChannel(m.bChannel, /*skip=*/0,
        [&](const ChannelPresenceEntry&,
            std::shared_ptr<tnetlib::AsioSession> ws)
        { watchers.push_back(std::move(ws)); });
    for (auto& w : watchers)
        co_await w->SendPacket(
            static_cast<std::uint16_t>(MessageId::CS_ADDMON_ACK), add);

    spdlog::info("respawn: mon tmpl={} reborn id={} on ch={} map={}",
        m.wTemplateID, m.dwInstanceID, m.bChannel, m.wMapID);
}

} // namespace

boost::asio::awaitable<void>
OnActionReq(std::shared_ptr<tnetlib::AsioSession> sess,
            std::vector<std::byte>                body,
            const HandlerContext&                 ctx)
{
    using tnetlib::protocol::MessageId;

    // Wire (legacy CSHandler.cpp:1248):
    //   DWORD obj_id, BYTE obj_type, BYTE action_id, DWORD act_id,
    //   DWORD ani_id, BYTE channel, WORD map_id, WORD skill_id
    wire::Reader r(body.data(), body.size());
    std::uint32_t obj_id = 0, act_id = 0, ani_id = 0;
    std::uint8_t  obj_type = 0, action_id = 0, channel = 0;
    std::uint16_t map_id = 0, skill_id = 0;
    if (!r.Read(obj_id) || !r.Read(obj_type) || !r.Read(action_id) ||
        !r.Read(act_id) || !r.Read(ani_id) || !r.Read(channel) ||
        !r.Read(map_id) || !r.Read(skill_id))
    {
        spdlog::warn("CS_ACTION_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    // This slice only ports attacks on monsters. PC / recall-mon / skill
    // targets route through the (not-yet-ported) skill + PvP subsystems.
    if (obj_type != OtMon || !ctx.monster_registry)
        co_return;

    // Attacker + level (for the placeholder damage).
    std::uint32_t attacker = 0;
    std::uint8_t  attacker_level = 1;
    if (ctx.session_reg)
        if (const auto cid = ctx.session_reg->FindCharIdBySession(sess.get()))
            attacker = *cid;
    if (attacker && ctx.char_state)
        if (const auto s = ctx.char_state->Get(attacker))
            attacker_level = s->bLevel;

    const std::uint32_t dmg = PlaceholderDamage(attacker_level);
    const auto after = ctx.monster_registry->ApplyDamage(obj_id, dmg);
    if (!after)
        co_return;   // monster already gone

    // Collect the watchers in view (presence visitor is synchronous;
    // co_await the sends afterwards), keyed by the monster's channel.
    std::vector<std::shared_ptr<tnetlib::AsioSession>> watchers;
    if (ctx.presence)
        ctx.presence->ForEachInChannel(after->bChannel, /*skip=*/0,
            [&](const ChannelPresenceEntry&,
                std::shared_ptr<tnetlib::AsioSession> ws)
            { watchers.push_back(std::move(ws)); });

    if (after->dwHP > 0)
    {
        // Survived — broadcast the new health bar.
        const auto hp =
            EncodeHpMpAck(obj_id, after->dwMaxHP, after->dwHP, 0, 0);
        for (auto& w : watchers)
            co_await w->SendPacket(
                static_cast<std::uint16_t>(MessageId::CS_HPMP_ACK), hp);
        spdlog::info("CS_ACTION_REQ char={} hit mon={} for {} → {}/{} HP",
            attacker, obj_id, dmg, after->dwHP, after->dwMaxHP);
        co_return;
    }

    // Died — remove it, tell everyone in view, award the kill.
    ctx.monster_registry->Remove(obj_id);

    std::uint16_t exp = 0;
    if (ctx.monster_chart)
        if (const auto t = ctx.monster_chart->Find(after->wTemplateID))
            exp = t->wExp;

    const auto del = EncodeDelMonAck(obj_id, /*exit_map=*/0);
    for (auto& w : watchers)
        co_await w->SendPacket(
            static_cast<std::uint16_t>(MessageId::CS_DELMON_ACK), del);

    // EXP to the killer. The level chart that would trigger a level-up
    // isn't modelled, so this accrues EXP and echoes it back only.
    if (attacker && ctx.char_state)
    {
        ctx.char_state->Update(attacker,
            [&](CharSnapshot& s) { s.dwEXP += exp; });
        if (const auto s = ctx.char_state->Get(attacker))
        {
            const auto exp_ack = EncodeExpAck(s->dwEXP, 0, 0, 0);
            co_await sess->SendPacket(
                static_cast<std::uint16_t>(MessageId::CS_EXP_ACK), exp_ack);
        }
    }

    spdlog::info("CS_ACTION_REQ char={} killed mon={} (tmpl={}) → +{} EXP",
        attacker, obj_id, after->wTemplateID, exp);

    // Schedule its return so the world doesn't empty out as players grind.
    // A fresh instance (new id, full HP) at the same spawn point.
    if (ctx.monster_seq)
    {
        MonsterInstance reborn = *after;
        reborn.dwInstanceID =
            ctx.monster_seq->fetch_add(1, std::memory_order_relaxed);
        reborn.dwHP = reborn.dwMaxHP;
        auto exec = co_await boost::asio::this_coro::executor;
        boost::asio::co_spawn(exec, RespawnMonster(reborn, ctx),
            [](std::exception_ptr ep)
            {
                if (!ep) return;
                try { std::rethrow_exception(ep); }
                catch (const std::exception& e)
                { spdlog::error("respawn coroutine threw: {}", e.what()); }
                catch (...) { spdlog::error("respawn coroutine threw unknown"); }
            });
    }
}

} // namespace tmapsvr
