#include "postgresql_battle_repository.h"
#include "postgresql_world_owner.h"
#include <soci/soci.h>
#include <bit>
#include <set>
#include <stdexcept>
#include <utility>
namespace tworldsvr {
namespace {
int Required(const soci::row& row,const char* name) {
    if(row.get_indicator(name)!=soci::i_ok)throw std::runtime_error("NULL source battle field");
    return row.get<int>(name);
}
std::uint8_t Byte(const soci::row& row,const char* name) {
    const auto value=Required(row,name);
    if(value<0||value>255)throw std::runtime_error("Source battle BYTE binding unavailable");
    return static_cast<std::uint8_t>(value);
}
std::uint32_t Dword(const soci::row& row,const char* name) {
    return std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(Required(row,name)));
}
}
PostgreSQLBattleRepository::PostgreSQLBattleRepository(fourstory::db::SessionPool& pool,std::uint8_t world,
    std::string token,BattleScheduler& scheduler):m_pool(pool),m_world(world),m_token(std::move(token)) {
    if(pool.GetBackend()!=fourstory::db::Backend::PostgreSQL)throw std::runtime_error("Native battles require PostgreSQL");
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginWorldTransaction(sql,m_world,m_token,true);
    long long battles=0,custom=0;
    sql<<"SELECT manifest_sha256,source_timezone,order_sha256,battle_rows,custom_rows FROM world_compat.battle_release WHERE world_id=:w",
        soci::use(m_world,"w"),soci::into(m_manifest),soci::into(m_timezone),soci::into(m_order),soci::into(battles),soci::into(custom);
    if(!sql.got_data()||m_manifest.size()!=64||m_order.size()!=64||m_timezone.empty()||battles!=5||custom!=3)
        throw std::runtime_error("Complete pinned battle/time publication required");
    std::vector<BattleTime> rows;
    {soci::rowset<soci::row> source=(sql.prepare<<
        "SELECT * FROM world_compat.\"TBATTLETIMECHART\" WHERE world_id=:w ORDER BY \"bType\"",soci::use(m_world,"w"));
     for(const auto& row:source) {
        BattleTime time;time.type=Byte(row,"bType");time.day=Byte(row,"bDay");time.week=Byte(row,"bWeek");
        time.duration=Dword(row,"dwBattleDur");time.start=Dword(row,"dwBattleStart");
        time.alarm_start=Dword(row,"dwAlarmStart");time.alarm_end=Dword(row,"dwAlarmEnd");time.peace=Dword(row,"dwPeaceDur");
        rows.push_back(time);
     }}
    std::vector<std::uint32_t> missions;long long custom_seen=0;std::set<long long> custom_rows;
    {soci::rowset<soci::row> source=(sql.prepare<<
        "SELECT * FROM world_compat.\"TCUSTOMTIMECHART\" WHERE world_id=:w ORDER BY order_index",soci::use(m_world,"w"));
     for(const auto& row:source) {++custom_seen;const auto type=Byte(row,"bType");const auto time=Dword(row,"dwTime");
        if(row.get<long long>("order_index")!=custom_seen||!custom_rows.insert(row.get<long long>("_source_row_number")).second)
            throw std::runtime_error("Source custom-time order repeats or omits a row");
        if(type==battle::kTypeMission)missions.push_back(time);
     }}
    if(rows.size()!=static_cast<std::size_t>(battles)||custom_seen!=custom)throw std::runtime_error("Incomplete native battle/time rows");
    long long now=0;sql<<"SELECT floor(extract(epoch FROM clock_timestamp()))::bigint",soci::into(now);
    scheduler.Load(rows,std::move(missions),Calendar(sql,now));tx->commit();
}
void PostgreSQLBattleRepository::CheckRelease(soci::session& sql) {
    int count=0;
    sql<<"SELECT count(*) FROM world_compat.battle_release WHERE world_id=:w AND manifest_sha256=:m "
        "AND source_timezone=:z AND order_sha256=:o",soci::use(m_world,"w"),soci::use(m_manifest,"m"),
        soci::use(m_timezone,"z"),soci::use(m_order,"o"),soci::into(count);
    if(count!=1)throw std::runtime_error("Native battle publication changed or unavailable");
}
BattleClock PostgreSQLBattleRepository::Calendar(soci::session& sql,std::int64_t second) {
    if(second<0||second>4000000000LL)throw std::runtime_error("Source battle clock domain unavailable");
    const long long observed=second;int hour=0,minute=0,seconds=0,weekday=0;
    sql<<"SELECT extract(hour FROM wall)::integer,extract(minute FROM wall)::integer,"
        "floor(extract(second FROM wall))::integer,extract(dow FROM wall)::integer+1 "
        "FROM (SELECT to_timestamp(:at) AT TIME ZONE :zone AS wall) c",soci::use(observed,"at"),soci::use(m_timezone,"zone"),
        soci::into(hour),soci::into(minute),soci::into(seconds),soci::into(weekday);
    if(!sql.got_data()||hour<0||hour>23||minute<0||minute>59||seconds<0||seconds>59||weekday<1||weekday>7)
        throw std::runtime_error("Source battle calendar unavailable");
    return {second,static_cast<std::uint32_t>(hour*3600+minute*60+seconds),static_cast<std::uint8_t>(weekday)};
}
BattleClock PostgreSQLBattleRepository::Observe(std::int64_t second) {
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginWorldTransaction(sql,m_world,m_token,true);
    CheckRelease(sql);const auto clock=Calendar(sql,second);tx->commit();return clock;
}
}
