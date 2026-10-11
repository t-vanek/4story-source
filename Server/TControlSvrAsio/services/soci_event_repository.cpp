#include "soci_event_repository.h"

#include "fourstory/db/orm/sp_call.h"
#include "event_value.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <ctime>
#include <bit>

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

SociEventRepository::SociEventRepository(fourstory::db::SessionPool& pool, std::shared_ptr<native_control::Owner> owner)
    : m_owner(std::move(owner)), m_pool(pool)
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
        if(native_control::Enabled(m_pool)) {
            auto tx=native_control::Begin(sql,m_owner);
            soci::rowset<soci::row> rows=(sql.prepare <<
                "SELECT e.\"dwIndex\",e.\"bID\",encode(e.title_bytes,'hex'),e.\"bGroupID\",e.\"bSvrType\",e.\"bSvrID\","
                "extract(epoch FROM e.\"dStartDate\" AT TIME ZONE c.legacy_timezone)::bigint,"
                "extract(epoch FROM e.\"dEndDate\" AT TIME ZONE c.legacy_timezone)::bigint,e.\"wValue\",e.\"wMapID\","
                "e.\"dwStartAlarm\",e.\"dwEndAlarm\",e.\"bPartTime\",encode(e.start_bytes,'hex'),encode(e.end_bytes,'hex'),encode(e.value_bytes,'hex') "
                "FROM app_control.\"TEVENTCHART\" e CROSS JOIN app_control.runtime_contract c ORDER BY (e.\"dwIndex\"::bigint & 4294967295)");
            for(const auto& r:rows) {
                EventInfo e;e.index=static_cast<std::uint32_t>(r.get<int>(0));e.kind=static_cast<std::uint8_t>(r.get<int>(1));
                e.title=native_control::Bytes(r.get<std::string>(2));e.group_id=static_cast<std::uint8_t>(r.get<int>(3));
                e.server_type=static_cast<std::uint8_t>(r.get<int>(4));e.server_id=static_cast<std::uint8_t>(r.get<int>(5));
                e.start_unix=r.get<long long>(6);e.end_unix=r.get<long long>(7);e.value=static_cast<std::uint16_t>(r.get<int>(8));
                e.map_id=static_cast<std::uint16_t>(r.get<int>(9));e.start_alarm=static_cast<std::uint32_t>(r.get<int>(10));
                e.end_alarm=static_cast<std::uint32_t>(r.get<int>(11));e.part_time=static_cast<std::uint8_t>(r.get<int>(12));
                e.start_msg=native_control::Bytes(r.get<std::string>(13));e.end_msg=native_control::Bytes(r.get<std::string>(14));
                event_value::Decode(e,native_control::Bytes(r.get<std::string>(15)));out.push_back(std::move(e));
            }
            tx->commit();return out;
        }
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
            if(r.get_indicator(15)!=soci::i_null)event_value::Decode(e,r.get<std::string>(15));
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
        if(native_control::Enabled(m_pool)) {
            auto tx=native_control::Begin(sql,m_owner);
            soci::rowset<soci::row> rows=(sql.prepare <<
                "SELECT \"wID\",encode(name_bytes,'hex') FROM app_control.\"TCASHSHOPITEMCHART\" WHERE \"bCanSell\"=1 ORDER BY \"wID\"");
            for(const auto& r:rows)out.push_back({static_cast<std::uint16_t>(r.get<int>(0)),native_control::Bytes(r.get<std::string>(1))});
            tx->commit();return out;
        }
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
        if(native_control::Enabled(m_pool))throw std::runtime_error("Native Control cash catalog unavailable");
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
        if(native_control::Enabled(m_pool)) {
            if(m_commit_uncertain)throw std::runtime_error("Previous event commit outcome unknown; restart required");
            if(op>event_op::kUpdate||ev.start_msg.size()>1024||ev.end_msg.size()>1024||value_blob.size()>1024)
                throw std::runtime_error("Event exceeds source parameter contract");
            auto& sql=*lease;auto tx=native_control::Begin(sql,m_owner);
            // Serializes TEventUpdate's delete/replace, including absent keys.
            sql << "LOCK TABLE app_control.\"TEVENTCHART\" IN SHARE ROW EXCLUSIVE MODE";
            const int id=std::bit_cast<std::int32_t>(ev.index);
            sql << "DELETE FROM app_control.\"TEVENTCHART\" WHERE \"dwIndex\"=:id",soci::use(id);
            if(op>0) {
                const int kind=ev.kind,group=ev.group_id,type=ev.server_type,server=ev.server_id,
                    amount=std::bit_cast<std::int16_t>(ev.value),map=std::bit_cast<std::int16_t>(ev.map_id),
                    start_alarm=std::bit_cast<std::int32_t>(ev.start_alarm),end_alarm=std::bit_cast<std::int32_t>(ev.end_alarm),part=ev.part_time;
                const long long start=ev.start_unix,end=ev.end_unix;
                // Backup procedure accepts VARCHAR(50) even though the table
                // title is VARCHAR(256). Preserve its parameter truncation.
                const auto title=native_control::Hex(ev.title.substr(0,50)),first=native_control::Hex(ev.start_msg),
                    last=native_control::Hex(ev.end_msg),value=native_control::Hex(value_blob);
                sql << "INSERT INTO app_control.\"TEVENTCHART\"(\"dwIndex\",\"bID\",title_bytes,\"bGroupID\",\"bSvrType\",\"bSvrID\","
                       "\"dStartDate\",\"dEndDate\",\"wValue\",\"wMapID\",\"dwStartAlarm\",\"dwEndAlarm\",\"bPartTime\",start_bytes,mid_bytes,end_bytes,value_bytes) "
                       "SELECT :id,:kind,decode(:title,'hex'),:group,:type,:server,"
                       "date_trunc('minute',(to_timestamp(:start) AT TIME ZONE legacy_timezone)+interval '30 seconds'),"
                       "date_trunc('minute',(to_timestamp(:end) AT TIME ZONE legacy_timezone)+interval '30 seconds'),"
                       ":amount,:map,:sa,:ea,:part,decode(:first,'hex'),''::bytea,decode(:last,'hex'),decode(:value,'hex') FROM app_control.runtime_contract",
                    soci::use(id,"id"),soci::use(kind,"kind"),soci::use(title,"title"),soci::use(group,"group"),soci::use(type,"type"),
                    soci::use(server,"server"),soci::use(start,"start"),soci::use(end,"end"),soci::use(amount,"amount"),soci::use(map,"map"),
                    soci::use(start_alarm,"sa"),soci::use(end_alarm,"ea"),soci::use(part,"part"),soci::use(first,"first"),soci::use(last,"last"),soci::use(value,"value");
            }
            native_control::Commit(*tx,m_commit_uncertain,m_owner);
            return event_result::kSuccess;
        }

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
        if(native_control::Enabled(m_pool))throw std::runtime_error("Native Control event persistence unavailable");
        spdlog::error("soci_event_repo.Persist({}) DB error: {}",
            ev.index, ex.what());
        return event_result::kFail;
    }
}

} // namespace tcontrolsvr
