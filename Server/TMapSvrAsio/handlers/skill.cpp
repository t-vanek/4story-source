// Skill-use handler — CS_SKILLUSE_REQ decode + server-side gates + ack.
//
// F11 decodes the 31-byte header + the defender list, then enforces the
// server-authoritative skill gates faithful to the legacy
// OnCS_SKILLUSE_REQ (CSHandler.cpp:2429 → CTSkill::CanUse /
// GetRequiredMP / GetRequiredHP):
//   * resource cost  — MP/HP from TSKILLCHART (skill_engine.h)
//   * reuse cooldown — TSKILLCHART.dwReuseDelay (skill_cooldown.h)
// A rejection answers the caster with the short CS_SKILLUSE_ACK form
// (SKILL_NEEDMP / SKILL_NEEDHP / SKILL_SPEEDYUSE). On success the cost is
// deducted, the fat SKILL_SUCCESS ack (with the defender list) is
// broadcast to everyone in view, and the caster's new bars are echoed via
// CS_HPMP — the same packet pair the legacy success path Says
// (CSHandler.cpp:2992-3030). The actual damage/heal then arrives from the
// defenders' clients as CS_DEFEND_REQ (combat.cpp), which is where the
// TSKILLDATA effects (heal) are applied.
//
// Known placeholders (documented until their waves land):
//   * attacker combat stats in the success ack (powers / crit / attack
//     level) ship 0 — the player AP/WAP/DP wave models them;
//   * skill_level ships 1 — the per-char learned-rank layer is pending;
//   * multi-attack target expansion (TSKILLCHART.bTargetHit) is skipped —
//     the decoded targets relay 1:1.
//
// Legacy parity: CSHandler.cpp:2429 (OnCS_SKILLUSE_REQ).

#include "handlers.h"

#include "domain/character.h"
#include "domain/skill_data.h"
#include "services/channel_presence.h"
#include "services/char_state_store.h"
#include "services/client_senders.h"
#include "services/session_registry.h"
#include "services/skill_chart.h"
#include "services/skill_cooldown.h"
#include "services/skill_engine.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace tmapsvr {

namespace {

constexpr std::uint8_t kOtPc      = 1;    // OBJ_TYPE::OT_PC (NetCode.h:1030)
constexpr std::size_t  kMaxTarget = 16;   // MAX_TARGET (TMapType.h)

} // namespace

