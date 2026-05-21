// Quest handlers — CS_QUESTEXEC_REQ and CS_QUESTDROP_REQ decode
// stubs.
//
// F12 reads the legacy wire bodies and logs the intent. The actual
// quest engine — template lookup, term-state advance, reward award,
// MW_/DM_ broadcasts — needs the QuestEngine layer that lives over
// the quest-template / item / skill charts; lands with the quest
// consolidation pass.
//
// Legacy parity: CSHandler.cpp:3536 (OnCS_QUESTEXEC_REQ),
// CSHandler.cpp:3590 (OnCS_QUESTDROP_REQ).

#include "handlers.h"

#include "services/session_registry.h"
#include "wire_codec.h"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tmapsvr {

boost::asio::awaitable<void>
OnQuestExecReq(std::shared_ptr<tnetlib::AsioSession> sess,
               std::vector<std::byte>                body,
               const HandlerContext&                 ctx)
{
    // CS_QUESTEXEC_REQ body (legacy CSHandler.cpp:3536):
    //   DWORD dwQuestID
    //   BYTE  bRewardType
    //   DWORD dwRewardID
    wire::Reader r(body.data(), body.size());
    std::uint32_t dwQuestID = 0, dwRewardID = 0;
    std::uint8_t  bRewardType = 0;
    if (!r.Read(dwQuestID) || !r.Read(bRewardType) || !r.Read(dwRewardID))
    {
        spdlog::warn("CS_QUESTEXEC_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    std::uint32_t cid = 0;
    if (ctx.session_reg)
        if (const auto found = ctx.session_reg->FindCharIdBySession(sess.get()))
            cid = *found;

    spdlog::info("CS_QUESTEXEC_REQ char={} quest={} rewardType={} reward={} "
                 "— F12 stub (quest engine evaluation lands after "
                 "scaffolding sweep)",
        cid, dwQuestID, bRewardType, dwRewardID);
    co_return;
}

boost::asio::awaitable<void>
OnQuestDropReq(std::shared_ptr<tnetlib::AsioSession> sess,
               std::vector<std::byte>                body,
               const HandlerContext&                 ctx)
{
    // CS_QUESTDROP_REQ body (legacy CSHandler.cpp:3590):
    //   DWORD dwQuestID
    // The real DropQuest path (rewrite term rows + emit
    // CS_QUESTCOMPLETE_ACK(QR_DROP) + audit log) lands with the
    // engine.
    wire::Reader r(body.data(), body.size());
    std::uint32_t dwQuestID = 0;
    if (!r.Read(dwQuestID))
    {
        spdlog::warn("CS_QUESTDROP_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    std::uint32_t cid = 0;
    if (ctx.session_reg)
        if (const auto found = ctx.session_reg->FindCharIdBySession(sess.get()))
            cid = *found;

    spdlog::info("CS_QUESTDROP_REQ char={} quest={} — F12 stub",
        cid, dwQuestID);
    co_return;
}

} // namespace tmapsvr
