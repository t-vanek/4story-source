#include "postgresql_rps_repository.h"
#include "postgresql_friend_repository.h"
#include "postgresql_world_owner.h"
#include <soci/soci.h>
#include <bit>
#include <ctime>
#include <limits>
#include <set>
#include <stdexcept>

namespace tworldsvr {
namespace {
int Signed(std::uint32_t value){return std::bit_cast<std::int32_t>(value);}
int Required(const soci::row& row,std::size_t column) {
    if(row.get_indicator(column)!=soci::i_ok)throw std::runtime_error("NULL native RPS value");
    return row.get<int>(column);
}
std::uint8_t Byte(const soci::row& row,std::size_t column) {
    const auto n=Required(row,column);
    if(n<0||n>255)throw std::runtime_error("Native RPS BYTE out of range");
    return static_cast<std::uint8_t>(n);
}
std::uint16_t Word(const soci::row& row,std::size_t column) {
    const auto n=Required(row,column);
    if(n<std::numeric_limits<std::int16_t>::min()||n>std::numeric_limits<std::int16_t>::max())
        throw std::runtime_error("Native RPS SMALLINT out of range");
    // Original SQL_C_SSHORT binding writes signed SMALLINT bits to a WORD.
    return std::bit_cast<std::uint16_t>(static_cast<std::int16_t>(n));
}
}
PostgreSQLRpsRepository::PostgreSQLRpsRepository(PostgreSQLFriendRepository& authority):m_authority(authority) {
    if(authority.m_pool.GetBackend()!=fourstory::db::Backend::PostgreSQL)
        throw std::runtime_error("Native RPS requires PostgreSQL");
    auto lease=authority.m_pool.Acquire();auto& sql=*lease;
    auto tx=BeginWorldTransaction(sql,authority.m_world,authority.m_token,true);
    soci::row release;
    sql<<"SELECT manifest_sha256,chart_rows,source_timezone FROM world_compat.rps_release WHERE world_id=:w",
        soci::use(authority.m_world,"w"),soci::into(release);
    if(!sql.got_data()||release.get<std::string>(0).size()!=64||release.get<std::string>(2).empty())
        throw std::runtime_error("Full verified native RPS publication required");
    m_manifest=release.get<std::string>(0);
    m_timezone=release.get<std::string>(2);
    const auto expected=release.get<long long>(1);std::set<std::uint16_t> keys;
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT \"bType\",\"bWinCount\",\"bProb_Win\",\"bProb_Draw\",\"bProb_Lose\",\"wWinKeep\",\"wWinPeriod\" "
        "FROM world_compat.\"TRPSGAMECHART\" WHERE world_id=:w ORDER BY \"bWinCount\",\"bType\"",
        soci::use(authority.m_world,"w"));
     for(const auto& row:rows) {
        TRpsGame game;game.type=Byte(row,0);game.win_count=Byte(row,1);
        game.win_prob=Byte(row,2);game.draw_prob=Byte(row,3);game.lose_prob=Byte(row,4);
        game.win_keep=Word(row,5);game.win_period=Word(row,6);
        if(!keys.insert(static_cast<std::uint16_t>(game.type|(game.win_count<<8))).second)
            throw std::runtime_error("Duplicate native RPS chart key");
        m_games.push_back(game);
     }}
    // Plain16 total <=65534; reply body has4 fixed bytes and9 per source row.
    constexpr std::size_t max_rows=(65534-16-4)/9;
    if(expected<0||expected>static_cast<long long>(max_rows)||m_games.size()!=static_cast<std::size_t>(expected))
        throw std::runtime_error("Incomplete native RPS chart");
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT game_type,win_count,hydrate_second FROM app_world.rps_records WHERE world_id=:w ORDER BY record_id",
        soci::use(authority.m_world,"w"));
     for(const auto& row:rows) {
        if(row.get_indicator(2)!=soci::i_ok)throw std::runtime_error("NULL native RPS time");
        const auto second=row.get<long long>(2);
        // Bounds also make signed age subtraction safe, including future dates.
        if(second < -3000000000LL||second>4000000000LL||second%60)
            throw std::runtime_error("Native RPS calendar domain is unavailable");
        m_dates.push_back({Byte(row,0),Byte(row,1),second});
     }}
    // Missing074/privileges must fail startup even when the genuine chart is empty.
    int probe=0;sql<<"SELECT count(operation_count) FROM app_world.rps_operations WHERE false",soci::into(probe);
    tx->commit();
}
std::vector<TRpsGame> PostgreSQLRpsRepository::LoadGames(){return m_games;}
std::vector<RpsWinDateRow> PostgreSQLRpsRepository::LoadWinDates(){return m_dates;}
void PostgreSQLRpsRepository::Stop(){if(!m_failed.exchange(true))m_authority.Fail();}
void PostgreSQLRpsRepository::CheckRelease(soci::session& sql) {
    if(m_failed||m_authority.m_failed)throw std::runtime_error("Native RPS stopped for reconciliation");
    int found=0;
    sql<<"SELECT 1 FROM world_compat.rps_release WHERE world_id=:w AND manifest_sha256=:m AND source_timezone=:tz",
        soci::use(m_authority.m_world,"w"),soci::use(m_manifest,"m"),soci::use(m_timezone,"tz"),soci::into(found);
    if(!sql.got_data()||found!=1)throw std::runtime_error("Native RPS publication changed");
}
void PostgreSQLRpsRepository::WriteRecord(soci::session& sql,const RpsRegistry::PersistOp& op) {
    if(op.date_unix < -3000000000LL||op.date_unix>4000000000LL)
        throw std::runtime_error("Native RPS timestamp outside supported calendar");
    const long long observed=op.date_unix;long long hydrated=0;std::tm stored{};
    sql<<"SELECT win_time,hydrate_second FROM app_world.rps_source_minute(:d,:tz)",
        soci::use(observed,"d"),soci::use(m_timezone,"tz"),soci::into(stored),soci::into(hydrated);
    if(!sql.got_data())throw std::runtime_error("Native RPS source minute unavailable");
    const int type=op.type,count=op.win_count,character=Signed(op.char_id);
    if(op.insert)sql<<"INSERT INTO app_world.rps_records(world_id,game_type,win_count,win_time,hydrate_second,character_id) "
        "VALUES(:w,:t,:c,:d,:h,:id)",soci::use(m_authority.m_world,"w"),soci::use(type,"t"),
        soci::use(count,"c"),soci::use(stored,"d"),soci::use(hydrated,"h"),soci::use(character,"id");
    else sql<<"DELETE FROM app_world.rps_records WHERE world_id=:w AND game_type=:t AND win_count=:c AND win_time<=:d",
        soci::use(m_authority.m_world,"w"),soci::use(type,"t"),soci::use(count,"c"),soci::use(stored,"d");
}
bool PostgreSQLRpsRepository::Record(bool insert,std::uint32_t character,std::uint8_t type,
    std::uint8_t count,std::int64_t date) {
    // Internal source DM contract. Native network dispatch rejects remote DM;
    // gameplay uses CommitWin to include actor authority and the entire batch.
    std::lock_guard guard(m_mutex);
    try {
        auto lease=m_authority.m_pool.Acquire();auto& sql=*lease;
        auto tx=BeginWorldTransaction(sql,m_authority.m_world,m_authority.m_token);
        CheckRelease(sql);WriteRecord(sql,{insert,character,type,count,date});tx->commit();return true;
    } catch(...) {Stop();throw std::runtime_error("Native RPS record failed; stopped without retry");}
}
bool PostgreSQLRpsRepository::CommitWin(const RpsRegistry::WinPlan& plan,std::uint32_t character,
    FriendAuthority authority,const std::string& map_token,std::uint8_t type,std::uint8_t count,std::int64_t now) {
    std::lock_guard guard(m_mutex);
    try {
        auto lease=m_authority.m_pool.Acquire();auto& sql=*lease;
        auto tx=BeginWorldTransaction(sql,m_authority.m_world,m_authority.m_token);
        CheckRelease(sql);
        if(!m_authority.AuthorizeCommunication(sql,character,authority,false))return false;
        const int server=authority.server;std::string token;
        sql<<"SELECT owner_token FROM app_world.map_runtime_owner WHERE world_id=:w AND server_id=:s",
            soci::use(m_authority.m_world,"w"),soci::use(server,"s"),soci::into(token);
        if(!sql.got_data()||token!=map_token||token.empty())return false;
        for(const auto& op:plan.operations)WriteRecord(sql,op);
        const int id=Signed(character),key=Signed(authority.key),ty=type,wc=count;
        const int allowed=plan.outcome==RpsRegistry::Outcome::kAllowed?1:0;
        const int size=static_cast<int>(plan.operations.size());const long long observed=now;
        sql<<"INSERT INTO app_world.rps_operations(world_id,owner_token,character_id,session_key,server_id,game_type,win_count,"
            "request_second,allowed,operation_count) VALUES(:w,:owner,:id,:key,:s,:t,:c,:now,(:ok=1),:n)",
            soci::use(m_authority.m_world,"w"),soci::use(m_authority.m_token,"owner"),soci::use(id,"id"),
            soci::use(key,"key"),soci::use(server,"s"),soci::use(ty,"t"),soci::use(wc,"c"),
            soci::use(observed,"now"),soci::use(allowed,"ok"),soci::use(size,"n");
        tx->commit();return true;
    } catch(...) {Stop();throw std::runtime_error("Native RPS batch failed; stopped without retry or success");}
}
}
