#include "chat_ban_repository.h"

#include <chrono>

namespace tcontrolsvr {

std::uint32_t
ChatBanRepository::CreateBan(const std::string& operator_id,
                             const std::string& target_user,
                             std::uint16_t minutes,
                             const std::string& reason,
                             std::uint32_t world_count,
                             std::uint32_t manager_seq)
{
    const std::uint32_t seq = ++m_next_seq;
    ChatBanInfo info{};
    info.seq           = seq;
    info.operator_id   = operator_id;
    info.target_user   = target_user;
    info.minutes       = minutes;
    info.reason        = reason;
    info.created_unix  = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    info.world_count   = world_count;
    info.pending_count = world_count;
    info.manager_seq   = manager_seq;
    m_bans.emplace(seq, std::move(info));
    return seq;
}

ChatBanRepository::AckResult
ChatBanRepository::ApplyAck(std::uint32_t seq, std::uint8_t ret)
{
    AckResult out{};
    auto it = m_bans.find(seq);
    if (it == m_bans.end())
    {
        // Late ack — registry already pruned. Surface as a no-op
        // completion so the handler doesn't dangle.
        out.completed = true;
        out.success   = false;
        return out;
    }
    auto& info = it->second;
    if (ret) info.success_so_far = true;
    if (info.pending_count > 0) info.pending_count -= 1;

    if (info.pending_count == 0)
    {
        out.completed   = true;
        out.success     = info.success_so_far;
        out.manager_seq = info.manager_seq;

        // Legacy: prune the entry if the aggregated outcome is a
        // failure. Successful bans stay in the registry so
        // CT_CHATBANLIST_REQ can render them. Match the legacy
        // delete-on-failure behavior.
        if (!info.success_so_far)
            m_bans.erase(it);
    }
    return out;
}

std::vector<ChatBanInfo>
ChatBanRepository::List() const
{
    std::vector<ChatBanInfo> out;
    out.reserve(m_bans.size());
    for (const auto& [seq, info] : m_bans)
        out.push_back(info);
    return out;
}

std::size_t ChatBanRepository::Delete(std::uint32_t seq)
{
    if (seq == 0)
    {
        const auto n = m_bans.size();
        m_bans.clear();
        return n;
    }
    return m_bans.erase(seq);
}

} // namespace tcontrolsvr
