// Combat handlers — the faithful two-packet attack model.
//
// A 4Story attack is two messages, mirrored here from the legacy
// CSHandler.cpp:
//
//   CS_ACTION_REQ  (OnCS_ACTION_REQ, CSHandler.cpp:1233) — the *animation*.
//                  The server broadcasts the action to everyone in view
//                  (CS_ACTION_ACK) and computes no damage.
//   CS_DEFEND_REQ  (OnCS_DEFEND_REQ, CSHandler.cpp:1438) — the *damage*.
//                  The client sends its attack powers (the weapon + stat
//                  derived phys/magic min/max it rolled client-side); the
//                  server rolls the final number against the target's
//                  server-side defense (CTObjBase::Defend) and applies it.
//
// This wave ports the physical player→monster path end to end: roll the
// real damage (services/damage_formula.h, faithful to CalcDamage's A/B/rand
// core) against the monster's TMONATTRCHART defense, drop its HP, and on
// death award EXP + respawn it.
//
// Deliberately bounded, each documented at its site: magic damage,
// multi-entry skill-data effects (heals / %-max-HP / transHP-MP / curse),
// crit / miss / block selection, PvP (OT_PC defender), and the real player
// defense (needs the equipment/stat layer) are later waves. Until they
// land the monster→player direction (services/monster_ai.cpp) rolls the
// same formula against a zero player defense.

#include "handlers.h"

#include "domain/character.h"
#include "domain/monster.h"
#include "services/channel_presence.h"
#include "services/char_state_store.h"
#include "services/client_senders.h"
#include "services/corpse_registry.h"
#include "services/damage_formula.h"
#include "services/loot.h"
#include "services/mon_item_chart.h"
#include "services/money.h"
#include "services/monster_chart.h"
#include "services/monster_registry.h"
#include "services/quest_chart.h"
#include "services/quest_engine.h"
#include "services/quest_log.h"
#include "services/quest_service.h"
#include "services/session_registry.h"
#include "domain/position.h"
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
#include <random>
#include <utility>
#include <vector>

