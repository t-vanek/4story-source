#include "postgresql_map_service.h"
#include "postgresql_map_owner.h"
#include "native_wire_text.h"
#include <soci/soci.h>
#include <bit>
#include <stdexcept>

namespace tmapsvr {
namespace {
std::uint8_t Byte(const soci::row& r,const char* name) {
    if(r.get_indicator(name)!=soci::i_ok)throw std::runtime_error("NULL source battle BYTE");
    const auto n=r.get<int>(name);
    if(n<0||n>255)throw std::runtime_error("Source battle BYTE range");
    return static_cast<std::uint8_t>(n);
}
std::uint16_t Word(const soci::row& r,const char* name) {
    if(r.get_indicator(name)!=soci::i_ok)throw std::runtime_error("NULL source battle WORD");
    const auto n=r.get<int>(name);
    if(n < -32768||n>32767)throw std::runtime_error("Source battle SMALLINT range");
    return std::bit_cast<std::uint16_t>(static_cast<std::int16_t>(n));
}
std::uint32_t Dword(const soci::row& r,const char* name) {
    if(r.get_indicator(name)!=soci::i_ok)throw std::runtime_error("NULL source battle DWORD");
    return std::bit_cast<std::uint32_t>(r.get<std::int32_t>(name));
}
std::uint32_t Unsigned(const soci::row& r,const char* name,std::uint32_t maximum) {
    if(r.get_indicator(name)!=soci::i_ok)throw std::runtime_error("NULL native battle identity");
    const auto n=r.get<long long>(name);
    if(n<0||static_cast<unsigned long long>(n)>maximum)throw std::runtime_error("Native battle identity range");
    return static_cast<std::uint32_t>(n);
}
std::string Text(const soci::row& r,const char* name) {
    if(r.get_indicator(name)!=soci::i_ok)throw std::runtime_error("Missing original battle text bytes");
    auto text=native_wire_text::Unhex(r.get<std::string>(name));
    if(text.size()>50||text.find('\0')!=std::string::npos)throw std::runtime_error("Source battle MAX_NAME/NUL");
    return text;
}
}
void PostgreSQLMapService::CheckNativeBattleRelease(soci::session& sql) const {
    if(!m_battle_current)throw std::runtime_error("Native battle-region state retired; process restart required");
    const int world=m_config.world;int count=0;
    sql<<"SELECT count(*) FROM battle_compat.catalog_release WHERE world_id=:w AND run_id=:run "
         "AND manifest_sha256=:m AND routing_manifest=:r AND character_manifest=:c AND content_manifest=:g "
         "AND order_sha256=:o AND source_timezone=:z",
        soci::use(world,"w"),soci::use(m_battles.run_id,"run"),soci::use(m_battles.manifest,"m"),
        soci::use(m_config.routing_manifest,"r"),soci::use(m_config.character_manifest,"c"),
        soci::use(m_battles.content_manifest,"g"),soci::use(m_battles.order_sha256,"o"),
        soci::use(m_battles.source_timezone,"z"),soci::into(count);
    if(count!=1)throw std::runtime_error("Native Map battle publication changed or unavailable");
    CheckNativeMonsterRelease(sql);
}
void PostgreSQLMapService::LoadNativeBattleRegions(soci::session& sql) {
    const int world=m_config.world,server=m_config.server;
    long long expected_regions=0,expected_days=0;
    sql<<"SELECT run_id,manifest_sha256,order_sha256,content_manifest,source_timezone,region_rows,occupation_rows "
         "FROM battle_compat.catalog_release WHERE world_id=:w AND routing_manifest=:r AND character_manifest=:c",
        soci::use(world,"w"),soci::use(m_config.routing_manifest,"r"),soci::use(m_config.character_manifest,"c"),
        soci::into(m_battles.run_id),soci::into(m_battles.manifest),soci::into(m_battles.order_sha256),
        soci::into(m_battles.content_manifest),soci::into(m_battles.source_timezone),soci::into(expected_regions),soci::into(expected_days);
    const auto hash=[](const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;};
    if(!sql.got_data()||!hash(m_battles.manifest)||!hash(m_battles.order_sha256)||!hash(m_battles.content_manifest)||m_battles.source_timezone.empty())
        throw std::runtime_error("Native Map requires explicit full battle-region publication078");
    CheckNativeBattleRelease(sql);
    // The existing native owner already validated the source server-only
    // projection and single mapped source group. Build owned map environments
    // from the same physical rows. DEFAULT_CHANNEL1 is a LOGICAL channel.
    const auto topology=fourstory::db::ReadNativeMapTopology(sql,m_config.world,m_config.server,m_config.routing_manifest);
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT r.\"wMapID\",r.\"wUnitID\",r.\"bChannel\",EXISTS(SELECT 1 FROM battle_compat.\"TINDUNCHART\" i "
        "WHERE i.world_id=:w AND i.\"wMapID\"=r.\"wMapID\")::integer AS instance_template "
        "FROM route_compat.source_map_regions r WHERE r.\"bServerID\"=:s",
        soci::use(world,"w"),soci::use(server,"s"));
     for(const auto& r:rows) {
        const auto key=std::uint32_t{Word(r,"wMapID")}|(std::uint32_t{Byte(r,"bChannel")}<<16);
        m_battles.maps[key].units.insert(Word(r,"wUnitID"));
        m_battles.maps[key].instance_template=r.get<int>("instance_template")!=0;
     }}
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT z.*,EXISTS(SELECT 1 FROM character_compat.\"TITEMCHART\" i WHERE i.\"wItemID\"=z.\"wNormalItem\")::integer AS normal_present,"
        "EXISTS(SELECT 1 FROM character_compat.\"TITEMCHART\" i WHERE i.\"wItemID\"=z.\"wChiefItem\")::integer AS chief_present "
        "FROM battle_compat.\"TBATTLEZONECHART\" z WHERE z.world_id=:w",soci::use(world,"w"));
     for(const auto& r:rows) {
        BattleZone z;z.id=Word(r,"wID");z.map=Word(r,"wMapID");z.castle=Word(r,"wCastle");
        z.name=Text(r,"name_bytes");z.boss=Word(r,"wBossSpawnID");
        z.left_keeper=Word(r,"wLGateKeeperSpawnID");z.right_keeper=Word(r,"wRGateKeeperSpawnID");z.center_keeper=Word(r,"wCGateKeeperSpawnID");
        z.left_switch=Dword(r,"dwLSwitchID");z.right_switch=Dword(r,"dwRSwitchID");z.center_switch=Dword(r,"dwCSwitchID");
        z.valorian_defend=Word(r,"wValorianBossDefend");z.valorian_attack=Word(r,"wValorianBossAttack");
        z.derion_defend=Word(r,"wDerionBossDefend");z.derion_attack=Word(r,"wDerionBossAttack");
        z.middle_spawn=Word(r,"wMiddleSpawnID");z.right_spawn=Word(r,"wRightSpawnID");z.left_spawn=Word(r,"wLeftSpawnID");
        z.line=Byte(r,"bLine");z.skill1=Word(r,"wSkill1");z.skill2=Word(r,"wSkill2");z.item_level=Byte(r,"bItemLevel");
        if(r.get<int>("normal_present"))z.normal_item=Word(r,"wNormalItem");
        if(r.get<int>("chief_present"))z.chief_item=Word(r,"wChiefItem");
        if(!m_battles.zones.emplace(z.id,std::move(z)).second)throw std::runtime_error("Duplicate source battle zone");
     }}
    // Source iterates std::map<WORD,spawn*> here, regardless of SQL load order.
    for(const auto& [id,spawn]:m_monster_catalog.spawns)
        if(auto z=m_battles.zones.find(spawn.wLocalID);z!=m_battles.zones.end())z->second.spawns.push_back(id);
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT local_id::bigint,local_type,country,source_guild_id,encode(guild_name_bytes,'hex') AS guild_name,"
        "encode(hero_bytes,'hex') AS hero,hero_second,next_defend_second,occupied_second "
        "FROM app_world.battle_regions WHERE world_id=:w AND source_run_id=:run ORDER BY local_type,boot_order",
        soci::use(world,"w"),soci::use(m_battles.run_id,"run"));
     for(const auto& r:rows) {
        BattleRegion local;local.id=static_cast<std::uint16_t>(Unsigned(r,"local_id",65535));
        const auto kind=Byte(r,"local_type");if(kind<1||kind>4)throw std::runtime_error("Unknown native LOCAL_TYPE");
        local.type=static_cast<LocalType>(kind);local.country=Byte(r,"country");local.source_guild=Unsigned(r,"source_guild_id",0xffffffffU);
        if(r.get_indicator("guild_name")!=soci::i_null)local.guild_name=Text(r,"guild_name");
        local.hero=Text(r,"hero");local.hero_second=r.get<long long>("hero_second");local.next_defend_second=r.get<long long>("next_defend_second");
        if(r.get_indicator("occupied_second")!=soci::i_null)local.occupied_second=r.get<long long>("occupied_second");
        const auto zone=m_battles.zones.find(local.id);
        if(zone==m_battles.zones.end())throw std::runtime_error("Native region lost its source battle zone");
        local.can_battle=local.type!=LocalType::Castle;
        if(local.type==LocalType::Occupation) {
            local.valid=m_monster_catalog.spawns.contains(zone->second.boss);local.occupied=false;local.gate_opened=false;local.last_occupy_type=0;
            for(auto& day:local.occupation_days)day=OccupationDay{};
        }else {
            local.attack_guild=local.defend_guild=0;local.defend_point=local.contribution_point=0;
            local.defend_count=local.attack_count=0;local.defend_country=3; // TCONTRY_N
            local.camp_owners=local.type==LocalType::SkyGarden?std::array<std::uint8_t,3>{1,1,1}:std::array<std::uint8_t,3>{0,0,0};
            if(local.type!=LocalType::Castle)local.occupied=false;
            if(local.type!=LocalType::Mission)local.last_occupy_type=0; // OCCUPY_DEFEND
            // InitEnvironment overrides initial validity against real owned
            // DEFAULT_CHANNEL logical route, then the physical map. Fresh
            // instance templates have no party0 instance and FindTMap returns
            // NULL. Physical channel1 presence alone is not this proof.
            const auto route=topology.logical_channels.find(std::uint32_t{zone->second.map}|(1U<<16));
            auto map=route==topology.logical_channels.end()?m_battles.maps.end():
                m_battles.maps.find(std::uint32_t{zone->second.map}|(std::uint32_t{route->second}<<16));
            local.valid=map!=m_battles.maps.end()&&!map->second.instance_template;
            if(local.valid&&local.type==LocalType::Castle) {
                map->second.castle=local.id;
            }
            if(local.valid&&local.type==LocalType::SkyGarden) {
                map->second.skygarden=local.id;
            }
        }
        m_battles.order.at(kind-1).push_back(local.id);
        if(!m_battles.locals.emplace(local.id,std::move(local)).second)throw std::runtime_error("Source cross-type local key collision");
     }}
    for(const auto id:m_battles.order.at(0)) {
        const auto castle_id=m_battles.zones.at(id).castle;
        auto castle=m_battles.locals.find(castle_id);
        if(castle!=m_battles.locals.end()&&castle->second.type==LocalType::Castle)castle->second.locals.insert(id);
    }
    std::size_t loaded_days=0;
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT local_id::bigint,day,source_guild_id,occupy_type FROM app_world.battle_occupation_days WHERE world_id=:w",soci::use(world,"w"));
     for(const auto& r:rows) {
        const auto id=static_cast<std::uint16_t>(Unsigned(r,"local_id",65535));const auto day=Byte(r,"day");
        if(day<1||day>7)throw std::runtime_error("Source occupation weekday out of bounds");
        m_battles.locals.at(id).occupation_days.at(day-1)=OccupationDay{Unsigned(r,"source_guild_id",0xffffffffU),Byte(r,"occupy_type")};
        ++loaded_days;
     }}
    if(expected_regions<0||expected_days<0||static_cast<std::size_t>(expected_regions)!=m_battles.locals.size()||
       static_cast<std::size_t>(expected_days)!=loaded_days)throw std::runtime_error("Native battle boot projection is incomplete");
    (void)m_battles.HeroList(); // validate the actual source wire bounds before listening
}
bool PostgreSQLMapService::NativeNpcCanTalk(soci::session& sql,const CharSnapshot& state,
    std::uint8_t country,std::uint16_t local) const {
    CheckNativeBattleRelease(sql);
    if(!state.payload||!state.payload->effects)throw std::runtime_error("NPC interaction requires actual disguise state");
    int disguise=0;
    for(const auto& effect:*state.payload->effects) {
        const int skill=std::bit_cast<std::int16_t>(effect.skill);
        int present=0;
        sql<<"SELECT 1 FROM character_compat.\"TSKILLCHART\" WHERE \"wID\"=:s",soci::use(skill,"s"),soci::into(present);
        if(!sql.got_data())throw std::runtime_error("Actual maintained NPC disguise has no source skill template");
        soci::rowset<int> rows=(sql.prepare<<"SELECT DISTINCT \"bExec\" FROM character_compat.\"TSKILLDATA\" "
            "WHERE \"wSkillID\"=:s AND \"bType\"=3 AND \"bExec\" BETWEEN 4 AND 6",soci::use(skill,"s"));
        for(const auto exec:rows) {
            if(disguise&&disguise!=exec)throw std::runtime_error("Ambiguous source disguise-data order requires original query evidence");
            disguise=exec;
        }
        // HaveDisguiseBuff traverses the actual maintained vector. Its inner
        // CTBLSkillData query has no ORDER BY: different disguise executors
        // within one template cannot be resolved by sorting by numeric ID.
        if(disguise)break;
    }
    return m_battles.CanTalk(state.bCountry,state.payload->aid_country,static_cast<std::uint8_t>(disguise),country,local);
}
std::vector<std::byte> PostgreSQLMapService::BattleHeroList(const MapSessionClaim& claim) {
    auto lease=m_pool.Acquire();auto& sql=*lease;
    auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token,true);
    if(!LockAccount(sql,claim))throw std::runtime_error("HEROLIST account authority unavailable");
    std::string phase;
    if(!LockClaim(sql,claim,phase)||phase!="ready")throw std::runtime_error("HEROLIST current primary authority unavailable");
    CheckCatalogs(sql);CheckNativeBattleRelease(sql);
    auto packet=m_battles.HeroList();
    if(!m_battle_current)throw std::runtime_error("HEROLIST boot authority retired while awaiting database");
    tx->commit();return packet;
}
} // namespace tmapsvr
