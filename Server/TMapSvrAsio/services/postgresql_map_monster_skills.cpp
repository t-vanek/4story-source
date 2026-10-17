#include "postgresql_map_service.h"
#include <soci/soci.h>
#include <bit>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace tmapsvr {
namespace {
std::uint8_t Byte(const soci::row& r,const char* field) {
    if(r.get_indicator(field)!=soci::i_ok)throw std::runtime_error("NULL monster skill BYTE");
    const auto n=r.get<int>(field);
    if(n<0||n>255)throw std::runtime_error("Monster skill BYTE range");
    return static_cast<std::uint8_t>(n);
}
std::uint16_t Word(const soci::row& r,const char* field) {
    if(r.get_indicator(field)!=soci::i_ok)throw std::runtime_error("NULL monster skill WORD");
    const auto n=r.get<int>(field);
    if(n < -32768||n>32767)throw std::runtime_error("Monster skill SMALLINT range");
    return std::bit_cast<std::uint16_t>(static_cast<std::int16_t>(n));
}
std::uint32_t Dword(const soci::row& r,const char* field) {
    if(r.get_indicator(field)!=soci::i_ok)throw std::runtime_error("NULL monster skill DWORD");
    return std::bit_cast<std::uint32_t>(r.get<std::int32_t>(field));
}
float Real(const soci::row& r,const char* field) {
    if(r.get_indicator(field)!=soci::i_ok)throw std::runtime_error("NULL monster skill FLOAT");
    const auto n=static_cast<float>(r.get<double>(field));
    if(!std::isfinite(n))throw std::runtime_error("Nonfinite monster skill FLOAT");
    return n;
}
}
void PostgreSQLMapService::CheckNativeMonsterSkillRelease(soci::session& sql) const {
    const int world=m_config.world,server=m_config.server;int count=0;
    sql<<"SELECT count(*) FROM monster_compat.skill_release WHERE world_id=:w AND server_id=:s "
         "AND run_id=:run AND manifest_sha256=:m AND character_manifest=:c AND order_sha256=:o",
        soci::use(world,"w"),soci::use(server,"s"),soci::use(m_monster_catalog.run_id,"run"),
        soci::use(m_monster_catalog.manifest,"m"),soci::use(m_config.character_manifest,"c"),
        soci::use(m_monster_catalog.order_sha256,"o"),soci::into(count);
    if(count!=1)throw std::runtime_error("Native monster skill/formula source publication082 changed or unavailable");
}
void PostgreSQLMapService::LoadNativeMonsterSkills(soci::session& sql) {
    CheckNativeMonsterSkillRelease(sql);
    const int world=m_config.world,server=m_config.server;
    auto& c=m_monster_catalog;
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT \"bID\",\"dwinit\",\"fRateX\",\"fRateY\" FROM monster_compat.\"TFORMULACHART\" WHERE world_id=:w AND server_id=:s",
        soci::use(world,"w"),soci::use(server,"s"));
     for(const auto& r:rows)if(!c.formulas.emplace(Byte(r,"bID"),FormulaRow{Dword(r,"dwinit"),Real(r,"fRateX"),Real(r,"fRateY")}).second)
        throw std::runtime_error("Duplicate source monster formula");}
    // LoadData initializes f1stRateX to0 before the optional FTYPE_1ST stamp.
    const auto growth=c.formulas.find(FTYPE_1ST);
    const float rate=growth==c.formulas.end()?0.0f:growth->second.fRateX;
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT \"wID\",\"wMinRange\",\"wMaxRange\",\"wPrevActiveID\",\"dwReuseDelay\","
        "\"dwUseHP\",\"bUseHPType\",\"dwUseMP\",\"bUseMPType\",\"bLevel\",\"bNextLevel\","
        "\"nReuseDelayInc\",\"dwKindDelay\",\"bKind\",\"bSpeedApply\",\"bTargetRange\" "
        "FROM monster_compat.\"TSKILLCHART\" WHERE world_id=:w AND server_id=:s",
        soci::use(world,"w"),soci::use(server,"s"));
     for(const auto& r:rows) {
        SkillTemplate t;t.wID=Word(r,"wID");t.wMinRange=Word(r,"wMinRange");t.wMaxRange=Word(r,"wMaxRange");
        t.wPrevActiveID=Word(r,"wPrevActiveID");t.dwReuseDelay=Dword(r,"dwReuseDelay");
        t.dwUseHP=Dword(r,"dwUseHP");t.bUseHPType=Byte(r,"bUseHPType");
        t.dwUseMP=Dword(r,"dwUseMP");t.bUseMPType=Byte(r,"bUseMPType");
        t.bStartLevel=Byte(r,"bLevel");t.bNextLevel=Byte(r,"bNextLevel");t.f1stRateX=rate;
        t.nReuseDelayInc=std::bit_cast<std::int32_t>(Dword(r,"nReuseDelayInc"));t.dwKindDelay=Dword(r,"dwKindDelay");
        t.bKind=Byte(r,"bKind");t.bSpeedApply=Byte(r,"bSpeedApply");t.bTargetRange=Byte(r,"bTargetRange");
        if(!c.skills.emplace(t.wID,std::move(t)).second)throw std::runtime_error("Duplicate source monster skill template");
     }}
    const auto stats=[&](const char* query,const char* key,auto& target) {
        soci::rowset<soci::row> rows=(sql.prepare<<query,soci::use(world,"w"),soci::use(server,"s"));
        for(const auto& r:rows) {
            const StatBlock s{Word(r,"wSTR"),Word(r,"wDEX"),Word(r,"wCON"),Word(r,"wINT"),Word(r,"wWIS"),Word(r,"wMEN")};
            if(!target.emplace(Byte(r,key),s).second)throw std::runtime_error("Duplicate source monster class/race");
        }
    };
    stats("SELECT * FROM monster_compat.skill_class_stats WHERE world_id=:w AND server_id=:s","bClassID",c.classes);
    stats("SELECT * FROM monster_compat.skill_race_stats WHERE world_id=:w AND server_id=:s","bRaceID",c.races);
    LoadNativeSkillData(sql); // original per-skill data then ordinary premium-template removals, before Initialize
}
} // namespace tmapsvr