namespace tmapsvr {

namespace {

constexpr std::uint8_t OtMon       = 2;   // OBJ_TYPE::OT_MON  (NetCode.h:1031)
constexpr std::uint8_t kSkillOk    = 0;   // SKILL_SUCCESS — action validated

// One process-wide RNG for the damage roll. rand_below(n) → [0, n),
// matching the legacy `rand() % n`. thread_local so concurrent SOCI /
// io threads don't share state.
std::uint32_t RandBelow(std::uint32_t n)
{
    if (n <= 1) return 0;
    thread_local std::mt19937 rng{ std::random_device{}() };
    return std::uniform_int_distribution<std::uint32_t>(0, n - 1)(rng);
}

// Collect the sessions that can see something on `channel`. The presence
// visitor is synchronous; we snapshot the sessions and co_await the sends
// afterwards. Channel-wide for now (bounded by population); spatial AOI
// around the target position is the follow-up.
std::vector<std::shared_ptr<tnetlib::AsioSession>>
WatchersOnChannel(const HandlerContext& ctx, std::uint8_t channel)
{
    std::vector<std::shared_ptr<tnetlib::AsioSession>> watchers;
    if (ctx.presence)
        ctx.presence->ForEachInChannel(channel, /*skip=*/0,
            [&](const ChannelPresenceEntry&,
                std::shared_ptr<tnetlib::AsioSession> ws)
            { watchers.push_back(std::move(ws)); });
    return watchers;
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
    for (auto& w : WatchersOnChannel(ctx, m.bChannel))
        co_await w->SendPacket(
            static_cast<std::uint16_t>(MessageId::CS_ADDMON_ACK), add);

    spdlog::info("respawn: mon tmpl={} reborn id={} on ch={} map={}",
        m.wTemplateID, m.dwInstanceID, m.bChannel, m.wMapID);
}

// Apply a resolved physical hit to a monster: drop HP, broadcast the new
// health bar, or — on death — remove it, award EXP to the killer, and
// schedule its respawn. Shared by CS_DEFEND_REQ (and, later, the skill
// path). `attacker` is the killer's char id (0 = unknown, no EXP).
boost::asio::awaitable<void>
HitMonster(std::shared_ptr<tnetlib::AsioSession> sess,
           std::uint32_t obj_id, std::uint32_t dmg, std::uint32_t attacker,
           const HandlerContext& ctx)
{
    using tnetlib::protocol::MessageId;

    const auto after = ctx.monster_registry->ApplyDamage(obj_id, dmg);
    if (!after)
        co_return;   // monster already gone

    const auto watchers = WatchersOnChannel(ctx, after->bChannel);

    if (after->dwHP > 0)
    {
        const auto hp =
            EncodeHpMpAck(obj_id, after->dwMaxHP, after->dwHP, 0, 0);
        for (auto& w : watchers)
            co_await w->SendPacket(
                static_cast<std::uint16_t>(MessageId::CS_HPMP_ACK), hp);
        spdlog::info("defend: char={} hit mon={} for {} → {}/{} HP",
            attacker, obj_id, dmg, after->dwHP, after->dwMaxHP);
        co_return;
    }

    // Died — remove it, tell everyone in view, award the kill.
    ctx.monster_registry->Remove(obj_id);

    // EXP is awarded on the kill; money + items are left on the corpse for
    // the player to loot (faithful CTMonster::AddItem on death fills the
    // corpse's INVEN_DEFAULT — 4Story loots via the corpse window, it does
    // not auto-loot or drop on the ground). All three come from the monster
    // template + its drop table.
    std::uint16_t exp = 0;
    std::uint16_t mon_kind = 0;         // TMONSTERCHART.wKind — quest HUNT key
    std::uint32_t money = 0;            // cooper, left on the corpse
    std::vector<std::uint16_t> drops;   // dropped item template ids
    if (ctx.monster_chart)
        if (const auto t = ctx.monster_chart->Find(after->wTemplateID))
        {
            exp      = t->wExp;
            mon_kind = t->wKind;
            money = RollMoneyDrop(t->bMoneyProb, t->dwMinMoney,
                                  t->dwMaxMoney, RandBelow);
            if (ctx.mon_item_chart)
                drops = RollItemDrops(
                    ctx.mon_item_chart->ForMon(after->wTemplateID),
                    t->bItemProb, t->bDropCount, /*add_item_drop=*/0,
                    RandBelow);
        }

    const auto del = EncodeDelMonAck(obj_id, /*exit_map=*/0);
    for (auto& w : watchers)
        co_await w->SendPacket(
            static_cast<std::uint16_t>(MessageId::CS_DELMON_ACK), del);

    // Stash the loot on the corpse, keyed by the dead instance id the
    // client passes in CS_MONITEMLIST_REQ. Skip an empty corpse (nothing
    // dropped → no loot window). The take handlers (CS_MONMONEYTAKE /
    // CS_MONITEMTAKE) drain it into the player's purse / bag.
    if (ctx.corpse_registry && (money > 0 || !drops.empty()))
        ctx.corpse_registry->Put(obj_id, MakeCorpse(money, drops));

    // EXP to the killer now (no level chart yet → it just accrues + echoes).
    if (attacker && ctx.char_state)
    {
        ctx.char_state->Update(attacker, [&](CharSnapshot& s)
        {
            s.dwEXP += exp;
        });
        if (const auto s = ctx.char_state->Get(attacker))
        {
            const auto exp_ack = EncodeExpAck(s->dwEXP, 0, 0, 0);
            co_await sess->SendPacket(
                static_cast<std::uint16_t>(MessageId::CS_EXP_ACK), exp_ack);
        }
    }

    spdlog::info("defend: char={} killed mon={} (tmpl={}) → +{} EXP; corpse "
        "= {} cooper + {} item(s)", attacker, obj_id, after->wTemplateID,
        exp, money, drops.size());

    // Quest kill-progress — advance any QTT_HUNT term the killer has for
    // this monster kind (legacy TMonster.cpp:683 CheckQuest(..., m_wKind,
    // QTT_HUNT, TT_KILLMON, 1)). Each advance echoes CS_QUESTUPDATE_ACK to
    // the killer; the turn-in (CS_QUESTEXEC_REQ → QT_COMPLETE) pays out.
    if (attacker && ctx.quest_log && ctx.quest_chart && ctx.quest_service)
    {
        EnsureQuestsLoaded(*ctx.quest_log, *ctx.quest_service, attacker);
        for (auto& prog : ctx.quest_log->ForChar(attacker))
        {
            const QuestDef* qdef = ctx.quest_chart->Find(prog.dwQuestID);
            if (!qdef) continue;
            const auto adv = quest_engine::AdvanceHunt(prog, *qdef, mon_kind);
            if (!adv.advanced) continue;
            co_await sess->SendPacket(
                static_cast<std::uint16_t>(MessageId::CS_QUESTUPDATE_ACK),
                EncodeQuestUpdateAck(prog.dwQuestID, adv.term_id,
                    QTT_HUNT, adv.count, adv.status));
            spdlog::info("quest: char={} hunt quest={} term={} {}/{}{}",
                attacker, prog.dwQuestID, adv.term_id, adv.count, adv.goal,
                adv.status == QTS_SUCCESS ? " (complete)" : "");
        }
    }

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

} // namespace

// CS_ACTION_REQ — broadcast the attack/skill animation to everyone in
// view. No damage (that's CS_DEFEND_REQ). Faithful to the legacy
// OnCS_ACTION_REQ, which decodes the action and Says CS_ACTION_ACK to the
// neighbors of the target object.
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

