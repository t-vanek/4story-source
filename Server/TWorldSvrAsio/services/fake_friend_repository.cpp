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

bool FakeFriendRepository::MakeGroup(std::uint32_t char_id, std::uint8_t group,
                                     const std::string& name)
{
    std::lock_guard g(m_mtx);
    m_data[char_id].groups.emplace_back(group, name);
    return true;
}

bool FakeFriendRepository::DeleteGroup(std::uint32_t char_id,
                                       std::uint8_t group)
{
    std::lock_guard g(m_mtx);
    auto& v = m_data[char_id].groups;
    for (auto it = v.begin(); it != v.end(); ++it)
        if (it->first == group) { v.erase(it); break; }
    return true;
}

bool FakeFriendRepository::RenameGroup(std::uint32_t char_id,
                                       std::uint8_t group,
                                       const std::string& name)
{
    std::lock_guard g(m_mtx);
    for (auto& gp : m_data[char_id].groups)
        if (gp.first == group) gp.second = name;
    return true;
}

bool FakeFriendRepository::ChangeFriendGroup(std::uint32_t char_id,
                                             std::uint32_t friend_id,
                                             std::uint8_t group)
{
    std::lock_guard g(m_mtx);
    for (auto& f : m_data[char_id].forward)
        if (f.id == friend_id) f.group = group;
    return true;
}

bool FakeFriendRepository::InsertFriend(std::uint32_t char_id,
                                        std::uint32_t friend_id)
{
    std::lock_guard g(m_mtx);
    auto& fwd = m_data[char_id].forward;
    for (const auto& f : fwd)
        if (f.id == friend_id) return true;   // already present
    fwd.push_back(FriendRow{friend_id, "", 0, 0, 0});
    return true;
}

bool FakeFriendRepository::EraseFriend(std::uint32_t char_id,
                                       std::uint32_t friend_id)
{
    std::lock_guard g(m_mtx);
    auto& fwd = m_data[char_id].forward;
    for (auto it = fwd.begin(); it != fwd.end(); ++it)
        if (it->id == friend_id) { fwd.erase(it); break; }
    return true;
}

bool FakeFriendRepository::RegSoulmate(std::uint32_t char_id,
                                       std::uint32_t target)
{
    std::lock_guard g(m_mtx);
    auto& fl = m_data[char_id];
    fl.has_soulmate    = target != 0;
    fl.soulmate_target = target;
    fl.soulmate_time   = 0;
    return true;
}

bool FakeFriendRepository::DelSoulmate(std::uint32_t char_id,
                                       std::uint32_t target)
{
    std::lock_guard g(m_mtx);
    auto& fl = m_data[char_id];
    if (fl.soulmate_target == target)
    {
        fl.has_soulmate    = false;
        fl.soulmate_target = 0;
        fl.soulmate_time   = 0;
    }
    return true;
}

} // namespace tworldsvr
