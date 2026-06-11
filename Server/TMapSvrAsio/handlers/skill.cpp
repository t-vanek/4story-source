// Skill-use handler — CS_SKILLUSE_REQ decode + server-side gates.
//
// F11 decodes the 31-byte header, then enforces the server-authoritative
// skill gates faithful to the legacy OnCS_SKILLUSE_REQ (CSHandler.cpp:2429
// → CTSkill::CanUse / GetRequiredMP / GetRequiredHP):
//   * resource cost  — MP/HP from TSKILLCHART (skill_engine.h)
//   * reuse cooldown — TSKILLCHART.dwReuseDelay (skill_cooldown.h)
// On success the cost is deducted from the live char state and the new
// bars are echoed to everyone in view via CS_HPMP. Damage calculation,
// defender-list resolution, skill-data effects (heal/buff), and the
// 25-field CS_SKILLUSE_ACK broadcast wait for the skill-data (TSKILLDATA)
// wave.
//
// Legacy parity: CSHandler.cpp:2429 (OnCS_SKILLUSE_REQ).

#include "handlers.h"

#include "domain/character.h"
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
#include <vector>

namespace tmapsvr {

boost::asio::awaitable<void>
OnSkillUseReq(std::shared_ptr<tnetlib::AsioSession> sess,
              std::vector<std::byte>                body,
              const HandlerContext&                 ctx)
{
    // CS_SKILLUSE_REQ header (legacy CSHandler.cpp:2429) — 31 bytes before
    // the variable defender list. F11 decodes the header, runs the gates,
    // and (for now) drops the cast after charging cost. Damage / defender
    // targeting / the CS_SKILLUSE_ACK broadcast (25+ fields of damage info)
    // need the skill-data layer and land in a later phase.
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

    if (!r.Read(dwAttackID)  || !r.Read(bAttackType) ||
        !r.Read(bChannel)    || !r.Read(wMapID)      ||
        !r.Read(wSkillID)    || !r.Read(bActionID)   ||
        !r.Read(dwActID)     || !r.Read(dwAniID)     ||
        !r.Read(fPosX) || !r.Read(fPosY) || !r.Read(fPosZ))
    {
        spdlog::warn("CS_SKILLUSE_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    std::uint32_t cid = 0;
    if (ctx.session_reg)
    {
        if (const auto found = ctx.session_reg->FindCharIdBySession(sess.get()))
            cid = *found;
    }

    // Server-side skill gates (legacy OnCS_SKILLUSE_REQ → CanUse +
    // GetRequiredMP/HP). Order matters: check resources (read-only) BEFORE
    // arming the reuse cooldown so an unaffordable cast doesn't burn the
    // cooldown. Both run only when we know the char and the skill has a chart
    // row; an unknown skill passes through. A rejection drops the cast
    // server-side (the legit client predicts cost + cooldown locally; this
    // backstops a hacked one) — the SKILL_NEEDMP/NEEDHP/SPEEDYUSE reject ack
    // is a follow-up, same staging as the damage broadcast. A successful
    // charge echoes the authoritative bars via CS_HPMP below.
    if (cid && ctx.skill_chart)
    {
        if (const auto tmpl = ctx.skill_chart->Find(wSkillID))
        {
            // Resource cost (Wave 4b) — faithful to CTSkill::GetRequiredMP/HP.
            // Type-2 (%-of-max) is exact; type-1 (flat × level-scale) is
            // deferred to 0 until the level-scale base + skill-rank layer land
            // (see services/skill_engine.h). 0 when char state isn't loaded
            // (no-DB / test path), so the gate is a no-op there.
            std::uint32_t req_mp = 0, req_hp = 0;
            if (ctx.char_state)
            {
                if (const auto s = ctx.char_state->Get(cid))
                {
                    req_mp = skill_engine::RequiredMP(*tmpl, s->dwMaxMP);
                    req_hp = skill_engine::RequiredHP(*tmpl, s->dwMaxHP);
                    if (s->dwMP < req_mp)
                    {
                        spdlog::info("CS_SKILLUSE_REQ char={} skill={} needs {} MP "
                                     "(has {}) — dropped", cid, wSkillID, req_mp,
                            s->dwMP);
                        co_return;
                    }
                    // An HP-cost skill must leave the caster alive (legacy
                    // requires HP strictly greater than the cost).
                    if (req_hp > 0 && s->dwHP <= req_hp)
                    {
                        spdlog::info("CS_SKILLUSE_REQ char={} skill={} needs {} HP "
                                     "(has {}) — dropped", cid, wSkillID, req_hp,
                            s->dwHP);
                        co_return;
                    }
                }
            }

            // Reuse-cooldown gate (legacy CTSkill::CanUse): drop a re-use that
            // arrives faster than the skill's TSKILLCHART reuse delay. TryUse
            // arms the cooldown, so it runs only after the resource check.
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
                                 "(reuse={}ms) — dropped",
                        cid, wSkillID, tmpl->dwReuseDelay);
                    co_return;
                }
            }

            // Both gates passed — commit the cost to the live char state (the
            // authoritative bars; the client predicts the same deduction).
            // The subtraction clamps so a bar change racing in between the
            // read-only gate and this commit can't underflow.
            if ((req_mp > 0 || req_hp > 0) && ctx.char_state)
            {
                std::uint32_t hp = 0, mp = 0, max_hp = 0, max_mp = 0;
                ctx.char_state->Update(cid, [&](CharSnapshot& cs)
                {
                    cs.dwMP = cs.dwMP >= req_mp ? cs.dwMP - req_mp : 0;
                    cs.dwHP = cs.dwHP >= req_hp ? cs.dwHP - req_hp : 1;
                    hp      = cs.dwHP;
                    mp      = cs.dwMP;
                    max_hp  = cs.dwMaxHP;
                    max_mp  = cs.dwMaxMP;
                });
                spdlog::info("CS_SKILLUSE_REQ char={} skill={} cost {} MP / {} HP "
                             "→ {}/{} HP {}/{} MP",
                    cid, wSkillID, req_mp, req_hp, hp, max_hp, mp, max_mp);

                // Echo the authoritative bars (legacy SendCS_HPMP_ACK after
                // the charge) to everyone in view — the caster sees the cost
                // land, watchers see the caster's new bars. Channel from the
                // authoritative presence entry, not the client-sent header.
                using tnetlib::protocol::MessageId;
                constexpr std::uint8_t kOtPc = 1;   // OBJ_TYPE::OT_PC
                const auto bars =
                    EncodeHpMpAck(cid, kOtPc, max_hp, hp, max_mp, mp);
                bool echoed = false;
                if (ctx.presence)
                {
                    if (const auto e = ctx.presence->FindEntry(cid))
                    {
                        std::vector<std::shared_ptr<tnetlib::AsioSession>> ws;
                        ctx.presence->ForEachInChannel(e->channel, /*skip=*/0,
                            [&](const ChannelPresenceEntry&,
                                std::shared_ptr<tnetlib::AsioSession> s)
                            { ws.push_back(std::move(s)); });
                        for (auto& w : ws)
                            co_await w->SendPacket(
                                static_cast<std::uint16_t>(
                                    MessageId::CS_HPMP_ACK), bars);
                        echoed = true;
                    }
                }
                if (!echoed)   // not in presence yet — at least tell the caster
                    co_await sess->SendPacket(
                        static_cast<std::uint16_t>(MessageId::CS_HPMP_ACK),
                        bars);
            }
        }
    }

    spdlog::info("CS_SKILLUSE_REQ char={} skill={} action={} target={} type={} "
                 "ch={} map={} pos=({:.1f},{:.1f},{:.1f}) — gates OK (cooldown + "
                 "MP/HP cost; damage / ack broadcast pend the skill-data layer)",
        cid, wSkillID, bActionID, dwAttackID, bAttackType,
        bChannel, wMapID, fPosX, fPosY, fPosZ);

    co_return;
}

} // namespace tmapsvr
