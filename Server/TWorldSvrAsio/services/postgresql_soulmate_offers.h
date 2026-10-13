#pragma once
#include "friend_repository.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tworldsvr {
class PostgreSQLFriendRepository;

// Internal receipts only. Original MW/CS packets carry no offer identity.
// Map creates a pending receipt before the World lookup, then atomically
// consumes the prepared result with purse/items/the directed relationship.
struct CommittedSoulmateOffer {
    std::int64_t offer_id=0;
    std::uint32_t character=0,key=0,target=0,region=0,target_key=0;
    std::uint8_t server=0,level=0,klass=0,target_server=0;
    std::string name;
};

class PostgreSQLSoulmateOffers {
public:
    explicit PostgreSQLSoulmateOffers(PostgreSQLFriendRepository&);
    bool Prepare(std::uint32_t character,FriendAuthority,std::uint16_t request_opcode,
                 const std::vector<std::byte>& request,std::uint16_t response_opcode,
                 const std::vector<std::byte>& response,std::uint32_t target,
                 const std::string& name,std::uint32_t region,std::uint8_t level,std::uint8_t klass);
    std::vector<CommittedSoulmateOffer> Committed(std::int64_t after);
private:
    PostgreSQLFriendRepository& m_authority;
};
}
