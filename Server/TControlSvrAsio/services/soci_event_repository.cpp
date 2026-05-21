#include "soci_event_repository.h"

#include "fourstory/db/orm/sp_call.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <ctime>

namespace tcontrolsvr {

namespace {

// SQL DATETIME → unix epoch seconds. SOCI's row::get<std::tm> path
// works across the ODBC + native backends.
std::int64_t TmToUnix(const std::tm& t)
{
    std::tm copy = t;
    return static_cast<std::int64_t>(std::mktime(&copy));
}

std::tm UnixToTm(std::int64_t unix_seconds)
{
    std::time_t t = static_cast<std::time_t>(unix_seconds);
    std::tm out{};
#ifdef _WIN32
    localtime_s(&out, &t);
#else
    localtime_r(&t, &out);
#endif
    return out;
}

} // namespace

SociEventRepository::SociEventRepository(fourstory::db::SessionPool& pool)
    : m_pool(pool)
{
}

std::vector<EventInfo>
SociEventRepository::LoadAll()
{
    std::vector<EventInfo> out;
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;
    try
    {
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT \"dwIndex\", \"bID\", \"szTitle\", \"bGroupID\", "
            "       \"bSvrType\", \"bSvrID\", \"dStartDate\", "
            "       \"dEndDate\", \"wValue\", \"wMapID\", "
            "       \"dwStartAlarm\", \"dwEndAlarm\", \"bPartTime\", "
            "       \"szStartMsg\", \"szEndMsg\", \"szValue\" "
            "FROM \"TEVENTCHART\"");
        for (const auto& r : rs)
        {
            EventInfo e{};
            e.index        = static_cast<std::uint32_t>(r.get<int>(0));
            e.kind         = static_cast<std::uint8_t>(r.get<int>(1));
            e.title        = r.get<std::string>(2);
            e.group_id     = static_cast<std::uint8_t>(r.get<int>(3));
            e.server_type  = static_cast<std::uint8_t>(r.get<int>(4));
            e.server_id    = static_cast<std::uint8_t>(r.get<int>(5));
            e.start_unix   = TmToUnix(r.get<std::tm>(6));
            e.end_unix     = TmToUnix(r.get<std::tm>(7));
            e.value        = static_cast<std::uint16_t>(r.get<int>(8));
            e.map_id       = static_cast<std::uint16_t>(r.get<int>(9));
            e.start_alarm  = static_cast<std::uint32_t>(r.get<int>(10));
            e.end_alarm    = static_cast<std::uint32_t>(r.get<int>(11));
            e.part_time    = static_cast<std::uint8_t>(r.get<int>(12));
            soci::indicator ind = r.get_indicator(13);
            if (ind != soci::i_null) e.start_msg = r.get<std::string>(13);
            ind = r.get_indicator(14);
            if (ind != soci::i_null) e.end_msg   = r.get<std::string>(14);
            // szValue intentionally not parsed back into cash_items /
            // mon_event / lottery vectors — legacy MakeStrValue /
            // ParseStrValue is lossy and the wire path re-supplies
            // the data when an operator edits a row.
            out.push_back(std::move(e));
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_event_repo.LoadAll DB error: {}", ex.what());
        throw;
    }
    return out;
}

std::vector<CashItem>
SociEventRepository::ListCashItems()
{
    std::vector<CashItem> out;
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;
    try
    {
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT \"wID\", \"szName\" "
            "FROM \"TCASHSHOPITEMCHART\" WHERE \"bCanSell\" = 1");
        for (const auto& r : rs)
        {
            CashItem ci{};
            ci.id   = static_cast<std::uint16_t>(r.get<int>(0));
            ci.name = r.get<std::string>(1);
            out.push_back(std::move(ci));
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_event_repo.ListCashItems DB error: {}", ex.what());
    }
    return out;
}

std::uint8_t
SociEventRepository::Persist(const EventInfo& ev,
                             std::uint8_t op,
                             const std::string& value_blob)
{
    using fourstory::db::orm::SpCall;
    try
    {
        auto lease = m_pool.Acquire();

        const std::tm start_tm = UnixToTm(ev.start_unix);
        const std::tm end_tm   = UnixToTm(ev.end_unix);

        auto r = SpCall("TEventUpdate")
            .In("dwIndex",      static_cast<int>(ev.index))
            .In("bID",          static_cast<int>(ev.kind))
            .In("bOp",          static_cast<int>(op))
            .In("szTitle",      ev.title)
            .In("bGroupID",     static_cast<int>(ev.group_id))
            .In("bSvrType",     static_cast<int>(ev.server_type))
            .In("bSvrID",       static_cast<int>(ev.server_id))
            .In("dStartDate",   start_tm)
            .In("dEndDate",     end_tm)
            .In("wValue",       static_cast<int>(ev.value))
            .In("wMapID",       static_cast<int>(ev.map_id))
            .In("dwStartAlarm", static_cast<int>(ev.start_alarm))
            .In("dwEndAlarm",   static_cast<int>(ev.end_alarm))
            .In("bPartTime",    static_cast<int>(ev.part_time))
            .In("szStartMsg",   ev.start_msg)
            .In("szMidMsg",     std::string{})  // legacy always empty
            .In("szEndMsg",     ev.end_msg)
            .In("szValue",      value_blob)
            .WithReturn()
            .Execute(*lease);

        if (!r.Ok()) return event_result::kFail;
        const auto nret = r.ReturnCode();
        if (nret < 0 || nret > 255) return 0xFF;
        return static_cast<std::uint8_t>(nret);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_event_repo.Persist({}) DB error: {}",
            ev.index, ex.what());
        return event_result::kFail;
    }
}

} // namespace tcontrolsvr
