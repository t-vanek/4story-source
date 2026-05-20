// CS_CHAT_REQ handler — F6 Part 1.
//
// Wire body (CS_CHAT_REQ):
//   CString strSender, BYTE bGroup, DWORD dwTarget,
//   CString strName, CString strTalk
//
// CS_CHAT_ACK broadcast:
//   BYTE bGroup, DWORD dwSenderID, CString strName, CString strTalk
//
// Chat group types handled in F6 Part 1:
//   0 = CHAT_NEAR   → broadcast to AOI neighbours
//   1 = CHAT_SHOUT  → broadcast to all map players (all sessions)
//   2 = CHAT_WHISPER → send to single target session
// All other types → PENDING (party, guild, tactics, etc.)
//
// Source:
//   CSHandler.cpp:5206 — OnCS_CHAT_REQ
//   CSSender.cpp:2621  — SendCS_CHAT_ACK

#include "handlers.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

namespace tmapsvr {

using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

// Chat group constants (legacy CHAT_* enum)
constexpr std::uint8_t CHAT_NEAR    = 0;
constexpr std::uint8_t CHAT_SHOUT   = 1;
constexpr std::uint8_t CHAT_WHISPER = 2;

boost::asio::awaitable<void>
SendChatAck(std::shared_ptr<tnetlib::AsioSession> sess,
            std::uint8_t                          group,
            std::uint32_t                         sender_id,
            const std::string&                    sender_name,
            const std::string&                    message)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint8_t> (body, group);
    wire::WritePOD<std::uint32_t>(body, sender_id);
    wire::WriteString(body, sender_name);
    wire::WriteString(body, message);
    co_await sess->SendPacket(
        ToUint16(MessageId::CS_CHAT_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// CS_CHAT_REQ handler
// ---------------------------------------------------------------------------

boost::asio::awaitable<void>
OnChatReq(std::shared_ptr<tnetlib::AsioSession> sess,
          MapSessionState&                     state,
          const tnetlib::DecodedPacket&        packet,
          const HandlerContext&                ctx)
{
    if (!state.connected || !state.snapshot) co_return;

    wire::Reader r(packet.body.data(), packet.body.size());
    std::string  sender_str, name_str, talk_str;
    std::uint8_t group    = 0;
    std::uint32_t target  = 0;

    if (!r.ReadString(sender_str) || !r.Read(group) || !r.Read(target) ||
        !r.ReadString(name_str)   || !r.ReadString(talk_str))
    {
        spdlog::warn("CS_CHAT_REQ malformed uid={}", state.user_id);
        co_return;
    }

    // Sanitise: cap message length at 256 chars
    if (talk_str.size() > 256) talk_str.resize(256);

    const std::string& char_name = state.snapshot->name;
    spdlog::info("CS_CHAT_REQ uid={} '{}' group={} msg='{}'",
        state.user_id, char_name, group, talk_str);

    // §1 CHAT_NEAR — broadcast to AOI neighbours
    if (group == CHAT_NEAR || group == CHAT_SHOUT)
    {
        if (ctx.session_registry && ctx.map_state && state.snapshot)
        {
            const auto aoi = ctx.map_state->GetNeighborIds(
                state.snapshot->position.pos_x,
                state.snapshot->position.pos_z);
            for (std::uint32_t pid : aoi)
            {
                auto nbr = ctx.session_registry->Get(pid);
                if (nbr && pid != state.char_id)
                    co_await SendChatAck(nbr, group, state.char_id,
                        char_name, talk_str);
            }
        }
        // Echo back to sender
        co_await SendChatAck(sess, group, state.char_id,
            char_name, talk_str);
    }
    else if (group == CHAT_WHISPER)
    {
        // Direct whisper to target
        auto target_sess = ctx.session_registry
            ? ctx.session_registry->Get(target) : nullptr;
        if (target_sess)
            co_await SendChatAck(target_sess, group, state.char_id,
                char_name, talk_str);
        // Echo back so sender sees their own whisper
        co_await SendChatAck(sess, group, state.char_id,
            char_name, talk_str);
    }
    else
    {
        spdlog::debug("CS_CHAT_REQ: group={} unhandled (party/guild/tactics "
                      "pending F6b)", group);
        co_await SendChatAck(sess, group, state.char_id,
            char_name, talk_str);
    }
}

} // namespace tmapsvr
