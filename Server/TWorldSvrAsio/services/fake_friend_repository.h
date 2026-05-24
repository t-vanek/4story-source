#pragma once

// FakeFriendRepository — in-memory IFriendRepository for tests.
// Seeded through the Add*/Set* helpers; LoadForChar returns a deep
// copy of the seeded FriendLoad (empty for an unseeded char).

#include "services/friend_repository.h"

#include <mutex>
#include <unordered_map>

namespace tworldsvr {

class FakeFriendRepository : public IFriendRepository
{
public:
    void AddForward(std::uint32_t char_id, FriendRow row);
    void AddReverse(std::uint32_t char_id, FriendRow row);
    void AddGroup(std::uint32_t char_id, std::uint8_t group,
                  const std::string& name);
    void SetSoulmate(std::uint32_t char_id, std::uint32_t target,
                     std::uint32_t time_unix);

    FriendLoad LoadForChar(std::uint32_t char_id) override;

    bool MakeGroup(std::uint32_t char_id, std::uint8_t group,
                   const std::string& name) override;
    bool DeleteGroup(std::uint32_t char_id, std::uint8_t group) override;
    bool RenameGroup(std::uint32_t char_id, std::uint8_t group,
                     const std::string& name) override;
    bool ChangeFriendGroup(std::uint32_t char_id, std::uint32_t friend_id,
                           std::uint8_t group) override;

private:
    mutable std::mutex                              m_mtx;
    std::unordered_map<std::uint32_t, FriendLoad>   m_data;
};

} // namespace tworldsvr
