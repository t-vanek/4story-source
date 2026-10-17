#include "postgresql_map_service.h"
#include <soci/soci.h>
#include <bit>
#include <utility>

namespace tmapsvr {
namespace {
std::uint8_t Byte(const soci::row& row,const char* column) {
    if(row.get_indicator(column)!=soci::i_ok)throw std::runtime_error("NULL source AI BYTE");
    const auto value=row.get<int>(column);
    if(value<0||value>255)throw std::runtime_error("Source AI BYTE range");
    return static_cast<std::uint8_t>(value);
}
std::uint32_t Dword(const soci::row& row,const char* column) {
    if(row.get_indicator(column)!=soci::i_ok)throw std::runtime_error("NULL source AI DWORD");
    return std::bit_cast<std::uint32_t>(row.get<std::int32_t>(column));
}
}
void PostgreSQLMapService::CheckNativeMonsterAiRelease(soci::session& sql) const {
    const int world=m_config.world,server=m_config.server;int count=0;
    sql<<"SELECT count(*) FROM monster_compat.ai_release WHERE world_id=:w AND server_id=:s AND run_id=:run "
         "AND manifest_sha256=:m AND allocation_order_sha256=:a AND order_sha256=:o",
        soci::use(world,"w"),soci::use(server,"s"),soci::use(m_monster_catalog.run_id,"run"),
        soci::use(m_monster_catalog.manifest,"m"),soci::use(m_monster_catalog.order_sha256,"a"),
        soci::use(m_monster_ai.order_sha256,"o"),soci::into(count);
    if(count!=1)throw std::runtime_error("Native Map source AI publication changed or unavailable");
}
void PostgreSQLMapService::LoadNativeMonsterAi(soci::session& sql) {
    const int world=m_config.world,server=m_config.server;
    long long commands=0,conditions=0,bindings=0;
    sql<<"SELECT order_sha256,command_rows,condition_rows,binding_rows FROM monster_compat.ai_release "
         "WHERE world_id=:w AND server_id=:s AND run_id=:run AND manifest_sha256=:m AND allocation_order_sha256=:a",
        soci::use(world,"w"),soci::use(server,"s"),soci::use(m_monster_catalog.run_id,"run"),
        soci::use(m_monster_catalog.manifest,"m"),soci::use(m_monster_catalog.order_sha256,"a"),
        soci::into(m_monster_ai.order_sha256),soci::into(commands),soci::into(conditions),soci::into(bindings);
    if(!sql.got_data()||commands<0||conditions<0||bindings<0||m_monster_ai.order_sha256.size()!=64||
       m_monster_ai.order_sha256.find_first_not_of("0123456789abcdef")!=std::string::npos)
        throw std::runtime_error("Native Map requires explicit full source AI publication080");
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT \"dwCmdID\",\"bCmdType\" FROM monster_compat.\"TAICMDCHART\" WHERE world_id=:w AND server_id=:s",
        soci::use(world,"w"),soci::use(server,"s"));
     for(const auto& row:rows) {
        NativeAiCommand command;command.id=Dword(row,"dwCmdID");command.kind=Byte(row,"bCmdType");
        if(!m_monster_ai.commands.emplace(command.id,std::move(command)).second)
            throw std::runtime_error("Duplicate source AI command identity");
     }}
    long long condition_count=0;
    {std::map<std::uint32_t,long long> order;
     soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT \"dwCmdID\",\"dwConditionID\",\"bConditionType\",query_order "
        "FROM monster_compat.\"TAICONCHART\" WHERE world_id=:w AND server_id=:s ORDER BY query_order",
        soci::use(world,"w"),soci::use(server,"s"));
     for(const auto& row:rows) {
        const auto id=Dword(row,"dwCmdID");
        if(!m_monster_ai.commands.contains(id)||row.get<long long>("query_order")!=++order[id])
            throw std::runtime_error("Invalid source AI condition owner/order");
        m_monster_ai.commands.at(id).conditions.push_back({Dword(row,"dwConditionID"),Byte(row,"bConditionType")});
        ++condition_count;
     }}
    long long binding_count=0;
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT \"_source_row_number\",query_order,\"dwCmdID\",\"dwDelay\",\"dwTriggerID\","
        "\"bLoop\",\"bTriggerType\",\"bAIType\" FROM monster_compat.\"TAICHART\" "
        "WHERE world_id=:w AND server_id=:s ORDER BY query_order",soci::use(world,"w"),soci::use(server,"s"));
     for(const auto& row:rows) {
        const auto trigger=Byte(row,"bTriggerType"),type=Byte(row,"bAIType");
        if(trigger>=14||row.get<long long>("query_order")!=++binding_count)
            throw std::runtime_error("Invalid source AI trigger/order");
        NativeAiBinding binding;const auto ordinal=row.get<long long>("_source_row_number");
        if(ordinal<=0)throw std::runtime_error("Missing actual source AI row identity");
        binding.source_row=static_cast<std::uint64_t>(ordinal);binding.command=Dword(row,"dwCmdID");
        binding.delay=Dword(row,"dwDelay");binding.loop=Byte(row,"bLoop");
        if(!m_monster_ai.commands.contains(binding.command))throw std::runtime_error("Source AI binding command template absent");
        if(!m_monster_ai.by_source_row.emplace(binding.source_row,binding).second)
            throw std::runtime_error("Duplicate source AI command-instance identity");
        m_monster_ai.types.insert(type);
        m_monster_ai.bindings[{type,trigger,Dword(row,"dwTriggerID")}].push_back(binding);
     }}
    if(commands!=static_cast<long long>(m_monster_ai.commands.size())||conditions!=condition_count||bindings!=binding_count)
        throw std::runtime_error("Incomplete source AI publication");
    CheckNativeMonsterAiRelease(sql);
}
} // namespace tmapsvr
