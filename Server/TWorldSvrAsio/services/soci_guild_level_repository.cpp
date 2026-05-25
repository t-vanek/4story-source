#include "services/soci_guild_level_repository.h"

#include "fourstory/db/orm/db_context.h"
#include "fourstory/db/orm/entity_mapping.h"
#include "fourstory/db/orm/repository.h"

#include <soci/soci.h>

#include <spdlog/spdlog.h>

// EntityMapping for the immutable TGUILDCHART rows. The row struct
// (TGuildLevelRow) doubles as the domain type — there is no shape
// mismatch, so this is a pure ORM read with no Automapper hop: a single
// Repository<TGuildLevelRow>::All() replaces the hand-written rowset loop.
// The mapping lives here because LoadAll is its only consumer.
namespace fourstory::db::orm {

template<>
struct EntityMapping<tworldsvr::TGuildLevelRow>
{
    using T = tworldsvr::TGuildLevelRow;

    static constexpr const char* Table    = "TGUILDCHART";
    static constexpr const char* PkColumn = "bLevel";

    static T FromRow(const soci::row& r)
    {
        T row;
        row.level             = static_cast<std::uint8_t>(r.get<int>("bLevel"));
        row.exp               = static_cast<std::uint32_t>(r.get<int>("dwEXP"));
        row.max_count         = static_cast<std::uint8_t>(r.get<int>("bMaxCnt"));
        row.min_count         = static_cast<std::uint8_t>(r.get<int>("bMinCnt"));
        row.cabinet_count     = static_cast<std::uint8_t>(r.get<int>("bCabinetCnt"));
        row.tactics_count     = static_cast<std::uint8_t>(r.get<int>("bTacticsCnt"));
        row.battle_set_count  = static_cast<std::uint8_t>(r.get<int>("bBattleSetCnt"));
        row.guard_count       = static_cast<std::uint8_t>(r.get<int>("bGuardCnt"));
        row.royal_guard_count = static_cast<std::uint8_t>(r.get<int>("bRoyalGuardCnt"));
        row.turret_count      = static_cast<std::uint8_t>(r.get<int>("bTurretCnt"));
        row.peer_slots[0]     = static_cast<std::uint8_t>(r.get<int>("bPeer1"));
        row.peer_slots[1]     = static_cast<std::uint8_t>(r.get<int>("bPeer2"));
        row.peer_slots[2]     = static_cast<std::uint8_t>(r.get<int>("bPeer3"));
        row.peer_slots[3]     = static_cast<std::uint8_t>(r.get<int>("bPeer4"));
        row.peer_slots[4]     = static_cast<std::uint8_t>(r.get<int>("bPeer5"));
        return row;
    }

    static std::string SelectAllSql()
    {
        return "SELECT \"bLevel\", \"dwEXP\", \"bMaxCnt\", \"bMinCnt\", "
               "\"bCabinetCnt\", \"bTacticsCnt\", \"bBattleSetCnt\", "
               "\"bGuardCnt\", \"bRoyalGuardCnt\", \"bTurretCnt\", "
               "\"bPeer1\", \"bPeer2\", \"bPeer3\", \"bPeer4\", \"bPeer5\" "
               "FROM \"TGUILDCHART\"";
    }
    static std::string SelectByIdSql()
    {
        return SelectAllSql() + " WHERE \"bLevel\" = :pk";
    }
    static std::string DeleteSql()
    {
        return "DELETE FROM \"TGUILDCHART\" WHERE \"bLevel\" = :pk";
    }

    // Read-only chart — write path intentionally unimplemented.
    static std::string InsertSql() { return ""; }
    static std::string UpdateSql() { return ""; }
    static void BindInsert(soci::statement&, const T&) {}
    static void BindUpdate(soci::statement&, const T&) {}
    static int  GetPk(const T& r) { return r.level; }
};

} // namespace fourstory::db::orm

namespace tworldsvr {

std::vector<TGuildLevelRow> SociGuildLevelRepository::LoadAll()
{
    fourstory::db::orm::DbContext ctx(m_pool);
    auto rows = ctx.Set<TGuildLevelRow>().All();
    spdlog::info("SociGuildLevelRepository: loaded {} TGUILDCHART row(s)",
        rows.size());
    return rows;
}

} // namespace tworldsvr
