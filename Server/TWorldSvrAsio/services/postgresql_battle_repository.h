#pragma once
#include "battle_scheduler.h"
#include "fourstory/db/session_pool.h"
#include <string>
namespace soci {class session;}
namespace tworldsvr {
class PostgreSQLBattleRepository {
public:
    PostgreSQLBattleRepository(fourstory::db::SessionPool&,std::uint8_t world,std::string token,BattleScheduler&);
    BattleClock Observe(std::int64_t second);
private:
    void CheckRelease(soci::session&);
    BattleClock Calendar(soci::session&,std::int64_t);
    fourstory::db::SessionPool& m_pool;
    int m_world;
    std::string m_token,m_manifest,m_timezone,m_order;
};
}
