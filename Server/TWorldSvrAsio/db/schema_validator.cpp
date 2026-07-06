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
    if (!TableHasColumns(*lease, "TITEMCHART",
            {"wItemID","bInitState"}))
    {
        spdlog::warn("schema_validator (world): TITEMCHART missing "
                     "wItemID/bInitState — the operator item-state tool "
                     "(CT_ITEMSTATE_REQ, W6-36) will report 0 applied "
                     "rows per batch.");
    }
    // Warn-only stored-procedure probes. W6-37 persists cash-sale
    // campaigns via the TCashItemSale TGAME wrapper (hops into
    // TGLOBAL_GSP — the cross-DB target can't be validated from this
    // pool); W6-38 uses THelpMessage + TClearMapCurrentUser.
    auto routine_exists = [&lease](const char* name) -> bool
    {
        int hits = 0;
        try
        {
            *lease << "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ROUTINES "
                      "WHERE ROUTINE_NAME = '" + std::string(name) + "'",
                soci::into(hits);
        }
        catch (const std::exception& ex)
        {
            spdlog::debug("schema_validator (world): routine probe '{}' "
                          "skipped: {}", name, ex.what());
        }
        return hits > 0;
    };
    if (!routine_exists("TCashItemSale"))
    {
        spdlog::warn("schema_validator (world): TCashItemSale SP not "
                     "deployed — the cash-sale confirm barrier "
                     "(MW_CASHITEMSALE_ACK, W6-37) will fail the "
                     "persist and skip the stop broadcast.");
    }
    if (!routine_exists("THelpMessage"))
    {
        spdlog::warn("schema_validator (world): THelpMessage SP not "
                     "deployed — CT_HELPMESSAGE_REQ (W6-38) will "
                     "broadcast without persisting.");
    }
    for (const char* rn : {"TInitMonthRank", "TSaveMonthRank",
                           "TInitMonthPvPoint"})
    {
        if (!routine_exists(rn))
            spdlog::warn("schema_validator (world): {} SP not deployed "
                         "- the MonthRank rollover (SM_MONTHRANKSAVE, "
                         "W6-42) will abort its persist.", rn);
    }
    if (!routine_exists("TRPSGameRecord"))
    {
        spdlog::warn("schema_validator (world): TRPSGameRecord SP not "
                     "deployed - RPS win-ledger persistence (W6-49) "
                     "will log SOCI errors.");
    }
    if (!TableHasColumns(*lease, "TRPSGAMECHART",
            {"bType","bWinCount","bProb_Win","wWinKeep","wWinPeriod"}))
    {
        spdlog::warn("schema_validator (world): TRPSGAMECHART not "
                     "deployed - the RPS game config boots empty.");
    }
    for (const char* rn : {"TEventQuarterUpdate", "TGetItemName"})
    {
        if (!routine_exists(rn))
            spdlog::warn("schema_validator (world): {} SP not deployed "
                         "- the EVENTQUARTER operator tools (W6-46) "
                         "degrade.", rn);
    }
    if (!TableHasColumns(*lease, "TEVENTQUARTERCHART",
            {"wID","bDay","bHour","bMinute","wItemID1","bCount"}))
    {
        spdlog::warn("schema_validator (world): TEVENTQUARTERCHART not "
                     "deployed - CT_EVENTQUARTERLIST_REQ returns an "
                     "empty listing.");
    }
    for (const char* rn : {"TCMGiftCanTake", "TCMGiftAdd",
                           "TCMGiftSet", "TCMGiftDel"})
    {
        if (!routine_exists(rn))
            spdlog::warn("schema_validator (world): {} SP not deployed "
                         "- the CMGift family (W6-45) degrades (FAIL "
                         "results / skipped chart updates).", rn);
    }
    if (!TableHasColumns(*lease, "TCMGIFTCHART",
            {"wGiftID","bGiftType","dwValue","bCount","bTakeType",
             "bMaxTakeCount","bToolOnly","wErrGiftID"}))
    {
        spdlog::warn("schema_validator (world): TCMGIFTCHART not "
                     "deployed - the gift catalogue boots empty; "
                     "CT_CMGIFT_REQ replies CMGIFT_ID.");
    }
    if (!routine_exists("TSaveCastleApplicant"))
    {
        spdlog::warn("schema_validator (world): TSaveCastleApplicant SP "
                     "not deployed - castle-apply persistence (W6-43) "
                     "will log SOCI errors.");
    }
    if (!TableHasColumns(*lease, "TACTIVECHARTABLE",
            {"dwCharID","dateEnter"}))
    {
        spdlog::warn("schema_validator (world): TACTIVECHARTABLE not "
                     "deployed - the war-country index (W6-43) stays "
                     "empty; MW_WARCOUNTRYBALANCE replies report 0.");
    }
    if (!routine_exists("TClearMapCurrentUser"))
    {
        spdlog::warn("schema_validator (world): TClearMapCurrentUser SP "
                     "not deployed — SM_DELSESSION_REQ (W6-38) will "
                     "skip the current-user clear.");
    }
    // W6-50: tournament core. Charts feed the boot load; the SPs
    // back the operator tools (TET_SCHEDULEADD/DEL, TET_ENTRYADD,
    // TET_PLAYERADD/DEL persistence).
    if (!TableHasColumns(*lease, "TTOURNAMENTSCHEDULECHART",
            {"bGroup","bStep","dwPeriod"}))
    {
        spdlog::warn("schema_validator (world): TTOURNAMENTSCHEDULECHART "
                     "not deployed - the main tournament boots "
                     "unscheduled (W6-50).");
    }
    if (!TableHasColumns(*lease, "TTOURNAMENTCHART",
            {"bGroup","bEntryID","szName","bType","dwFee","bEnable"}))
    {
        spdlog::warn("schema_validator (world): TTOURNAMENTCHART not "
                     "deployed - the main tournament has no entries "
                     "(W6-50).");
    }
    if (!TableHasColumns(*lease, "TTNMTEVENTTIMETABLE",
            {"wTournamentID","bWeek","bDay","dwStart"}))
    {
        spdlog::warn("schema_validator (world): TTNMTEVENTTIMETABLE not "
                     "deployed - operator tournament events boot empty "
                     "(W6-50).");
    }
    for (const char* rn : {"TTnmtEventTime", "TTnmtEventSchedule",
                           "TTnmtEventDel", "TTnmtEventEntry",
                           "TTnmtEventReward", "TTournamentApply"})
    {
        if (!routine_exists(rn))
            spdlog::warn("schema_validator (world): {} SP not deployed "
                         "- the tournament operator tools (W6-50) "
                         "will log SOCI errors on persist.", rn);
    }
}

} // namespace tworldsvr::db
