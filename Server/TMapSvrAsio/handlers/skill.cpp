// Skill-use handler — CS_SKILLUSE_REQ decode + server-side gates + ack.
//
// F11 decodes the 31-byte header + the defender list, then enforces the
// server-authoritative skill gates faithful to the legacy
// OnCS_SKILLUSE_REQ (CSHandler.cpp:2429 → CTSkill::CanUse /
// GetRequiredMP / GetRequiredHP):
//   * learned ownership and rank — native CharacterPayload.skills
//   * resource cost — pinned TSKILLCHART and FTYPE_1ST (skill_engine.h)
//   * reuse cooldown — restored timers and optional gameplay chart delays
// A rejection answers the caster with the short CS_SKILLUSE_ACK form
// (SKILL_NOTFOUND / SKILL_NEEDMP / SKILL_NEEDHP / SKILL_SPEEDYUSE). On success the cost is
// deducted, the fat SKILL_SUCCESS ack (with the defender list) is
// broadcast to everyone in view, and the caster's new bars are echoed via
// CS_HPMP — the same packet pair the legacy success path Says
// (CSHandler.cpp:2992-3030). The actual damage/heal then arrives from the
// defenders' clients as CS_DEFEND_REQ (combat.cpp), which is where the
// TSKILLDATA effects (heal) are applied.
//
// Known placeholders (documented until their waves land):
//   * native new-use rank/attack-speed/shared-kind cooldown generation;
//   * attacker combat stats in the success ack (powers / crit / attack
//     level) ship 0 — the player AP/WAP/DP wave models them;
//   * native learned rank is loaded; the older no-payload path assumes rank 1;
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

#include <algorithm>
#include <cmath>
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
            co_return; // partial targets must not charge a cast
        if (bIsTarget && targets.size() < kMaxTarget)
            targets.push_back({ dwTarget, bTargetType });
    }

    if(!r.Eof()||!std::isfinite(fPosX)||!std::isfinite(fPosY)||!std::isfinite(fPosZ))co_return;

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

    // Resolve and charge under the character-state lock. Combat/AI updates
    // must not invalidate a read-only affordability check before deduction.
    // The lock order (character, then cooldown) matches transfer capture.
    std::uint32_t req_mp=0,req_hp=0,hp=0,mp=0,max_hp=0,max_mp=0;
    std::uint8_t char_level=1,char_country=0,rank=1;
    bool visited=false,ignored=false;
    if(!cid||!ctx.char_state)co_return;
    const auto identity=ctx.session_reg->Identity(sess.get());
    ctx.char_state->Update(cid,[&](CharSnapshot& cs) {
        visited=true;
        std::optional<SkillTemplate> definition;
        std::uint32_t reuse_delay=0;
        if(ctx.skill_chart) {
            definition=ctx.skill_chart->Find(wSkillID);
            if(definition)reuse_delay=definition->dwReuseDelay;
        }
        if(cs.payload) {
            // This native path owns this PC only. Summon/monster casting and
            // cross-peer gameplay require their separately ported authority.
            if(!identity||dwAttackID!=cid||bAttackType!=kOtPc||bChannel!=identity->channel||wMapID!=cs.wMapID) {
                ignored=true;return;
            }
            const auto& learned=cs.payload->skills;
            const auto row=std::find_if(learned.begin(),learned.end(),[&](const auto& s){return s.wSkillID==wSkillID;});
            if(row==learned.end()){ack.result=SKILL_NOTFOUND;return;}
            rank=row->bLevel;
            const auto& templates=cs.payload->skill_templates;
            const auto t=std::find_if(templates.begin(),templates.end(),[&](const auto& s){return s.wID==wSkillID;});
            if(t==templates.end())throw std::runtime_error("Native learned skill lacks pinned template");
            definition=*t;
            // This increment ports learned-rank resource gates. New-use attack
            // speed/rank/kind reuse modifiers remain a separate gameplay port;
            // existing restored timers and optional chart delays stay in force.
        }
        if(definition) {
            req_mp=skill_engine::RequiredMP(*definition,cs.dwMaxMP,rank);
            req_hp=skill_engine::RequiredHP(*definition,cs.dwMaxHP,rank);
        }
        if(cs.dwMP<req_mp){ack.result=SKILL_NEEDMP;return;}
        // Source refuses HP <= cost even for cost zero (dead caster).
        if(cs.dwHP<=req_hp){ack.result=SKILL_NEEDHP;return;}
        if(ctx.skill_cooldown&&!ctx.skill_cooldown->TryUse(cid,wSkillID,SkillClockMs(),reuse_delay)) {
            ack.result=SKILL_SPEEDYUSE;return;
        }
        cs.dwMP-=req_mp;cs.dwHP-=req_hp;
        hp=cs.dwHP;mp=cs.dwMP;max_hp=cs.dwMaxHP;max_mp=cs.dwMaxMP;
        char_level=cs.bLevel;char_country=cs.bCountry;ack.result=SKILL_SUCCESS;
    });
    if(!visited||ignored)co_return;
    if(ack.result!=SKILL_SUCCESS) {
        co_await sess->SendPacket(static_cast<std::uint16_t>(MessageId::CS_SKILLUSE_ACK),EncodeSkillUseAck(ack,{}));
        co_return;
    }

    // Success — broadcast the fat SKILL_SUCCESS ack (the cast + its
    // defender list) and, when a cost was charged, the caster's new bars
    // (legacy CSHandler.cpp:2992-3030 sends exactly this pair to every
    // near player, caster included). Attacker combat stats ship 0 until
    // the AP/WAP/DP wave. Native skill_level uses the stored learned rank.
    ack.result         = SKILL_SUCCESS;
    ack.skill_level    = rank;
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
