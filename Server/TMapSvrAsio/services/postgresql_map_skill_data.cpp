#include "postgresql_map_service.h"
#include <soci/soci.h>
#include <bit>
#include <stdexcept>

namespace tmapsvr {
namespace {
std::uint8_t Byte(const soci::row& row,const char* field) {
    if(row.get_indicator(field)!=soci::i_ok)throw std::runtime_error("NULL native skill-data BYTE");
    const auto n=row.get<int>(field);
    if(n<0||n>255)throw std::runtime_error("Native skill-data BYTE range");
    return static_cast<std::uint8_t>(n);
}
std::uint16_t Word(const soci::row& row,const char* field) {
    if(row.get_indicator(field)!=soci::i_ok)throw std::runtime_error("NULL native skill-data WORD");
    const auto n=row.get<int>(field);
    if(n < -32768||n>32767)throw std::runtime_error("Native skill-data SMALLINT range");
    return std::bit_cast<std::uint16_t>(static_cast<std::int16_t>(n));
}
std::uint16_t QuerySkill(const soci::row& row) {
    if(row.get_indicator("skill_id")!=soci::i_ok)throw std::runtime_error("NULL native skill query identity");
    const auto n=row.get<int>("skill_id");
    if(n<0||n>65535)throw std::runtime_error("Native skill query identity range");
    return static_cast<std::uint16_t>(n);
}
}
void PostgreSQLMapService::CheckNativeSkillDataRelease(soci::session& sql) const {
    const int world=m_config.world,server=m_config.server;int count=0;
    sql<<"SELECT count(*) FROM monster_compat.skill_data_release WHERE world_id=:w AND server_id=:s "
         "AND run_id=:run AND manifest_sha256=:m AND allocation_order_sha256=:a AND character_manifest=:c AND order_sha256=:o",
        soci::use(world,"w"),soci::use(server,"s"),soci::use(m_monster_catalog.run_id,"run"),
        soci::use(m_monster_catalog.manifest,"m"),soci::use(m_monster_catalog.order_sha256,"a"),
        soci::use(m_config.character_manifest,"c"),soci::use(m_monster_catalog.skill_data_order_sha256,"o"),soci::into(count);
    if(count!=1)throw std::runtime_error("Native ordered skill-data publication083 changed or unavailable");
}
void PostgreSQLMapService::LoadNativeSkillData(soci::session& sql) {
    auto& c=m_monster_catalog;
    const int world=m_config.world,server=m_config.server;
    long long expected_queries=0,expected_data=0,expected_premium=0;
    sql<<"SELECT order_sha256,skill_queries,data_rows,premium_rows FROM monster_compat.skill_data_release "
         "WHERE world_id=:w AND server_id=:s AND run_id=:run AND manifest_sha256=:m "
         "AND allocation_order_sha256=:a AND character_manifest=:c",
        soci::use(world,"w"),soci::use(server,"s"),soci::use(c.run_id,"run"),soci::use(c.manifest,"m"),
        soci::use(c.order_sha256,"a"),soci::use(m_config.character_manifest,"c"),
        soci::into(c.skill_data_order_sha256),soci::into(expected_queries),soci::into(expected_data),soci::into(expected_premium);
    if(!sql.got_data()||expected_queries<0||expected_queries>65536||expected_data<0||expected_data>1'000'000||
       expected_premium<0||expected_premium>1'000'000||c.skill_data_order_sha256.size()!=64||
       c.skill_data_order_sha256.find_first_not_of("0123456789abcdef")!=std::string::npos)
        throw std::runtime_error("Native Map requires complete ordered skill-data publication083");
    std::map<std::uint16_t,long long> sizes,orders;
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT skill_id,data_rows FROM monster_compat.skill_data_queries WHERE world_id=:w AND server_id=:s ORDER BY skill_id",
        soci::use(world,"w"),soci::use(server,"s"));
     for(const auto& row:rows) {
        const auto skill=QuerySkill(row);
        if(row.get_indicator("data_rows")!=soci::i_ok)throw std::runtime_error("NULL skill query count");
        const auto size=row.get<long long>("data_rows");
        if(size<0||size>1'000'000||!c.skills.contains(skill)||!sizes.emplace(skill,size).second||
           !c.skill_data.emplace(skill,std::vector<SkillDataRow>{}).second)
            throw std::runtime_error("Unknown/duplicate native skill query or invalid size");
     }}
    if(static_cast<long long>(sizes.size())!=expected_queries||sizes.size()!=c.skills.size())
        throw std::runtime_error("Missing per-existing-skill query, including empty results");
    long long loaded=0;
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT skill_id,query_order,\"bAction\",\"bType\",\"bAttr\",\"bExec\",\"bInc\",\"wValue\",\"wValueInc\",\"bCalc\" "
        "FROM monster_compat.skill_data WHERE world_id=:w AND server_id=:s ORDER BY skill_id,query_order",
        soci::use(world,"w"),soci::use(server,"s"));
     for(const auto& row:rows) {
        const auto skill=QuerySkill(row);
        if(++loaded>expected_data||!sizes.contains(skill)||row.get_indicator("query_order")!=soci::i_ok||
           row.get<long long>("query_order")!=++orders[skill]||orders[skill]>sizes.at(skill))
            throw std::runtime_error("Native skill-data query order/count differs from publication");
        c.skill_data.at(skill).push_back({Byte(row,"bAction"),Byte(row,"bType"),Byte(row,"bAttr"),Byte(row,"bExec"),
            Byte(row,"bInc"),Word(row,"wValue"),Word(row,"wValueInc"),Byte(row,"bCalc")});
     }}
    if(loaded!=expected_data)throw std::runtime_error("Incomplete native skill data");
    for(const auto& [skill,size]:sizes)if(static_cast<long long>(c.skill_data.at(skill).size())!=size)
        throw std::runtime_error("Incomplete per-skill data result");
    // Ordinary non-BOW/non-BR LoadData removes ALL premium-chart templates
    // after it loaded their data. Repeated/missing IDs simply remain absent.
    long long premium=0;
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT \"bType\",\"wSkillID\",\"wMedals\" FROM monster_compat.premium_skill_removals WHERE world_id=:w AND server_id=:s",
        soci::use(world,"w"),soci::use(server,"s"));
     for(const auto& row:rows) {
        if(++premium>expected_premium)throw std::runtime_error("Native premium removal count differs");
        (void)Byte(row,"bType");(void)Word(row,"wMedals");const auto skill=Word(row,"wSkillID");
        c.skill_data.erase(skill);c.skills.erase(skill);
     }}
    if(premium!=expected_premium)throw std::runtime_error("Incomplete source premium skill removals");
    c.player_skill_data=std::make_shared<const SourceSkillDataCatalog>(c.skill_data);
    CheckNativeSkillDataRelease(sql);
}
} // namespace tmapsvr
