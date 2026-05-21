#include "handlers_world.h"

#include "services/companion_service.h"
#include "services/inventory_service.h"
#include "services/player_service.h"
#include "services/quest_service.h"
#include "services/skill_service.h"
#include "services/world_client.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <utility>
#include <vector>

namespace tmapsvr {

namespace {

// CN_-style result codes for DM_LOADCHAR_ACK. Internal-only constants
// — legacy header not yet recovered. Values match what other
// handlers in this tree use.
constexpr std::uint8_t CnSuccess  = 0;
constexpr std::uint8_t CnInternal = 3;
constexpr std::uint8_t CnNoChar   = 6;

// DM_LOADCHAR_ACK error body — 9 bytes (dwCharID + dwKEY + result).
// Mirrors the error path in legacy SSHandler.cpp:3379 / 3392 where
// the char row is missing or the DB query fails.
std::vector<std::byte> EncodeLoadCharAckError(std::uint32_t dwCharID,
                                              std::uint32_t dwKEY,
                                              std::uint8_t  result)
{
    std::vector<std::byte> body;
    body.reserve(9);
    wire::WritePOD<std::uint32_t>(body, dwCharID);
    wire::WritePOD<std::uint32_t>(body, dwKEY);
    wire::WritePOD<std::uint8_t> (body, result);
    return body;
}

// DM_LOADCHAR_ACK success body — TCHARTABLE-derived snapshot plus the
// sentinel trio (TRand(600000) / WORD(0) / BYTE(TRUE)) that legacy
// SSHandler.cpp:3445 appends right after the snapshot fields. The
// trailing sub-sections (secure code, aid table, PC bang, post info,
// inventory, cabinet, skills, quests, …) are documented as zero /
// empty here and will be filled in by their owning phases as those
// services come online (F9 items, F11 skills, F12 quests, …).
std::vector<std::byte> EncodeLoadCharAckSuccess(std::uint32_t dwCharID,
                                                std::uint32_t dwKEY,
                                                const CharSnapshot& s,
                                                const std::vector<InventoryRow>& inven,
                                                const std::vector<SkillRow>& skills,
                                                const std::vector<QuestProgressRow>& quests,
                                                const std::vector<CompanionRow>& companions)
{
    std::vector<std::byte> body;
    body.reserve(256);

    wire::WritePOD<std::uint32_t>(body, dwCharID);
    wire::WritePOD<std::uint32_t>(body, dwKEY);
    wire::WritePOD<std::uint8_t> (body, CnSuccess);

    wire::WriteString            (body, s.szNAME);
    wire::WritePOD<std::uint8_t> (body, s.bStartAct);
    wire::WritePOD<std::uint8_t> (body, s.bRealSex);
    wire::WritePOD<std::uint8_t> (body, s.bClass);
    wire::WritePOD<std::uint8_t> (body, s.bLevel);
    wire::WritePOD<std::uint8_t> (body, s.bRace);
    wire::WritePOD<std::uint8_t> (body, s.bCountry);
    wire::WritePOD<std::uint8_t> (body, s.bOriCountry);
    wire::WritePOD<std::uint8_t> (body, s.bSex);
    wire::WritePOD<std::uint8_t> (body, s.bHair);
    wire::WritePOD<std::uint8_t> (body, s.bFace);
    wire::WritePOD<std::uint8_t> (body, s.bBody);
    wire::WritePOD<std::uint8_t> (body, s.bPants);
    wire::WritePOD<std::uint8_t> (body, s.bHand);
    wire::WritePOD<std::uint8_t> (body, s.bFoot);
    wire::WritePOD<std::uint8_t> (body, s.bHelmetHide);
    wire::WritePOD<std::uint32_t>(body, s.dwGold);
    wire::WritePOD<std::uint32_t>(body, s.dwSilver);
    wire::WritePOD<std::uint32_t>(body, s.dwCooper);
    wire::WritePOD<std::uint32_t>(body, s.dwEXP);
    wire::WritePOD<std::uint32_t>(body, s.dwHP);
    wire::WritePOD<std::uint32_t>(body, s.dwMP);
    wire::WritePOD<std::uint16_t>(body, s.wSkillPoint);
    wire::WritePOD<std::uint32_t>(body, s.dwRegion);
    wire::WritePOD<std::uint8_t> (body, s.bGuildLeave);
    wire::WritePOD<std::uint32_t>(body, s.dwGuildLeaveTime);
    wire::WritePOD<std::uint16_t>(body, s.wMapID);
    wire::WritePOD<std::uint16_t>(body, s.wSpawnID);
    wire::WritePOD<std::uint16_t>(body, s.wLastSpawnID);
    wire::WritePOD<std::uint32_t>(body, s.dwLastDestination);
    wire::WritePOD<std::uint16_t>(body, s.wTemptedMon);
    wire::WritePOD<std::uint8_t> (body, s.bAftermath);
    wire::WritePOD<float>        (body, s.fPosX);
    wire::WritePOD<float>        (body, s.fPosY);
    wire::WritePOD<float>        (body, s.fPosZ);
    wire::WritePOD<std::uint16_t>(body, s.wDIR);
    wire::WritePOD<std::uint8_t> (body, s.bStatLevel);
    wire::WritePOD<std::uint8_t> (body, s.bStatPoint);
    wire::WritePOD<std::uint32_t>(body, s.dwStatExp);

    // Sentinel trio from legacy SSHandler.cpp:3445. TRand(600000) is
    // a per-load anti-replay value; we ship a fixed sentinel until
    // the gameplay logic that actually consumes it lands (BR/Bow
    // round timing in F16). WORD(0) and BYTE(TRUE) are constants.
    wire::WritePOD<std::uint32_t>(body, 0u);    // TRand placeholder
    wire::WritePOD<std::uint16_t>(body, 0);     // WORD(0)
    wire::WritePOD<std::uint8_t> (body, 1);     // BYTE(TRUE)

    // F9 inventory section (legacy SSHandler.cpp:3540): WORD count
    // followed by one record per row. Each record carries:
    //   BYTE  bInvenID    slot id
    //   WORD  wItemID     item template id
    //   INT64 dEndTime    expiry tick (0 = permanent)
    //   BYTE  bELD        legacy ELD flag
    wire::WritePOD<std::uint16_t>(body, static_cast<std::uint16_t>(inven.size()));
    for (const auto& r : inven)
    {
        wire::WritePOD<std::uint8_t> (body, r.bInvenID);
        wire::WritePOD<std::uint16_t>(body, r.wItemID);
        wire::WritePOD<std::int64_t> (body, r.dEndTime);
        wire::WritePOD<std::uint8_t> (body, r.bELD);
    }

    // F11 skill section: WORD count + one record per learned skill.
    //   WORD  wSkillID
    //   BYTE  bLevel
    //   DWORD dwRemainTick    cooldown remaining (0 = ready)
    wire::WritePOD<std::uint16_t>(body, static_cast<std::uint16_t>(skills.size()));
    for (const auto& r : skills)
    {
        wire::WritePOD<std::uint16_t>(body, r.wSkillID);
        wire::WritePOD<std::uint8_t> (body, r.bLevel);
        wire::WritePOD<std::uint32_t>(body, r.dwRemainTick);
    }

    // F12 quest section: WORD count + one record per accepted quest.
    // Each record carries the TQUESTTABLE fields followed by a
    // nested WORD count + N TQUESTTERMTABLE term-progress rows. The
    // legacy encoder streams the same pair of counts inline (see
    // SSHandler.cpp around line 3700).
    wire::WritePOD<std::uint16_t>(body, static_cast<std::uint16_t>(quests.size()));
    for (const auto& q : quests)
    {
        wire::WritePOD<std::uint32_t>(body, q.dwQuestID);
        wire::WritePOD<std::uint32_t>(body, q.dwTick);
        wire::WritePOD<std::uint8_t> (body, q.bCompleteCount);
        wire::WritePOD<std::uint8_t> (body, q.bTriggerCount);
        wire::WritePOD<std::uint16_t>(body, static_cast<std::uint16_t>(q.terms.size()));
        for (const auto& t : q.terms)
        {
            wire::WritePOD<std::uint32_t>(body, t.dwTermID);
            wire::WritePOD<std::uint8_t> (body, t.bTermType);
            wire::WritePOD<std::uint8_t> (body, t.bCount);
        }
    }

    // F15 companion section: WORD count + one record per row.
    //   BYTE  bSlot
    //   DWORD dwMonID
    //   BYTE  bLevel
    //   string strName
    //   DWORD dwExp
    //   WORD  wLife
    //   BYTE  bStatusPoints
    //   BYTE  bEffect
    //   WORD  wSTR / wDEX / wCON / wINT / wWIS / wMEN
    //   WORD  wBonusID
    wire::WritePOD<std::uint16_t>(body, static_cast<std::uint16_t>(companions.size()));
    for (const auto& c : companions)
    {
        wire::WritePOD<std::uint8_t> (body, c.bSlot);
        wire::WritePOD<std::uint32_t>(body, c.dwMonID);
        wire::WritePOD<std::uint8_t> (body, c.bLevel);
        wire::WriteString            (body, c.strName);
        wire::WritePOD<std::uint32_t>(body, c.dwExp);
        wire::WritePOD<std::uint16_t>(body, c.wLife);
        wire::WritePOD<std::uint8_t> (body, c.bStatusPoints);
        wire::WritePOD<std::uint8_t> (body, c.bEffect);
        wire::WritePOD<std::uint16_t>(body, c.wSTR);
        wire::WritePOD<std::uint16_t>(body, c.wDEX);
        wire::WritePOD<std::uint16_t>(body, c.wCON);
        wire::WritePOD<std::uint16_t>(body, c.wINT);
        wire::WritePOD<std::uint16_t>(body, c.wWIS);
        wire::WritePOD<std::uint16_t>(body, c.wMEN);
        wire::WritePOD<std::uint16_t>(body, c.wBonusID);
    }

    // The legacy ack continues with cabinet / equip / friend / craft
    // / mail / chapter / recall-mon / pet sections. Each remaining
    // section lands with its owning phase, in wire order, on top of
    // this body.

    return body;
}

} // namespace

boost::asio::awaitable<void>
OnDMLoadCharReq(std::vector<std::byte> body, const HandlerContext& ctx)
{
    using tnetlib::protocol::MessageId;

    // Wire layout from legacy SSHandler.cpp:3311 (12 bytes):
    //   DWORD dwCharID
    //   DWORD dwKEY
    //   DWORD dwUserID
    wire::Reader r(body.data(), body.size());
    std::uint32_t dwCharID = 0, dwKEY = 0, dwUserID = 0;
    if (!r.Read(dwCharID) || !r.Read(dwKEY) || !r.Read(dwUserID))
    {
        spdlog::warn("DM_LOADCHAR_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    if (!ctx.world_client || !ctx.world_client->IsConnected())
    {
        spdlog::warn("DM_LOADCHAR_REQ: world peer not connected — ack "
                     "dropped (char={})", dwCharID);
        co_return;
    }

    // F8 success path: player service configured + char row exists.
    // Falls through to the error variants when either condition
    // isn't met, matching legacy CN_INTERNAL / CN_NOCHAR semantics.
    if (!ctx.player_service)
    {
        spdlog::warn("DM_LOADCHAR_REQ char={}: no player service "
                     "configured — ack INTERNAL", dwCharID);
        co_await ctx.world_client->SendPacket(
            static_cast<std::uint16_t>(MessageId::DM_LOADCHAR_ACK),
            EncodeLoadCharAckError(dwCharID, dwKEY, CnInternal));
        co_return;
    }

    const auto snap = ctx.player_service->LoadChar(dwCharID);
    if (!snap)
    {
        spdlog::info("DM_LOADCHAR_REQ char={} user={}: no row — ack NOCHAR",
            dwCharID, dwUserID);
        co_await ctx.world_client->SendPacket(
            static_cast<std::uint16_t>(MessageId::DM_LOADCHAR_ACK),
            EncodeLoadCharAckError(dwCharID, dwKEY, CnNoChar));
        co_return;
    }

    // Optional sections — without their services we ship empty
    // sub-bodies (count = 0) so the wire shape stays well-defined
    // through F11. Legacy treats "0 inventory rows" as the load-
    // error path; we keep that contract documented per service.
    std::vector<InventoryRow> inven;
    if (ctx.inventory_service)
        inven = ctx.inventory_service->LoadInventory(dwCharID);

    std::vector<SkillRow> skills;
    if (ctx.skill_service)
        skills = ctx.skill_service->LoadSkills(dwCharID);

    std::vector<QuestProgressRow> quests;
    if (ctx.quest_service)
        quests = ctx.quest_service->LoadProgress(dwCharID);

    std::vector<CompanionRow> companions;
    if (ctx.companion_service)
        companions = ctx.companion_service->LoadCompanions(dwCharID);

    spdlog::info("DM_LOADCHAR_REQ char={} user={} name='{}' lvl={} class={} "
                 "map={} pos=({:.1f},{:.1f},{:.1f}) inven={} skills={} "
                 "quests={} companions={} — F15 snapshot encoded",
        dwCharID, dwUserID, snap->szNAME, snap->bLevel, snap->bClass,
        snap->wMapID, snap->fPosX, snap->fPosY, snap->fPosZ,
        inven.size(), skills.size(), quests.size(), companions.size());

    co_await ctx.world_client->SendPacket(
        static_cast<std::uint16_t>(MessageId::DM_LOADCHAR_ACK),
        EncodeLoadCharAckSuccess(dwCharID, dwKEY, *snap, inven, skills,
                                 quests, companions));
}

boost::asio::awaitable<void>
DispatchWorld(std::uint16_t          wId,
              std::vector<std::byte> body,
              const HandlerContext&  ctx)
{
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToMessageId;

    const auto id = ToMessageId(wId);
    switch (id)
    {
    case MessageId::DM_LOADCHAR_REQ:
        co_await OnDMLoadCharReq(std::move(body), ctx);
        break;

    default:
        spdlog::debug("world_client: unhandled wId=0x{:04X} body={} bytes",
            wId, body.size());
        break;
    }
}

} // namespace tmapsvr
