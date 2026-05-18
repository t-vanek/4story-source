#include "log_sink.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <sstream>
#include <utility>

namespace tlogsvr {

SociLogSink::SociLogSink(fourstory::db::SessionPool& pool,
                         std::string target_table)
    : m_pool(pool)
    , m_table(std::move(target_table))
{
}

void SociLogSink::Write(const LogRecord& rec)
{
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;
    try
    {
        // Build INSERT — column list is the modern TLOG_AUDIT schema
        // (see schema/tlog-audit.sql). Table name is interpolated as
        // a constant from config (no user input), parameters all
        // bound through SOCI.
        const std::string q =
            "INSERT INTO \"" + m_table + "\" "
            "(LT_LOGDATE, LT_SERVERID, LT_CLIENTIP, LT_ACTION, LT_MAPID, "
            " LT_X, LT_Y, LT_Z, "
            " LT_DWKEY1, LT_DWKEY2, LT_DWKEY3, LT_DWKEY4, LT_DWKEY5, "
            " LT_DWKEY6, LT_DWKEY7, LT_DWKEY8, LT_DWKEY9, LT_DWKEY10, LT_DWKEY11, "
            " LT_KEY1, LT_KEY2, LT_KEY3, LT_KEY4, LT_KEY5, LT_KEY6, LT_KEY7, "
            " LT_FMT, LT_LOG) "
            "VALUES (:ts, :sid, :ip, :act, :mid, :x, :y, :z, "
            " :k1, :k2, :k3, :k4, :k5, :k6, :k7, :k8, :k9, :k10, :k11, "
            " :s1, :s2, :s3, :s4, :s5, :s6, :s7, :fmt, "
            // ODBC binds std::string as VARCHAR; LT_LOG is VARBINARY.
            // CONVERT lets MSSQL do the typed cast server-side.
            " CONVERT(VARBINARY(512), :blob))";

        const int sid = static_cast<int>(rec.server_id);
        const int act = static_cast<int>(rec.action);
        const int mid = static_cast<int>(rec.map_id);
        const int fmt = static_cast<int>(rec.format);
        // payload as std::string for SOCI VARBINARY binding (binary-safe
        // because std::string stores arbitrary bytes; the driver maps
        // to VARBINARY column type on MSSQL via the column DDL).
        // Empty payload binds as SQL NULL — MSSQL rejects empty-string
        // → VARBINARY implicit conversion otherwise.
        std::string blob;
        soci::indicator blob_ind = soci::i_null;
        if (!rec.payload.empty())
        {
            blob.assign(reinterpret_cast<const char*>(rec.payload.data()),
                        rec.payload.size());
            blob_ind = soci::i_ok;
        }

        sql << q,
            soci::use(rec.timestamp_iso),
            soci::use(sid),
            soci::use(rec.client_ip),
            soci::use(act),
            soci::use(mid),
            soci::use(rec.pos_x),
            soci::use(rec.pos_y),
            soci::use(rec.pos_z),
            soci::use(rec.search_int[0]),
            soci::use(rec.search_int[1]),
            soci::use(rec.search_int[2]),
            soci::use(rec.search_int[3]),
            soci::use(rec.search_int[4]),
            soci::use(rec.search_int[5]),
            soci::use(rec.search_int[6]),
            soci::use(rec.search_int[7]),
            soci::use(rec.search_int[8]),
            soci::use(rec.search_int[9]),
            soci::use(rec.search_int[10]),
            soci::use(rec.search_str[0]),
            soci::use(rec.search_str[1]),
            soci::use(rec.search_str[2]),
            soci::use(rec.search_str[3]),
            soci::use(rec.search_str[4]),
            soci::use(rec.search_str[5]),
            soci::use(rec.search_str[6]),
            soci::use(fmt),
            soci::use(blob, blob_ind);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("log_sink: insert failed action={} uid={} ip={} — {}",
            rec.action, rec.search_int[0], rec.client_ip, ex.what());
    }
}

void StdoutLogSink::Write(const LogRecord& rec)
{
    spdlog::info("audit action=0x{:04X} server={} uid={} ip={} key='{}' '{}'",
        rec.action, rec.server_id, rec.search_int[0], rec.client_ip,
        rec.search_str[0], rec.search_str[4]);
}

} // namespace tlogsvr
