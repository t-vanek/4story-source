#pragma once

// AuditQueryRepository — Repository<LogAuditEntry> over TLOG_AUDIT.
//
// Demonstrates fourstory::db::orm framework on a real production
// schema. The write path stays in SociLogSink (it does dialect-
// specific binary BLOB binding which doesn't fit the generic CRUD
// shape). This class is the read-side counterpart: SELECTs recent
// rows for the admin shell / health endpoint / forensic queries.
//
// Usage:
//
//   AuditQueryRepository repo(pool);
//   auto recent = repo.LatestN(50);
//   auto by_user = repo.WhereUserId(12345, 20);

#include "fourstory/db/orm/db_context.h"
#include "fourstory/db/orm/entity_mapping.h"
#include "fourstory/db/orm/repository.h"
#include "fourstory/db/session_pool.h"
#include "db/schema_validator.h"
#include "audit_bytes.h"
#include <limits>

#include <soci/soci.h>

#include <cstdint>
#include <string>
#include <vector>

namespace tlogsvr {

// One TLOG_AUDIT row — the columns operators most often want to view.
// Subset of the full schema (28 columns); BLOB payload is intentionally
// omitted so SELECTs stay cheap (varies 0..512 bytes per row).
struct LogAuditEntry
{
    std::int64_t  log_id        = 0;   // synthetic PK (IDENTITY in MSSQL)
    std::string   log_date;            // ISO string
    std::uint32_t server_id     = 0;
    std::string   client_ip;
    std::uint32_t action        = 0;
    std::uint16_t map_id        = 0;
    std::int64_t  search_int_0  = 0;   // typically dwUserID
    std::string   search_str_0; // typically character name
    std::uint32_t format        = 0;
};

} // namespace tlogsvr

namespace fourstory::db::orm {

template<>
struct EntityMapping<tlogsvr::LogAuditEntry>
{
    using T = tlogsvr::LogAuditEntry;

    static constexpr const char* Table    = "TLOG_AUDIT";
    static constexpr const char* PkColumn = "LT_ID";

    static T FromRow(const soci::row& r)
    {
        T e;
        e.log_id       = r.get<long long>(0);
        e.log_date     = r.get<std::string>(1);
        e.server_id    = static_cast<std::uint32_t>(r.get<int>(2));
        e.client_ip    = r.get<std::string>(3);
        e.action       = static_cast<std::uint32_t>(r.get<int>(4));
        e.map_id       = static_cast<std::uint16_t>(r.get<int>(5));
        e.search_int_0 = r.get<long long>(6);
        e.search_str_0 = r.get<std::string>(7);
        e.format       = static_cast<std::uint32_t>(r.get<int>(8));
        return e;
    }

    static std::string SelectAllSql()
    {
        return "SELECT LT_ID, LT_LOGDATE, LT_SERVERID, LT_CLIENTIP, "
               "  LT_ACTION, LT_MAPID, LT_DWKEY1, LT_KEY1, LT_FMT "
               "FROM TLOG_AUDIT";
    }
    static std::string SelectByIdSql()
    {
        return SelectAllSql() + " WHERE LT_ID = :pk";
    }
    // INSERT/UPDATE/DELETE intentionally not implemented — the write
    // path lives in SociLogSink (dialect-specific BLOB handling).
    // Repository<LogAuditEntry>::Insert/Update/Delete will throw if
    // ever invoked, which is the documented contract for read-mostly
    // entities.
    static std::string InsertSql() { return ""; }
    static std::string UpdateSql() { return ""; }
    static std::string DeleteSql() { return "DELETE FROM TLOG_AUDIT WHERE LT_ID = :pk"; }
    static void BindInsert(soci::statement&, const T&) {}
    static void BindUpdate(soci::statement&, const T&) {}
    static long long GetPk(const T& e) { return e.log_id; }
};

} // namespace fourstory::db::orm

namespace tlogsvr {

class AuditQueryRepository
{
public:
    explicit AuditQueryRepository(fourstory::db::SessionPool& pool, std::string table = "TLOG_AUDIT")
        : m_pool(pool), m_table(std::move(table))
    { db::AuditRelation(m_pool.GetBackend(), m_table); }

