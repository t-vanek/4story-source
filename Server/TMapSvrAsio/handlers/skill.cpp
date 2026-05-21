// Skill-use handler — CS_SKILLUSE_REQ decode stub.
//
// F11 decodes the 31-byte header, logs the cast intent, and drops
// the request. Damage calculation, MP consumption, defender list
// resolution, and the 25-field CS_SKILLUSE_ACK broadcast wait for
// the skill-template (TSKILLCHART) consolidation pass that wires
// range / MP / cooldown gates.
//
// Legacy parity: CSHandler.cpp:2429 (OnCS_SKILLUSE_REQ).

#include "handlers.h"

#include "services/session_registry.h"
#include "wire_codec.h"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tmapsvr {

boost::asio::awaitable<void>
OnSkillUseReq(std::shared_ptr<tnetlib::AsioSession> sess,
              std::vector<std::byte>                body,
              const HandlerContext&                 ctx)
{
    // CS_SKILLUSE_REQ header (legacy CSHandler.cpp:2429) — 31 bytes
    // before the variable defender list. F11 decodes the header,
    // logs the skill cast intent, and drops the request. Damage
    // calculation, MP consumption, defender targeting, and the
    // CS_SKILLUSE_ACK broadcast (25+ fields of damage info) are
    // gameplay-policy work that needs the skill template chart
    // (TSKILLCHART) and equipment / class state — those land in a
    // later phase.
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

    spdlog::info("CS_SKILLUSE_REQ char={} skill={} action={} target={} type={} "
                 "ch={} map={} pos=({:.1f},{:.1f},{:.1f}) — F11 stub (damage / "
                 "ack broadcast lands with skill-template phase)",
        cid, wSkillID, bActionID, dwAttackID, bAttackType,
        bChannel, wMapID, fPosX, fPosY, fPosZ);

    co_return;
}

} // namespace tmapsvr
