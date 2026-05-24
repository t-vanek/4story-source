#include "services/fake_friend_repository.h"

namespace tworldsvr {

void FakeFriendRepository::AddForward(std::uint32_t char_id, FriendRow row)
{
    std::lock_guard g(m_mtx);
    m_data[char_id].forward.push_back(std::move(row));
}

void FakeFriendRepository::AddReverse(std::uint32_t char_id, FriendRow row)
{
    std::lock_guard g(m_mtx);
    m_data[char_id].reverse.push_back(std::move(row));
}

void FakeFriendRepository::AddGroup(std::uint32_t char_id, std::uint8_t group,
                                    const std::string& name)
{
    std::lock_guard g(m_mtx);
    m_data[char_id].groups.emplace_back(group, name);
}

void FakeFriendRepository::SetSoulmate(std::uint32_t char_id,
                                       std::uint32_t target,
                                       std::uint32_t time_unix)
{
    std::lock_guard g(m_mtx);
    auto& fl = m_data[char_id];
    fl.has_soulmate    = target != 0;
    fl.soulmate_target = target;
    fl.soulmate_time   = time_unix;
}

FriendLoad FakeFriendRepository::LoadForChar(std::uint32_t char_id)
{
    std::lock_guard g(m_mtx);
    auto it = m_data.find(char_id);
    return it == m_data.end() ? FriendLoad{} : it->second;
}

} // namespace tworldsvr
