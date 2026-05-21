// Social handlers — CS_CHAT_REQ and the CS_PARTY{ADD,JOIN,DEL}_REQ
// trio. All decode-stubs: real routing (broadcast to channel, world
// relay, anti-spoof, ban-timer) lands with the chat / party
// consolidation pass.
//
// Legacy parity:
//   CSHandler.cpp:5206 (OnCS_CHAT_REQ)
//   CSHandler.cpp:3419 (OnCS_PARTYADD_REQ)
//   CSHandler.cpp:3451 (OnCS_PARTYJOIN_REQ)
//   CSHandler.cpp:3483 (OnCS_PARTYDEL_REQ)

#include "common.h"
#include "handlers.h"

#include "wire_codec.h"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tmapsvr {

boost::asio::awaitable<void>
OnChatReq(std::shared_ptr<tnetlib::AsioSession> sess,
          std::vector<std::byte>                body,
          const HandlerContext&                 ctx)
{
    // CS_CHAT_REQ body (legacy CSHandler.cpp:5206):
    //   string  strSender
    //   BYTE    bGroup
    //   DWORD   dwTarget
    //   string  strName
    //   string  strTalk
    // F14 decodes + logs. Real routing (CHAT_NORMAL → broadcast to
    // channel via presence, CHAT_PARTY → world relay, CHAT_WHISPER
    // → direct DM, anti-spoof check sender == player name, ban
    // timer enforcement) lands with the chat consolidation pass.
    wire::Reader r(body.data(), body.size());
    std::string sender, target_name, talk;
    std::uint8_t  bGroup  = 0;
    std::uint32_t dwTarget = 0;
    if (!r.ReadString(sender) || !r.Read(bGroup) || !r.Read(dwTarget) ||
        !r.ReadString(target_name) || !r.ReadString(talk))
    {
        spdlog::warn("CS_CHAT_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }
    spdlog::info("CS_CHAT_REQ char={} group={} target={}/{} talk='{}' — "
                 "F14 stub (broadcast / world relay lands with chat "
                 "consolidation)",
        handlers_detail::SenderCharId(sess, ctx), bGroup, dwTarget,
        target_name, talk);
    co_return;
}

boost::asio::awaitable<void>
OnPartyAddReq(std::shared_ptr<tnetlib::AsioSession> sess,
              std::vector<std::byte>                body,
              const HandlerContext&                 ctx)
{
    // CS_PARTYADD_REQ body (legacy CSHandler.cpp:3419):
    //   string strTarget
    //   BYTE   bObtainType
    // Routes through MW_PARTYADD_ACK on the world peer (legacy
    // SSSender.cpp); the world owns party state. F14 decodes + logs;
    // the MW_ encoder + world relay land with the party
    // consolidation pass.
    wire::Reader r(body.data(), body.size());
    std::string target;
    std::uint8_t bObtainType = 0;
    if (!r.ReadString(target) || !r.Read(bObtainType))
    {
        spdlog::warn("CS_PARTYADD_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }
    spdlog::info("CS_PARTYADD_REQ char={} target='{}' obtainType={} — F14 stub",
        handlers_detail::SenderCharId(sess, ctx), target, bObtainType);
    co_return;
}

boost::asio::awaitable<void>
OnPartyJoinReq(std::shared_ptr<tnetlib::AsioSession> sess,
               std::vector<std::byte>                body,
               const HandlerContext&                 ctx)
{
    // CS_PARTYJOIN_REQ body (legacy CSHandler.cpp:3451):
    //   string strOrigin
    //   BYTE   bObtainType
    //   BYTE   bResponse
    wire::Reader r(body.data(), body.size());
    std::string origin;
    std::uint8_t bObtainType = 0, bResponse = 0;
    if (!r.ReadString(origin) || !r.Read(bObtainType) || !r.Read(bResponse))
    {
        spdlog::warn("CS_PARTYJOIN_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }
    spdlog::info("CS_PARTYJOIN_REQ char={} origin='{}' obtainType={} "
                 "response={} — F14 stub",
        handlers_detail::SenderCharId(sess, ctx), origin, bObtainType,
        bResponse);
    co_return;
}

boost::asio::awaitable<void>
OnPartyDelReq(std::shared_ptr<tnetlib::AsioSession> sess,
              std::vector<std::byte>                body,
              const HandlerContext&                 ctx)
{
    // CS_PARTYDEL_REQ body (legacy CSHandler.cpp:3483):
    //   DWORD dwMemberID
    wire::Reader r(body.data(), body.size());
    std::uint32_t dwMemberID = 0;
    if (!r.Read(dwMemberID))
    {
        spdlog::warn("CS_PARTYDEL_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }
    spdlog::info("CS_PARTYDEL_REQ char={} member={} — F14 stub",
        handlers_detail::SenderCharId(sess, ctx), dwMemberID);
    co_return;
}

} // namespace tmapsvr
