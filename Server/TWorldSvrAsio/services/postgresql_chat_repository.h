#pragma once
#include "friend_repository.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace tworldsvr {
class PostgreSQLFriendRepository;
struct ChatAuthority {bool valid{},is_operator{};std::int64_t ban_until{};};
class PostgreSQLChatRepository {
public:
    explicit PostgreSQLChatRepository(PostgreSQLFriendRepository&);
    std::vector<std::uint32_t> LoadOperators();
    ChatAuthority Authorize(std::uint32_t character,FriendAuthority,bool loading=false);
    // Target must be a current online primary. Map automatic bans use issuer0;
    // Control uses replace_deadline and is authorized by its registered peer.
    std::optional<std::int64_t> Ban(std::uint32_t target,const std::string& name,
        FriendAuthority target_authority,std::uint16_t minutes,std::uint32_t issuer,
        FriendAuthority issuer_authority={},bool replace_deadline=false);
private:
    PostgreSQLFriendRepository& m_authority;
};
}
