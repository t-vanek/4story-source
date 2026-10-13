#include "postgresql_guild_level_repository.h"
#include "postgresql_world_owner.h"
#include <soci/soci.h>
#include <bit>
#include <set>
#include <stdexcept>

namespace tworldsvr {
PostgreSQLGuildLevelRepository::PostgreSQLGuildLevelRepository(fourstory::db::SessionPool& pool,
    std::uint8_t world,std::string token):m_pool(pool),m_world(world),m_token(std::move(token)) {
    if(pool.GetBackend()!=fourstory::db::Backend::PostgreSQL)throw std::runtime_error("Native guild levels require PostgreSQL");
}
std::vector<TGuildLevelRow> PostgreSQLGuildLevelRepository::LoadAll() {
    try {
        auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginWorldTransaction(sql,m_world,m_token,true);
        soci::row release;
        sql<<"SELECT manifest_sha256,value_sha256,row_count,status FROM world_compat.guild_level_release",soci::into(release);
        if(!sql.got_data()||release.get<std::string>(0).size()!=64||release.get<std::string>(1).size()!=64||
           release.get<long long>(2)!=11||release.get<std::string>(3)!="verified")
            throw std::runtime_error("Full verified native guild level publication required");
        std::vector<TGuildLevelRow> out;std::set<std::uint8_t> levels;
        {soci::rowset<soci::row> rows=(sql.prepare<<
            "SELECT \"bLevel\",\"dwEXP\",\"bMaxCnt\",\"bMinCnt\",\"bCabinetCnt\",\"bTacticsCnt\",\"bBattleSetCnt\","
            "\"bGuardCnt\",\"bRoyalGuardCnt\",\"bTurretCnt\",\"bPeer1\",\"bPeer2\",\"bPeer3\",\"bPeer4\",\"bPeer5\" "
            "FROM world_compat.\"TGUILDCHART\" ORDER BY \"bLevel\"");
         for(const auto& r:rows) {
            const auto byte=[&](std::size_t i){
                if(r.get_indicator(i)!=soci::i_ok)throw std::runtime_error("NULL native guild chart field");
                const auto value=r.get<int>(i);
                if(value<0||value>255)throw std::runtime_error("Native guild chart BYTE out of range");
                return static_cast<std::uint8_t>(value);
            };
            TGuildLevelRow row;row.level=byte(0);
            if(row.level>kMaxGuildLevel||!levels.insert(row.level).second||r.get_indicator(1)!=soci::i_ok)
                throw std::runtime_error("Invalid native guild chart level key");
            row.exp=std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(r.get<int>(1)));
            row.max_count=byte(2);row.min_count=byte(3);row.cabinet_count=byte(4);row.tactics_count=byte(5);
            row.battle_set_count=byte(6);row.guard_count=byte(7);row.royal_guard_count=byte(8);row.turret_count=byte(9);
            for(std::size_t i=0;i<row.peer_slots.size();++i)row.peer_slots[i]=byte(i+10);
            out.push_back(row);
         }}
        if(out.size()!=static_cast<std::size_t>(kMaxGuildLevel)+1)throw std::runtime_error("Incomplete native guild chart");
        tx->commit();return out;
    } catch(...) {throw std::runtime_error("Native guild level bootstrap failed; no empty/default gameplay fallback");}
}
}
