#include "postgresql_map_service.h"
#include <soci/soci.h>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace tmapsvr {
namespace {
std::uint8_t Byte(const soci::row& row,const char* name) {
    if(row.get_indicator(name)!=soci::i_ok)throw std::runtime_error("NULL source monster BYTE");
    const auto n=row.get<int>(name);
    if(n<0||n>255)throw std::runtime_error("Source monster BYTE range");
    return static_cast<std::uint8_t>(n);
}
std::uint16_t Word(const soci::row& row,const char* name) {
    if(row.get_indicator(name)!=soci::i_ok)throw std::runtime_error("NULL source monster WORD");
    const auto n=row.get<int>(name);
    if(n < -32768||n>32767)throw std::runtime_error("Source monster SMALLINT range");
    return std::bit_cast<std::uint16_t>(static_cast<std::int16_t>(n));
}
std::uint32_t Dword(const soci::row& row,const char* name) {
    if(row.get_indicator(name)!=soci::i_ok)throw std::runtime_error("NULL source monster DWORD");
    return std::bit_cast<std::uint32_t>(row.get<std::int32_t>(name));
}
float Real(const soci::row& row,const char* name) {
    if(row.get_indicator(name)!=soci::i_ok)throw std::runtime_error("NULL source spawn FLOAT");
    const auto n=static_cast<float>(row.get<double>(name));
    if(!std::isfinite(n))throw std::runtime_error("Non-finite source spawn coordinate");
    return n;
}
bool Hash(const std::string& s) {
    return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;
}
}
void PostgreSQLMapService::CheckNativeMonsterRelease(soci::session& sql) const {
    const int world=m_config.world,server=m_config.server;int count=0;
    sql<<"SELECT count(*) FROM monster_compat.catalog_release WHERE world_id=:w AND server_id=:s "
         "AND run_id=:run AND manifest_sha256=:m AND order_sha256=:o AND routing_manifest=:r AND character_manifest=:c",
        soci::use(world,"w"),soci::use(server,"s"),soci::use(m_monster_catalog.run_id,"run"),
        soci::use(m_monster_catalog.manifest,"m"),soci::use(m_monster_catalog.order_sha256,"o"),
        soci::use(m_config.routing_manifest,"r"),soci::use(m_config.character_manifest,"c"),soci::into(count);
    if(count!=1)throw std::runtime_error("Native Map source monster publication changed or unavailable");
    CheckNativeMonsterSkillRelease(sql);
    CheckNativeMonsterAiRelease(sql);
}
void PostgreSQLMapService::LoadNativeMonsterCatalog(soci::session& sql) {
    const int world=m_config.world,server=m_config.server;
    long long expected_spawns=0,expected_candidates=0,expected_parties=0;
    auto& catalog=m_monster_catalog;
    sql<<"SELECT run_id,manifest_sha256,order_sha256,spawn_rows,candidate_rows,party_rows "
         "FROM monster_compat.catalog_release WHERE world_id=:w AND server_id=:s "
         "AND routing_manifest=:r AND character_manifest=:c",
        soci::use(world,"w"),soci::use(server,"s"),soci::use(m_config.routing_manifest,"r"),
        soci::use(m_config.character_manifest,"c"),soci::into(catalog.run_id),soci::into(catalog.manifest),
        soci::into(catalog.order_sha256),soci::into(expected_spawns),soci::into(expected_candidates),soci::into(expected_parties);
    if(!sql.got_data()||catalog.run_id<=0||!Hash(catalog.manifest)||!Hash(catalog.order_sha256)||
       expected_spawns<0||expected_candidates<0||expected_parties<0)
        throw std::runtime_error("Native Map requires explicit source monster publication079");
    long long actual_spawns=0,actual_candidates=0,actual_parties=0;
    sql<<"SELECT spawn_rows,candidate_rows,party_rows FROM monster_compat.allocation_counts WHERE world_id=:w AND server_id=:s",
        soci::use(world,"w"),soci::use(server,"s"),soci::into(actual_spawns),soci::into(actual_candidates),soci::into(actual_parties);
    if(!sql.got_data()||actual_spawns!=expected_spawns||actual_candidates!=expected_candidates||actual_parties!=expected_parties)
        throw std::runtime_error("Source monster query-order publication omits real source rows");
    for(unsigned id=0x8000;id<0xffff;++id)catalog.extra_spawn_ids.insert(static_cast<std::uint16_t>(id));
    long long spawn_order=0;
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT * FROM monster_compat.\"TMONSPAWNCHART\" WHERE world_id=:w AND server_id=:s ORDER BY query_order",
        soci::use(world,"w"),soci::use(server,"s"));
     for(const auto& row:rows) {
        if(row.get<long long>("query_order")!=++spawn_order)throw std::runtime_error("Source spawn query order gap");
        SpawnPoint p;p.wID=Word(row,"wID");p.wGroup=Word(row,"wGroup");p.wLocalID=Word(row,"wLocalID");p.wMapID=Word(row,"wMapID");
        p.fPosX=Real(row,"fPosX");p.fPosY=Real(row,"fPosY");p.fPosZ=Real(row,"fPosZ");p.wDir=Word(row,"wDir");
        p.bCountry=Byte(row,"bCountry");p.bCount=Byte(row,"bCount");p.bRange=Byte(row,"bRange");p.bArea=Byte(row,"bArea");
        p.bLink=Byte(row,"bLink");p.bProb=Byte(row,"bProb");p.bRoamType=Byte(row,"bRoamType");p.bEvent=Byte(row,"bEvent");
        p.dwRegion=Dword(row,"dwRegion");p.dwDelay=Dword(row,"dwDelay");p.wPartyID=Word(row,"wPartyID");
        if(!catalog.spawns.emplace(p.wID,p).second)throw std::runtime_error("Duplicate source spawn template");
        if(p.wGroup)catalog.groups[p.wGroup].push_back(p.wID);
        if(p.wPartyID)catalog.parties.try_emplace(p.wID);
        catalog.candidates.try_emplace(p.wID); // actual zero-row query, not failed lookup
        catalog.extra_spawn_ids.erase(p.wID);
     }}
    if(spawn_order!=expected_spawns)throw std::runtime_error("Incomplete source spawn query result");
    long long candidate_count=0;
    {std::map<std::uint16_t,long long> order;
     soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT * FROM monster_compat.\"TMAPMONCHART\" WHERE world_id=:w AND server_id=:s ORDER BY query_order",
        soci::use(world,"w"),soci::use(server,"s"));
     for(const auto& row:rows) {
        MapMonEntry p;p.wSpawnID=Word(row,"wSpawnID");p.wMonID=Word(row,"wMonID");
        p.bEssential=Byte(row,"bEssential");p.bLeader=Byte(row,"bLeader");p.bProb=Byte(row,"bProb");
        if(!catalog.spawns.contains(p.wSpawnID)||row.get<long long>("query_order")!=++order[p.wSpawnID])
            throw std::runtime_error("Invalid source candidate query order/owner");
        catalog.candidates.at(p.wSpawnID).push_back(p);++candidate_count;
     }}
    long long party_count=0;
    {std::map<std::uint16_t,long long> order;
     soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT spawn_id,query_order,\"wID\" FROM monster_compat.\"TMONPARTY\" WHERE world_id=:w AND server_id=:s ORDER BY query_order",
        soci::use(world,"w"),soci::use(server,"s"));
     for(const auto& row:rows) {
        const auto n=row.get<int>("spawn_id");
        if(n<0||n>65535)throw std::runtime_error("Source party owner WORD range");
        const auto spawn=static_cast<std::uint16_t>(n);
        if(!catalog.parties.contains(spawn)||row.get<long long>("query_order")!=++order[spawn])
            throw std::runtime_error("Invalid source party query order/owner");
        // CTBLMonParty reads the full chart, including templates not owned
        // by this server. Do not prune missing local pointers here.
        catalog.parties.at(spawn).push_back(Word(row,"wID"));++party_count;
     }}
    if(candidate_count!=expected_candidates||party_count!=expected_parties)
        throw std::runtime_error("Incomplete source candidate/party queries");
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT \"wID\",\"wMonAttr\",\"bLevel\",\"bClass\",\"bRace\",\"wSkill1\",\"wSkill2\",\"wSkill3\",\"wSkill4\","
        "\"bAIType\",\"bCanAttack\",\"bIsSpecial\",\"bRemove\",\"fSize\" "
        "FROM monster_compat.\"TMONSTERCHART\" WHERE world_id=:w AND server_id=:s",
        soci::use(world,"w"),soci::use(server,"s"));
     for(const auto& row:rows) {
        NativeMonsterAllocationTemplate p;p.wID=Word(row,"wID");p.wMonAttr=Word(row,"wMonAttr");
        p.bLevel=Byte(row,"bLevel");p.bClass=Byte(row,"bClass");p.bRace=Byte(row,"bRace");
        p.skills={Word(row,"wSkill1"),Word(row,"wSkill2"),Word(row,"wSkill3"),Word(row,"wSkill4")};
        p.ai_type=Byte(row,"bAIType");p.can_attack=Byte(row,"bCanAttack");
        p.special=Byte(row,"bIsSpecial");p.remove=Byte(row,"bRemove");p.size=Real(row,"fSize");
        if(!catalog.monsters.emplace(p.wID,p).second)throw std::runtime_error("Duplicate source monster template");
     }}
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT \"wID\",\"bLevel\",\"dwMaxHP\",\"dwMaxMP\" FROM monster_compat.\"TMONATTRCHART\" WHERE world_id=:w AND server_id=:s",
        soci::use(world,"w"),soci::use(server,"s"));
     for(const auto& row:rows) {
        const auto key=std::uint32_t{Word(row,"wID")}|(std::uint32_t{Byte(row,"bLevel")}<<16);
        if(!catalog.attributes.emplace(key,NativeMonsterBaseVitals{Dword(row,"dwMaxHP"),Dword(row,"dwMaxMP")}).second)
            throw std::runtime_error("Duplicate source attribute FAMILY/level");
     }}
    const auto load_bytes=[&](const char* query,const char* column,auto& target) {
        soci::rowset<soci::row> rows=(sql.prepare<<query,soci::use(world,"w"),soci::use(server,"s"));
        for(const auto& row:rows)if(!target.insert(Byte(row,column)).second)
            throw std::runtime_error("Duplicate source allocation metadata key");
    };
    load_bytes("SELECT \"bLevel\" FROM monster_compat.\"TLEVELCHART\" WHERE world_id=:w AND server_id=:s","bLevel",catalog.levels);
    LoadNativeMonsterSkills(sql);
    {soci::rowset<soci::row> rows=(sql.prepare<<"SELECT \"wItemID\" FROM character_compat.\"TITEMCHART\"");
     for(const auto& row:rows)if(!catalog.items.insert(Word(row,"wItemID")).second)throw std::runtime_error("Duplicate item template");}
}
void PostgreSQLMapService::AllocateNativeMonsters() {
    if(m_monster_catalog.run_id!=m_battles.run_id||m_monster_catalog.manifest!=m_battles.manifest)
        throw std::runtime_error("Native battle and monster source releases differ");
    auto bootstrap=MakeNativeMonsterAiExecutor(0); // source LoadData precedes AIThread clock initialization
    AddNativeDefaultSpawns(m_monster_catalog,m_battles,m_monster_maps,m_monster_generation,
        [&](NativeMonsterMap& map,std::uint32_t id){bootstrap.Initialize(map,id);});
    // Source objects/jobs may now advance through the actual AI queues. Until
    // actor cell/host consumers exist, ALL player admission is blocked for this
    // process, including invisible/WAIT slots and later removal of every slot.
    m_native_ai_actor_barrier=m_monster_ai_queue.Pending()!=0;
    for(const auto& [key,map]:m_monster_maps)
        if(!map.slots.empty())m_native_ai_actor_barrier=true;
}
} // namespace tmapsvr