boost::asio::awaitable<void>
OnSkillUseReq(std::shared_ptr<tnetlib::AsioSession> sess,
              std::vector<std::byte>                body,
              const HandlerContext&                 ctx)
{
    using tnetlib::protocol::MessageId;

    // CS_SKILLUSE_REQ header (legacy CSHandler.cpp:2429) — 31 bytes, then
    // BYTE count × { DWORD target, BYTE target_type, BYTE is_target }.
    wire::Reader r(body.data(), body.size());

    std::uint32_t dwAttackID  = 0;
    std::uint8_t  bAttackType = 0;
    std::uint8_t  bChannel    = 0;
    std::uint16_t wMapID      = 0;
    std::uint16_t wSkillID    = 0;
    std::uint8_t  bActionID   = 0;
    std::uint32_t dwActID     = 0;
    std::uint32_t dwAniID     = 0;
    float fPosX = 0.f, fPosY = 0.f, fPosZ = 0.f;
    std::uint8_t  bCount      = 0;

    if (!r.Read(dwAttackID)  || !r.Read(bAttackType) ||
        !r.Read(bChannel)    || !r.Read(wMapID)      ||
        !r.Read(wSkillID)    || !r.Read(bActionID)   ||
        !r.Read(dwActID)     || !r.Read(dwAniID)     ||
        !r.Read(fPosX) || !r.Read(fPosY) || !r.Read(fPosZ) ||
        !r.Read(bCount))
    {
        spdlog::warn("CS_SKILLUSE_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    // Defender list — only flagged entries become broadcast targets
    // (legacy vDEFEND, capped at MAX_TARGET; CSHandler.cpp:2666-2701).
    // The multi-attack expansion (bTargetHit duplicates) is skipped.
    std::vector<SkillTarget> targets;
    for (std::uint8_t i = 0; i < bCount; ++i)
    {
        std::uint32_t dwTarget    = 0;
        std::uint8_t  bTargetType = 0, bIsTarget = 0;
        if (!r.Read(dwTarget) || !r.Read(bTargetType) || !r.Read(bIsTarget))
            break;   // short list — keep what parsed
        if (bIsTarget && targets.size() < kMaxTarget)
            targets.push_back({ dwTarget, bTargetType });
    }

    std::uint32_t cid = 0;
    if (ctx.session_reg)
    {
        if (const auto found = ctx.session_reg->FindCharIdBySession(sess.get()))
            cid = *found;
    }

    // The short reject form echoes the request identity with everything
    // else zeroed (legacy 7-arg SendCS_SKILLUSE_ACK calls).
    SkillUseAckFields ack;
    ack.attack_id   = dwAttackID;
    ack.attack_type = bAttackType;
    ack.skill_id    = wSkillID;
    ack.action_id   = bActionID;
    ack.act_id      = dwActID;
    ack.ani_id      = dwAniID;

    // Server-side skill gates (legacy OnCS_SKILLUSE_REQ → GetRequiredMP/HP
    // at :2726/:2771, then CanUse + SkillUse at :2804). Order matters:
    // check resources (read-only) BEFORE arming the reuse cooldown so an
    // unaffordable cast doesn't burn the cooldown. Both run only when we
    // know the char and the skill has a chart row; an unknown skill passes
    // through (no rank/learn validation yet).
    std::uint32_t req_mp = 0, req_hp = 0;
    if (cid && ctx.skill_chart)
    {
        if (const auto tmpl = ctx.skill_chart->Find(wSkillID))
        {
            // Resource cost (Wave 4b) — type-2 (%-of-max) exact, type-1
            // deferred to 0 (see services/skill_engine.h). 0 when char
            // state isn't loaded (no-DB / test path).
            if (ctx.char_state)
            {
                if (const auto s = ctx.char_state->Get(cid))
                {
                    req_mp = skill_engine::RequiredMP(*tmpl, s->dwMaxMP);
                    req_hp = skill_engine::RequiredHP(*tmpl, s->dwMaxHP);
                    if (s->dwMP < req_mp)
                    {
                        spdlog::info("CS_SKILLUSE_REQ char={} skill={} needs {} MP "
                                     "(has {}) — SKILL_NEEDMP", cid, wSkillID,
                            req_mp, s->dwMP);
                        ack.result = SKILL_NEEDMP;
                        co_await sess->SendPacket(
                            static_cast<std::uint16_t>(MessageId::CS_SKILLUSE_ACK),
                            EncodeSkillUseAck(ack, {}));
                        co_return;
                    }
                    // An HP-cost skill must leave the caster alive (legacy
                    // checks HP <= cost, CSHandler.cpp:2772).
                    if (req_hp > 0 && s->dwHP <= req_hp)
                    {
                        spdlog::info("CS_SKILLUSE_REQ char={} skill={} needs {} HP "
                                     "(has {}) — SKILL_NEEDHP", cid, wSkillID,
                            req_hp, s->dwHP);
                        ack.result = SKILL_NEEDHP;
                        co_await sess->SendPacket(
                            static_cast<std::uint16_t>(MessageId::CS_SKILLUSE_ACK),
                            EncodeSkillUseAck(ack, {}));
                        co_return;
                    }
                }
            }

            // Reuse-cooldown gate (legacy CTSkill::CanUse): reject a re-use
            // that arrives faster than the skill's TSKILLCHART reuse delay.
            // TryUse arms the cooldown, so it runs only after the resource
            // check.
            if (tmpl->dwReuseDelay > 0 && ctx.skill_cooldown)
            {
                const auto now_ms = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count());
                if (!ctx.skill_cooldown->TryUse(cid, wSkillID, now_ms,
                                                tmpl->dwReuseDelay))
                {
                    spdlog::info("CS_SKILLUSE_REQ char={} skill={} on cooldown "
                                 "(reuse={}ms) — SKILL_SPEEDYUSE",
                        cid, wSkillID, tmpl->dwReuseDelay);
                    ack.result = SKILL_SPEEDYUSE;
                    co_await sess->SendPacket(
                        static_cast<std::uint16_t>(MessageId::CS_SKILLUSE_ACK),
                        EncodeSkillUseAck(ack, {}));
                    co_return;
                }
            }
        }
    }

    // Gates passed — commit the cost to the live char state (clamped so a
    // bar change racing in can't underflow) and capture the bars + identity
    // fields the broadcasts need.
    std::uint32_t hp = 0, mp = 0, max_hp = 0, max_mp = 0;
    std::uint8_t  char_level = 1, char_country = 0;
    if (cid && ctx.char_state)
    {
        ctx.char_state->Update(cid, [&](CharSnapshot& cs)
        {
            cs.dwMP = cs.dwMP >= req_mp ? cs.dwMP - req_mp : 0;
            cs.dwHP = cs.dwHP >= req_hp ? cs.dwHP - req_hp : 1;
            hp           = cs.dwHP;
            mp           = cs.dwMP;
            max_hp       = cs.dwMaxHP;
            max_mp       = cs.dwMaxMP;
            char_level   = cs.bLevel;
            char_country = cs.bCountry;
        });
        if (req_mp > 0 || req_hp > 0)
            spdlog::info("CS_SKILLUSE_REQ char={} skill={} cost {} MP / {} HP "
                         "→ {}/{} HP {}/{} MP",
                cid, wSkillID, req_mp, req_hp, hp, max_hp, mp, max_mp);
    }

    // Success — broadcast the fat SKILL_SUCCESS ack (the cast + its
    // defender list) and, when a cost was charged, the caster's new bars
    // (legacy CSHandler.cpp:2992-3030 sends exactly this pair to every
    // near player, caster included). Attacker combat stats ship 0 until
    // the AP/WAP/DP wave; skill_level ships 1 until the rank layer.
    ack.result         = SKILL_SUCCESS;
    ack.skill_level    = 1;
    ack.attacker_level = char_level;
    ack.country        = char_country;
    ack.gnd_x          = fPosX;
    ack.gnd_y          = fPosY;
    ack.gnd_z          = fPosZ;
    const auto use_ack = EncodeSkillUseAck(ack, targets);
    const auto bars    = EncodeHpMpAck(cid, kOtPc, max_hp, hp, max_mp, mp);
    const bool charged = (req_mp > 0 || req_hp > 0) && max_hp > 0;

    std::vector<std::shared_ptr<tnetlib::AsioSession>> watchers;
    if (cid && ctx.presence)
    {
        // Channel from the authoritative presence entry, not the
        // client-sent header.
        if (const auto e = ctx.presence->FindEntry(cid))
            ctx.presence->ForEachInChannel(e->channel, /*skip=*/0,
                [&](const ChannelPresenceEntry&,
                    std::shared_ptr<tnetlib::AsioSession> s)
                { watchers.push_back(std::move(s)); });
    }
    if (watchers.empty())   // not in presence yet — at least tell the caster
        watchers.push_back(sess);

    for (auto& w : watchers)
    {
        co_await w->SendPacket(
            static_cast<std::uint16_t>(MessageId::CS_SKILLUSE_ACK), use_ack);
        if (charged)
            co_await w->SendPacket(
                static_cast<std::uint16_t>(MessageId::CS_HPMP_ACK), bars);
    }

    spdlog::info("CS_SKILLUSE_REQ char={} skill={} action={} target={} type={} "
                 "ch={} map={} pos=({:.1f},{:.1f},{:.1f}) targets={} — "
                 "SKILL_SUCCESS broadcast to {} watcher(s)",
        cid, wSkillID, bActionID, dwAttackID, bAttackType,
        bChannel, wMapID, fPosX, fPosY, fPosZ, targets.size(),
        watchers.size());

    co_return;
}

} // namespace tmapsvr
