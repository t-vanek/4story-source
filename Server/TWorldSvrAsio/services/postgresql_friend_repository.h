#pragma once
#include "friend_repository.h"
#include "fourstory/db/session_pool.h"
#include <atomic>
#include <functional>
#include <mutex>
namespace tworldsvr {
class PostgreSQLFriendRepository final : public IFriendRepository {
public:
    PostgreSQLFriendRepository(fourstory::db::SessionPool&,std::uint8_t world,std::string owner_token,
                              std::string wire_encoding,std::function<void()> stop);
    FriendLoad LoadForChar(std::uint32_t,FriendAuthority = {}) override;
    bool MakeGroup(std::uint32_t,std::uint8_t,const std::string&,FriendAuthority = {}) override;
    bool DeleteGroup(std::uint32_t,std::uint8_t,FriendAuthority = {}) override;
    bool RenameGroup(std::uint32_t,std::uint8_t,const std::string&,FriendAuthority = {}) override;
    bool ChangeFriendGroup(std::uint32_t,std::uint32_t,std::uint8_t,FriendAuthority = {}) override;
    bool InsertFriend(std::uint32_t,std::uint32_t,FriendAuthority = {}) override;
    bool InsertMutualFriends(std::uint32_t,std::uint32_t,FriendAuthority = {}) override;
    bool EraseFriend(std::uint32_t,std::uint32_t,FriendAuthority = {}) override;
    bool RegSoulmate(std::uint32_t,std::uint32_t,FriendAuthority = {}) override;
    bool DelSoulmate(std::uint32_t,std::uint32_t,FriendAuthority = {}) override;
    bool EndSoulmate(std::uint32_t,std::uint32_t,FriendAuthority = {}) override;
    bool EndSoulmateExact(std::uint32_t,std::uint32_t,std::uint32_t,FriendAuthority = {}) override;
private:
    friend class PostgreSQLSoulmateOffers;
    bool AuthorizeSoulmateOffer(soci::session&,std::uint32_t,std::uint32_t,FriendAuthority);
    bool Mutate(const std::string& kind,std::uint32_t character,std::uint32_t target,std::uint8_t group,
                const std::string& name,std::uint32_t time,FriendAuthority);
    void LockActors(soci::session&,std::uint32_t,std::uint32_t,FriendAuthority,bool mutual);
    void GuardActor(soci::session&,std::uint32_t,FriendAuthority,bool loading=false);
    void Fail();
    fourstory::db::SessionPool& m_pool;
    int m_world;
    std::string m_token,m_encoding;
    std::function<void()> m_stop;
    std::mutex m_mutex;
    std::atomic<bool> m_failed{false};
};
}
