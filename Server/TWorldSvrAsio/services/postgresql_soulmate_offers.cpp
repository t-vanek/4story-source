#include "postgresql_soulmate_offers.h"
#include "postgresql_friend_repository.h"
#include "postgresql_world_owner.h"
#include <soci/soci.h>
#include <bit>
#include <mutex>
#include <stdexcept>

namespace tworldsvr {
namespace {
int Signed(std::uint32_t value){return std::bit_cast<std::int32_t>(value);}
std::string Hex(const std::vector<std::byte>& bytes) {
    std::string out;constexpr char digits[]="0123456789abcdef";
    out.reserve(bytes.size()*2);
    for(auto byte:bytes){const auto b=std::to_integer<unsigned char>(byte);out+=digits[b>>4];out+=digits[b&15];}
    return out;
}
std::string Hex(const std::string& bytes) {
    std::vector<std::byte> data;data.reserve(bytes.size());
    for(unsigned char byte:bytes)data.push_back(static_cast<std::byte>(byte));
    return Hex(data);
}
std::string Unhex(const std::string& hex) {
    if(hex.size()%2)throw std::runtime_error("Invalid soulmate name receipt");
    std::string out;out.reserve(hex.size()/2);
    for(std::size_t n=0;n<hex.size();n+=2)out+=static_cast<char>(std::stoul(hex.substr(n,2),nullptr,16));
    return out;
}
}

PostgreSQLSoulmateOffers::PostgreSQLSoulmateOffers(PostgreSQLFriendRepository& authority):m_authority(authority) {
    auto lease=authority.m_pool.Acquire();
    auto tx=BeginWorldTransaction(*lease,authority.m_world,authority.m_token);int rows=0;
    *lease<<"SELECT count(offer_id) FROM app_world.soulmate_offers WHERE false",soci::into(rows);
    tx->commit();
}

bool PostgreSQLSoulmateOffers::Prepare(std::uint32_t character,FriendAuthority a,std::uint16_t request_opcode,
    const std::vector<std::byte>& request,std::uint16_t response_opcode,const std::vector<std::byte>& response,
    std::uint32_t target,const std::string& name,std::uint32_t region,std::uint8_t level,std::uint8_t klass) {
    auto& repo=m_authority;std::lock_guard guard(repo.m_mutex);
    if(repo.m_failed)throw std::runtime_error("Native social persistence stopped");
    try {
        auto lease=repo.m_pool.Acquire();auto& sql=*lease;auto tx=BeginWorldTransaction(sql,repo.m_world,repo.m_token);
        if(!repo.AuthorizeSoulmateOffer(sql,character,target,a))return false;
        const int character_id=Signed(character),key=Signed(a.key),server=a.server,target_id=Signed(target);
        const int target_key=Signed(a.other_key),target_server=a.other_server,req_opcode=request_opcode,res_opcode=response_opcode;
        const int mate_level=level,mate_class=klass;const long long mate_region=region;
        const auto req=Hex(request),res=Hex(response),mate_name=Hex(name);
        int prepared=0;
        // Claim identity, source request bytes and the expected active target
        // are all held through COMMIT. Preparing creates no relationship and
        // grants no prize; a later Map transaction alone may spend/consume it.
        sql<<"WITH prepared AS (UPDATE app_world.soulmate_offers f SET status='prepared',world_owner_token=:token,"
             "response_opcode=:response_opcode,response=decode(:response,'hex'),target_id=NULLIF(:target,0),target_name=decode(:name,'hex'),"
             "target_region=:region,target_level=:level,target_class=:class,target_session_key=:target_key,target_server=:target_server,"
             "prepared_at=clock_timestamp() FROM app_world.map_sessions m "
             "WHERE f.world_id=:world AND f.char_id=:character AND f.session_key=:key AND f.source_server=:server "
             "AND f.status='pending' AND f.request_opcode=:request_opcode AND f.request=decode(:request,'hex') "
             "AND m.world_id=f.world_id AND m.char_id=f.char_id AND m.session_key=f.session_key AND m.server_id=f.source_server "
             "AND m.owner_token=f.map_owner_token AND m.connection_id=f.connection_id AND m.authority_epoch=f.authority_epoch "
             "AND m.phase='ready' AND f.created_at>clock_timestamp()-interval '15 seconds' RETURNING f.offer_id) "
             "SELECT count(*) FROM prepared",
            soci::use(repo.m_token,"token"),soci::use(res_opcode,"response_opcode"),soci::use(res,"response"),
            soci::use(target_id,"target"),soci::use(mate_name,"name"),soci::use(mate_region,"region"),soci::use(mate_level,"level"),
            soci::use(mate_class,"class"),soci::use(target_key,"target_key"),soci::use(target_server,"target_server"),
            soci::use(repo.m_world,"world"),soci::use(character_id,"character"),soci::use(key,"key"),soci::use(server,"server"),
            soci::use(req_opcode,"request_opcode"),soci::use(req,"request"),soci::into(prepared);
        if(prepared!=1)return false;
        tx->commit();return true;
    } catch(...) {
        repo.Fail();throw std::runtime_error("Native soulmate preparation failed; reconciliation required, no replay");
    }
}

std::vector<CommittedSoulmateOffer> PostgreSQLSoulmateOffers::Committed(std::int64_t after) {
    auto& repo=m_authority;std::lock_guard guard(repo.m_mutex);
    if(repo.m_failed)throw std::runtime_error("Native social persistence stopped");
    try {
        auto lease=repo.m_pool.Acquire();auto& sql=*lease;auto tx=BeginWorldTransaction(sql,repo.m_world,repo.m_token);
        const long long cursor=after;std::vector<CommittedSoulmateOffer> out;
        // Scan cyclically, not a permanent high-water mark: offer IDs are
        // allocated before COMMIT, which can occur in a different order.
        // Only the current active directed relationship is projected. The
        // original same-login primary handoff may finish before this poll.
        soci::rowset<soci::row> rows=(sql.prepare<<
            "SELECT f.offer_id,f.char_id,f.session_key,m.server_id,f.target_id,encode(f.target_name,'hex'),"
            "f.target_region,c.\"bLevel\",c.\"bClass\",f.target_session_key,f.target_server "
            "FROM app_world.soulmate_offers f JOIN app_world.\"TSOULMATETABLE\" s ON s.\"bWorldID\"=f.world_id "
            "AND s.\"dwCharID\"=f.char_id AND s.\"dwTarget\"=f.target_id AND s.\"dwTime\"=0 "
            "JOIN app_world.\"TCHARTABLE\" c ON c.\"bWorldID\"=f.world_id AND c.\"dwCharID\"=f.target_id AND c.\"bDelete\"=0 "
            "JOIN app_world.map_sessions m ON m.world_id=f.world_id AND m.char_id=f.char_id AND m.session_key=f.session_key AND m.phase='ready' "
            "JOIN app_global.\"TCURRENTUSER\" u ON u.\"bGroupID\"=m.world_id AND u.\"dwCharID\"=m.char_id "
            "AND u.\"dwUserID\"=m.user_id AND u.\"dwKEY\"=m.session_key AND u.\"bChannel\"=m.channel "
            "JOIN app_world.map_runtime_owner o ON o.world_id=m.world_id AND o.server_id=m.server_id AND o.owner_token=m.owner_token "
            "WHERE f.world_id=:world AND f.world_owner_token=:token "
            "AND f.status='consumed' AND f.offer_id>:cursor AND f.committed_operation_id IS NOT NULL "
            "AND EXISTS(SELECT 1 FROM pg_locks l WHERE l.pid=o.backend_pid AND l.locktype='advisory' "
            "AND l.classid=CAST(877478224 AS oid) AND l.objid=CAST((o.world_id::integer<<8)|o.server_id::integer AS oid) "
            "AND l.objsubid=2 AND l.granted) "
            "AND NOT EXISTS(SELECT 1 FROM app_world.soulmate_offers newer WHERE newer.world_id=f.world_id AND newer.char_id=f.char_id "
            "AND newer.status='consumed' AND newer.offer_id>f.offer_id) "
            "ORDER BY f.offer_id LIMIT 128",soci::use(repo.m_world,"world"),soci::use(repo.m_token,"token"),soci::use(cursor,"cursor"));
        for(const auto& r:rows)out.push_back({r.get<long long>(0),static_cast<std::uint32_t>(r.get<int>(1)),
            static_cast<std::uint32_t>(r.get<int>(2)),static_cast<std::uint32_t>(r.get<int>(4)),
            static_cast<std::uint32_t>(r.get<long long>(6)),static_cast<std::uint32_t>(r.get<int>(9)),
            static_cast<std::uint8_t>(r.get<int>(3)),static_cast<std::uint8_t>(r.get<int>(7)),
            static_cast<std::uint8_t>(r.get<int>(8)),static_cast<std::uint8_t>(r.get<int>(10)),Unhex(r.get<std::string>(5))});
        tx->commit();return out;
    } catch(...) {
        repo.Fail();throw std::runtime_error("Native soulmate reconciliation failed; World stopped");
    }
}
}