    // Broadcast the animation to everyone on the channel (spatial AOI
    // around the target position is the follow-up). `result` = success;
    // skill validation (MP/HP/cooldown) rides the skill wave.
    const auto ack = EncodeActionAck(kSkillOk, obj_id, obj_type, action_id,
                                     act_id, ani_id, skill_id);
    for (auto& w : WatchersOnChannel(ctx, channel))
        co_await w->SendPacket(
            static_cast<std::uint16_t>(MessageId::CS_ACTION_ACK), ack);
}

// CS_DEFEND_REQ — resolve a hit into real damage. The client supplies its
// attack powers; the server rolls against the target's defense and applies
// it. This wave ports the physical player→monster path.
boost::asio::awaitable<void>
OnDefendReq(std::shared_ptr<tnetlib::AsioSession> sess,
            std::vector<std::byte>                body,
            const HandlerContext&                 ctx)
{
    // Wire (legacy CSHandler.cpp:1485 read order). Decoded in full so the
    // packet is documented and the PvP / magic / skill-effect waves only
    // add behavior, not parsing.
    wire::Reader r(body.data(), body.size());
    std::uint32_t dwHostID = 0, dwAttackID = 0, dwTargetID = 0;
    std::uint8_t  bAttackType = 0, bTargetType = 0;
    std::uint16_t wAttackPartyID = 0;
    std::uint32_t dwActID = 0, dwAniID = 0;
    std::uint8_t  bChannel = 0;
    std::uint16_t wMapID = 0;
    std::uint8_t  bAttackerLevel = 0;
    std::uint32_t dwPysMinPower = 0, dwPysMaxPower = 0;
    std::uint32_t dwMgMinPower = 0, dwMgMaxPower = 0;
    std::uint16_t wTransHP = 0, wTransMP = 0;
    std::uint8_t  bCurseProb = 0, bEquipSpecial = 0, bCanSelect = 0;
    std::uint8_t  bAttackCountry = 0, bAttackAidCountry = 0;
    std::uint16_t wAttackLevel = 0;
    std::uint8_t  bCP = 0;
    std::uint16_t wSkillID = 0;
    std::uint8_t  bSkillLevel = 0;
    float fAtkPosX = 0, fAtkPosY = 0, fAtkPosZ = 0;
    float fDefPosX = 0, fDefPosY = 0, fDefPosZ = 0;
    std::uint32_t dwRemainTick = 0;
    if (!r.Read(dwHostID) || !r.Read(dwAttackID) || !r.Read(dwTargetID) ||
        !r.Read(bAttackType) || !r.Read(bTargetType) || !r.Read(wAttackPartyID) ||
        !r.Read(dwActID) || !r.Read(dwAniID) || !r.Read(bChannel) ||
        !r.Read(wMapID) || !r.Read(bAttackerLevel) || !r.Read(dwPysMinPower) ||
        !r.Read(dwPysMaxPower) || !r.Read(dwMgMinPower) || !r.Read(dwMgMaxPower) ||
        !r.Read(wTransHP) || !r.Read(wTransMP) || !r.Read(bCurseProb) ||
        !r.Read(bEquipSpecial) || !r.Read(bCanSelect) || !r.Read(bAttackCountry) ||
        !r.Read(bAttackAidCountry) || !r.Read(wAttackLevel) || !r.Read(bCP) ||
        !r.Read(wSkillID) || !r.Read(bSkillLevel) || !r.Read(fAtkPosX) ||
        !r.Read(fAtkPosY) || !r.Read(fAtkPosZ) || !r.Read(fDefPosX) ||
        !r.Read(fDefPosY) || !r.Read(fDefPosZ) || !r.Read(dwRemainTick))
    {
        spdlog::warn("CS_DEFEND_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    // This wave resolves physical hits on monsters. PvP (OT_PC defender),
    // recall-mon / companion targets, and magic damage route through the
    // not-yet-ported PvP + skill subsystems.
    if (bTargetType != OtMon || !ctx.monster_registry)
        co_return;

    const auto target = ctx.monster_registry->Find(dwTargetID);
    if (!target)
        co_return;   // already dead / never existed

    // Real physical damage: roll the client's power range against the
    // monster's TMONATTRCHART defense (faithful to CalcDamage:392-398).
    // Magic (dwMg*), crit (bCP / wAttackLevel → GetAtkHitType), transHP/MP,
    // and curse are skill-wave follow-ups.
    const std::uint32_t dmg = RollPhysicalDamage(
        dwPysMinPower, dwPysMaxPower, target->wDP, RandBelow);

    // Attacker char id (for EXP) from the sending session.
    std::uint32_t attacker = 0;
    if (ctx.session_reg)
        if (const auto cid = ctx.session_reg->FindCharIdBySession(sess.get()))
            attacker = *cid;

    co_await HitMonster(sess, dwTargetID, dmg, attacker, ctx);
}

// CS_REVIVAL_REQ — a dead player chooses to revive. Faithful to legacy
// OnCS_REVIVAL_REQ (CSHandler.cpp:1067): clear the death state, restore
// HP, move to the chosen position, and tell everyone in view the corpse
// stood back up.
boost::asio::awaitable<void>
OnRevivalReq(std::shared_ptr<tnetlib::AsioSession> sess,
             std::vector<std::byte>                body,
             const HandlerContext&                 ctx)
{
    using tnetlib::protocol::MessageId;

    // Wire (legacy CSHandler.cpp:1076): FLOAT x, y, z, BYTE type
    // (REVIVAL_NPC → revive in place vs ghost-walk; the aftermath variant
    // rides the stat/aftermath wave).
    wire::Reader r(body.data(), body.size());
    float x = 0, y = 0, z = 0;
    std::uint8_t type = 0;
    if (!r.Read(x) || !r.Read(y) || !r.Read(z) || !r.Read(type))
    {
        spdlog::warn("CS_REVIVAL_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    if (!ctx.session_reg || !ctx.char_state)
        co_return;
    const auto cid = ctx.session_reg->FindCharIdBySession(sess.get());
    if (!cid)
        co_return;

    // Only a dead char revives; restore HP and clear the death state
    // (legacy CTPlayer::Revival). MP / max-MP restore rides the stat wave.
    bool revived = false;
    std::uint32_t max_hp = 0;
    ctx.char_state->Update(*cid, [&](CharSnapshot& s)
    {
        if (!s.bDead)
            return;
        s.bDead = 0;
        s.dwHP  = s.dwMaxHP;
        s.fPosX = x;
        s.fPosY = y;
        s.fPosZ = z;
        max_hp  = s.dwMaxHP;
        revived = true;
    });
    if (!revived)
        co_return;

    const auto rev = EncodeRevivalAck(*cid, x, y, z);
    const auto hp  = EncodeHpMpAck(*cid, max_hp, max_hp, 0, 0);

    // The reviving player always gets the ack + refilled bar directly.
    co_await sess->SendPacket(
        static_cast<std::uint16_t>(MessageId::CS_REVIVAL_ACK), rev);
    co_await sess->SendPacket(
        static_cast<std::uint16_t>(MessageId::CS_HPMP_ACK), hp);

    // Reposition in presence (so the AI sees the new spot) and broadcast to
    // everyone else in view.
    if (const auto e =
            ctx.presence ? ctx.presence->FindEntry(*cid) : std::nullopt)
    {
        ctx.presence->UpdatePosition(*cid, e->map_id, Position{ x, y, z });
        for (auto& w : WatchersOnChannel(ctx, e->channel))
        {
            if (w.get() == sess.get()) continue;   // already sent above
            co_await w->SendPacket(
                static_cast<std::uint16_t>(MessageId::CS_REVIVAL_ACK), rev);
            co_await w->SendPacket(
                static_cast<std::uint16_t>(MessageId::CS_HPMP_ACK), hp);
        }
    }

    spdlog::info("revival: char={} revived at ({:.1f},{:.1f},{:.1f})",
        *cid, x, y, z);
}

} // namespace tmapsvr
