#pragma once

// In-memory chat-ban registry — mirrors legacy m_mapBanInfo on
// CTControlSvrModule. Tracks active chat bans for the
// CT_CHATBANLIST_REQ display, plus the per-ban N-wave aggregation
// state for CT_CHATBAN_REQ → forward to all World/Relay → each
// CT_CHATBAN_ACK back from the world servers.
//
// Legacy contract:
//   - CT_CHATBAN_REQ assigns a global `m_dwChatBanSeq` to the ban
//     and forwards to every World+Relay peer. m_dwSendCount counts
//     World peers only (legacy code).
//   - Each World replies CT_CHATBAN_ACK with the same seq +
//     bRet + manager id. m_dwSendCount-- on each.
//   - When m_dwSendCount hits zero we send the operator one
//     CT_CHATBAN_ACK with the OR-aggregated bRet.
//   - Failed aggregates also remove the ban from the registry.
//
// Aggregation state lives here so the F3 handlers can be stateless.

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tcontrolsvr {

struct ChatBanInfo
{
    std::uint32_t  seq           = 0;   // legacy m_dwChatBanSeq
    std::string    operator_id;         // legacy m_strOPName
    std::string    target_user;         // legacy m_strBanName
    std::string    reason;              // legacy m_strReason
    std::uint16_t  minutes       = 0;
    std::int64_t   created_unix  = 0;

    // N-wave aggregation. world_count = expected number of world acks.
    // pending = remaining; success = OR of bRet values seen so far.
    std::uint32_t  world_count       = 0;
    std::uint32_t  pending_count     = 0;
    std::uint32_t  manager_seq       = 0;  // operator that issued the ban
    bool           success_so_far    = false;
};

class ChatBanRepository
{
public:
    // Allocate a fresh seq, store the meta. Returns the seq the
    // handler should forward to the peers.
    std::uint32_t CreateBan(const std::string& operator_id,
                            const std::string& target_user,
                            std::uint16_t minutes,
                            const std::string& reason,
                            std::uint32_t world_count,
                            std::uint32_t manager_seq);

    // Apply one CT_CHATBAN_ACK to the aggregation. Returns true
    // when all expected world acks have arrived (caller may then
    // pick up the final aggregated success flag and forward to the
    // operator). When the aggregation completes with bRet=0 the
    // ban is removed from the registry (matches legacy
    // `delete (*it).second; m_mapBanInfo.erase(it);`).
    struct AckResult
    {
        bool          completed       = false;
        bool          success         = false;
        std::uint32_t manager_seq     = 0;
    };
    AckResult ApplyAck(std::uint32_t seq, std::uint8_t ret);

    // Snapshot for CT_CHATBANLIST_REQ display.
    std::vector<ChatBanInfo> List() const;

    // CT_CHATBANLISTDEL_REQ: remove one (when seq != 0) or all
    // (when seq == 0). Returns number removed.
    std::size_t Delete(std::uint32_t seq);

    std::size_t Size() const { return m_bans.size(); }

private:
    std::uint32_t                                     m_next_seq = 0;
    std::unordered_map<std::uint32_t, ChatBanInfo>    m_bans;
};

} // namespace tcontrolsvr
