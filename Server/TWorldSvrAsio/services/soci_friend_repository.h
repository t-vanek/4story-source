#pragma once

// SociFriendRepository — IFriendRepository backed by SOCI against
// TFRIENDTABLE + TFRIENDGROUPTABLE + TSOULMATETABLE. Mirrors the
// legacy load queries (DBAccess.h CTBLFriendGroupTable / CTBLFriend /
// CTBLFriendTarget): names / level / class come from a JOIN against
// TCHARTABLE on the friend id.

#include "services/friend_repository.h"

#include "fourstory/db/session_pool.h"

namespace tworldsvr {

class SociFriendRepository : public IFriendRepository
{
public:
    explicit SociFriendRepository(fourstory::db::SessionPool& pool)
        : m_pool(pool) {}

    FriendLoad LoadForChar(std::uint32_t char_id) override;

    bool MakeGroup(std::uint32_t char_id, std::uint8_t group,
                   const std::string& name) override;
    bool DeleteGroup(std::uint32_t char_id, std::uint8_t group) override;
    bool RenameGroup(std::uint32_t char_id, std::uint8_t group,
                     const std::string& name) override;
    bool ChangeFriendGroup(std::uint32_t char_id, std::uint32_t friend_id,
                           std::uint8_t group) override;
    bool InsertFriend(std::uint32_t char_id, std::uint32_t friend_id) override;
    bool EraseFriend(std::uint32_t char_id, std::uint32_t friend_id) override;
    bool RegSoulmate(std::uint32_t char_id, std::uint32_t target) override;
    bool DelSoulmate(std::uint32_t char_id, std::uint32_t target) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tworldsvr
