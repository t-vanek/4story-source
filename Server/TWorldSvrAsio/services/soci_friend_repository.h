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

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tworldsvr