    // Most recent N rows, ordered by LT_ID DESC.
    std::vector<LogAuditEntry> LatestN(std::size_t limit)
    {
        if(m_pool.GetBackend()==fourstory::db::Backend::PostgreSQL) return NativeRead(limit, nullptr);
        fourstory::db::orm::DbContext ctx(m_pool);
        return ctx.Set<LogAuditEntry>().RawSelect(
            "SELECT TOP " + std::to_string(limit) + " "
            "  LT_ID, LT_LOGDATE, LT_SERVERID, LT_CLIENTIP, "
            "  LT_ACTION, LT_MAPID, LT_DWKEY1, LT_KEY1, LT_FMT "
            "FROM "+db::AuditRelation(m_pool.GetBackend(),m_table)+" ORDER BY LT_ID DESC");
    }

    // Latest N rows for a specific user (filters on LT_DWKEY1 which
    // legacy convention uses for dwUserID).
    std::vector<LogAuditEntry> WhereUserId(std::int64_t user_id,
                                           std::size_t limit)
    {
        if(m_pool.GetBackend()==fourstory::db::Backend::PostgreSQL) return NativeRead(limit, &user_id);
        fourstory::db::orm::DbContext ctx(m_pool);
        return ctx.Set<LogAuditEntry>().RawSelect(
            "SELECT TOP " + std::to_string(limit) + " "
            "  LT_ID, LT_LOGDATE, LT_SERVERID, LT_CLIENTIP, "
            "  LT_ACTION, LT_MAPID, LT_DWKEY1, LT_KEY1, LT_FMT "
            "FROM "+db::AuditRelation(m_pool.GetBackend(),m_table)+" WHERE LT_DWKEY1 = " + std::to_string(user_id) +
            " ORDER BY LT_ID DESC");
    }

    std::size_t Count()
    {
        auto lease=m_pool.Acquire(); long long count=0;
        *lease << "SELECT count(*) FROM "+db::AuditRelation(m_pool.GetBackend(),m_table),soci::into(count);
        if(count<0 || static_cast<unsigned long long>(count)>std::numeric_limits<std::size_t>::max())
            throw std::overflow_error("Audit count exceeds host size");
        return static_cast<std::size_t>(count);
    }

private:
    std::vector<LogAuditEntry> NativeRead(std::size_t limit,const std::int64_t* user) {
        if(limit>static_cast<std::size_t>(std::numeric_limits<long long>::max()))
            throw std::overflow_error("Audit query limit exceeds PostgreSQL range");
        auto lease=m_pool.Acquire(); const long long count=limit,uid=user?*user:0;
        const auto query="SELECT lt_id,to_char(lt_logdate,'YYYY-MM-DD HH24:MI:SS'),lt_serverid,"
            "encode(lt_clientip,'hex'),lt_action,lt_mapid,lt_dwkey1,encode(lt_key1,'hex'),lt_fmt FROM "
            +db::AuditRelation(m_pool.GetBackend(),m_table)+(user?" WHERE lt_dwkey1=:u":"")+" ORDER BY lt_id DESC LIMIT :n";
        long long id=0,server=0,action=0,key=0,format=0; int map=0;
        std::string date,ip,name;
        soci::statement statement(*lease);statement.alloc();statement.prepare(query);
        if(user)statement.exchange(soci::use(uid,"u"));
        statement.exchange(soci::use(count,"n"));
        statement.exchange(soci::into(id));statement.exchange(soci::into(date));statement.exchange(soci::into(server));
        statement.exchange(soci::into(ip));statement.exchange(soci::into(action));statement.exchange(soci::into(map));
        statement.exchange(soci::into(key));statement.exchange(soci::into(name));statement.exchange(soci::into(format));
        statement.define_and_bind();statement.execute();
        std::vector<LogAuditEntry> out;
        while(statement.fetch()) {
            if(server<0||server>4294967295LL||action<0||action>4294967295LL||format<0||format>4294967295LL||map<0||map>65535)
                throw std::runtime_error("Audit row exceeds original field width");
            out.push_back({id,date,static_cast<std::uint32_t>(server),AuditUnhex(ip),static_cast<std::uint32_t>(action),
                static_cast<std::uint16_t>(map),key,AuditUnhex(name),static_cast<std::uint32_t>(format)});
        }
        return out;
    }
    fourstory::db::SessionPool& m_pool;
    std::string m_table;
};

} // namespace tlogsvr
