// Boot-time schema validator for TWorldSvrAsio.
//
// W3a-1 ships TGUILDTABLE + TGUILDMEMBERTABLE — columns the
// SociGuildRepository::LoadAll / FindById queries read. Other
// world tables (TGUILDARTICLETABLE, TGUILDCABINETTABLE, TGUILDTACTICS*,
// TBOWPLAYERTABLE, TFRIENDTABLE, TSOULMATETABLE, …) join over
// subsequent W3a-N / W4 / W6 phases — each phase appends its own
// required block here. Optional tables warn rather than abort so
// dev DBs missing a recent migration still boot.

#include "schema_validator.h"

#include "fourstory/db/schema_validator.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <string>
#include <vector>

namespace tworldsvr::db {

namespace {

bool TableHasColumns(soci::session& sql,
                     const std::string& table,
                     const std::vector<std::string>& cols)
{
    if (cols.empty()) return false;
    std::string in_clause;
    for (std::size_t i = 0; i < cols.size(); ++i)
    {
        if (i) in_clause += ",";
        in_clause += "'";
        in_clause += cols[i];
        in_clause += "'";
    }
    int hits = 0;
    try
    {
        sql << "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
               "WHERE TABLE_NAME = '" + table + "' "
               "AND COLUMN_NAME IN (" + in_clause + ")",
            soci::into(hits);
    }
    catch (const std::exception& ex)
    {
        spdlog::debug("schema_validator (world): probe '{}' skipped: {}",
            table, ex.what());
        return false;
    }
    return hits == static_cast<int>(cols.size());
}

} // namespace

void ValidateWorldSchema(fourstory::db::SessionPool& pool)
{
    auto lease = pool.Acquire();

    // Required for W3a-1: the columns SociGuildRepository reads.
    fourstory::db::CheckColumns(*lease, "world", {
        { "TGUILDTABLE",       "dwID" },
        { "TGUILDTABLE",       "szName" },
        { "TGUILDTABLE",       "dwChief" },
        { "TGUILDTABLE",       "bLevel" },
        { "TGUILDTABLE",       "dwFame" },
        { "TGUILDTABLE",       "dwFameColor" },
        { "TGUILDTABLE",       "bMaxCabinet" },
        { "TGUILDTABLE",       "dwGold" },
        { "TGUILDTABLE",       "dwSilver" },
        { "TGUILDTABLE",       "dwCooper" },
        { "TGUILDTABLE",       "dwGI" },
        { "TGUILDTABLE",       "dwExp" },
        { "TGUILDTABLE",       "bGPoint" },
        { "TGUILDTABLE",       "bStatus" },
        { "TGUILDTABLE",       "bDisorg" },
        { "TGUILDTABLE",       "dwTime" },
        { "TGUILDTABLE",       "timeEstablish" },
        { "TGUILDTABLE",       "dwPvPTotalPoint" },
        { "TGUILDTABLE",       "dwPvPUseablePoint" },
        { "TGUILDMEMBERTABLE", "dwCharID" },
        { "TGUILDMEMBERTABLE", "dwGuildID" },
        { "TGUILDMEMBERTABLE", "bDuty" },
        { "TGUILDMEMBERTABLE", "bPeer" },
        { "TGUILDMEMBERTABLE", "dwService" },
    });

    // W3a-4d additions: TGUILDCHART (guild-level cap table).
    // Loaded once at boot into GuildLevelCache; handlers consult
    // it for per-level member / cabinet / peerage limits. Missing
    // table = empty cache → CheckPeerage falls back to "always
    // allow" (legacy refuses without the chart; we relax for dev
    // setups so a missing migration doesn't brick the binary).
    fourstory::db::CheckColumns(*lease, "world", {
        { "TGUILDCHART", "bLevel" },
        { "TGUILDCHART", "dwEXP" },
        { "TGUILDCHART", "bMaxCnt" },
        { "TGUILDCHART", "bCabinetCnt" },
        { "TGUILDCHART", "bPeer1" },
        { "TGUILDCHART", "bPeer5" },
    });

    // Optional — these power handler families that ship later
    // (W3a-2 articles / cabinet, W3a-3 tactics, W5 castle, …).
    // Missing them just downgrades those handlers to no-ops; the
    // W3a-1 char + guild flows still work.
    if (!TableHasColumns(*lease, "TGUILDARTICLETABLE",
            {"dwGuildID","dwIndex","szTitle","szBody","timeWrite"}))
    {
        spdlog::warn("schema_validator (world): TGUILDARTICLETABLE not "
                     "deployed — guild-board handlers (W3a-2) will return "
                     "empty until the table is added.");
    }
    if (!TableHasColumns(*lease, "TGUILDCABINETTABLE",
            {"dwGuildID","dwItemID","wItemKind"}))
    {
        spdlog::warn("schema_validator (world): TGUILDCABINETTABLE not "
                     "deployed — guild-storage handlers (W3a-2) will be "
                     "stubbed.");
    }
    if (!TableHasColumns(*lease, "TGUILDTACTICSTABLE",
            {"dwGuildID","dwCharID","bRole"}))
    {
        spdlog::warn("schema_validator (world): TGUILDTACTICSTABLE not "
                     "deployed — tactics-alliance handlers (W3a-3) will "
                     "be stubbed.");
    }
    if (!TableHasColumns(*lease, "TGUILDPVPOINTREWARDTABLE",
            {"dwGuildID","szName","dwPoint","dlDate"}))
    {
        spdlog::warn("schema_validator (world): TGUILDPVPOINTREWARDTABLE "
                     "not deployed — DM_GUILDPOINTREWARD_REQ (W3a-14) "
                     "will log a SOCI error and drop the row; the "
                     "TGUILDTABLE running totals still get updated.");
    }
    if (!TableHasColumns(*lease, "TGUILDPVPRECORDTABLE",
            {"dwGuildID","dwCharID","dwDate","wKillCount","wDieCount",
             "dwPoint_1","dwPoint_8"}))
    {
        spdlog::warn("schema_validator (world): TGUILDPVPRECORDTABLE not "
                     "deployed — DM_PVPRECORD_REQ (W3a-21) will log a "
                     "SOCI error per row and drop the batch; no in-memory "
                     "fallback (audit-log only).");
    }
}

} // namespace tworldsvr::db
