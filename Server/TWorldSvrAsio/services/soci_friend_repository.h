#pragma once

// SociFriendRepository — IFriendRepository backed by SOCI against
// TFRIENDTABLE + TFRIENDGROUPTABLE + TSOULMATETABLE. Mirrors the
// legacy load queries (DBAccess.h CTBLFriendGroupTable / CTBLFriend /
// CTBLFriendTarget): names / level / class come from a JOIN against
// TCHARTABLE on the friend id.

#include "services/friend_repository.h"

#include "fourstory/db/session_pool.h"
#include <atomic>
#include <functional>
#include <mutex>

namespace tworldsvr {

class SociFriendRepository : public IFriendRepository
{
public:
    explicit SociFriendRepository(fourstory::db::SessionPool& pool,std::function<void()> stop_on_failure={})
        : m_pool(pool),m_stop_on_failure(std::move(stop_on_failure)) {}

    FriendLoad LoadForChar(std::uint32_t char_id, FriendAuthority authority = {}) override;

    bool MakeGroup(std::uint32_t char_id, std::uint8_t group,
                   const std::string& name, FriendAuthority authority = {}) override;
    bool DeleteGroup(std::uint32_t char_id, std::uint8_t group, FriendAuthority authority = {}) override;
    bool RenameGroup(std::uint32_t char_id, std::uint8_t group,
                     const std::string& name, FriendAuthority authority = {}) override;
    bool ChangeFriendGroup(std::uint32_t char_id, std::uint32_t friend_id,
                           std::uint8_t group, FriendAuthority authority = {}) override;
    bool InsertFriend(std::uint32_t char_id, std::uint32_t friend_id, FriendAuthority authority = {}) override;
    bool InsertMutualFriends(std::uint32_t char_id, std::uint32_t friend_id, FriendAuthority authority = {}) override;
    bool EraseFriend(std::uint32_t char_id, std::uint32_t friend_id, FriendAuthority authority = {}) override;
    bool RegSoulmate(std::uint32_t char_id, std::uint32_t target, FriendAuthority authority = {}) override;
    bool EndSoulmate(std::uint32_t char_id, std::uint32_t time, FriendAuthority authority = {}) override;
    bool DelSoulmate(std::uint32_t char_id, std::uint32_t target, FriendAuthority authority = {}) override;

private:
    fourstory::db::SessionPool& m_pool;
    std::mutex m_pair_mutex;
    std::atomic<bool> m_pair_failed{false};
    std::function<void()> m_stop_on_failure;
    void CheckWritable() const;
};

} // namespace tworldsvr
