#pragma once

// PeerAuthRepository — Repository<PeerAuthRow> on TPEER_AUTH.
//
// Canonical source-of-trust for server-to-server authentication.
// Every peer that wants to talk to another server must have an
// enabled row here; the row carries:
//   * (bGroupID, bServerID, bType) — identity
//   * szSecretHex                  — HMAC shared secret (hex-encoded)
//   * szIPAllowlist                — comma-separated CIDR blocks
//   * bEnabled                     — kill switch
//   * dwFailCount / dwOkCount      — operational health counters
//
// Used by PeerSecurityGate at handshake time. Loaded once at startup
// + reloadable on demand (admin shell "peer-auth reload" command).

#include "fourstory/db/orm/db_context.h"
#include "fourstory/db/orm/entity_mapping.h"
#include "fourstory/db/orm/repository.h"
#include "fourstory/db/orm/sp_call.h"
#include "fourstory/db/session_pool.h"
#include "peer_auth_token.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <string>
#include <vector>

namespace fourstory::security {

struct PeerAuthRow
{
    std::uint8_t   group_id      = 0;
    std::uint8_t   server_id     = 0;
    std::uint8_t   type_id       = 0;
    std::string    peer_name;
    std::string    secret_hex;
    std::string    ip_allowlist;   // comma-separated CIDR
    std::uint8_t   enabled        = 1;
    std::uint32_t  fail_count     = 0;
    std::uint32_t  ok_count       = 0;
};

} // namespace fourstory::security

namespace fourstory::db::orm {

template<>
struct EntityMapping<fourstory::security::PeerAuthRow>
{
    using T = fourstory::security::PeerAuthRow;

    static constexpr const char* Table    = "TPEER_AUTH";
    // Composite PK (bGroupID, bServerID) — Repository::FindById uses
    // bServerID alone; full lookup goes through Where() or FindByKey().
    static constexpr const char* PkColumn = "bServerID";

    static T FromRow(const soci::row& r)
    {
        T row;
        row.group_id     = static_cast<std::uint8_t>(r.get<int>(0));
        row.server_id    = static_cast<std::uint8_t>(r.get<int>(1));
        row.type_id      = static_cast<std::uint8_t>(r.get<int>(2));
        row.peer_name    = r.get<std::string>(3);
        row.secret_hex   = r.get<std::string>(4);
        row.ip_allowlist = r.get<std::string>(5);
        row.enabled      = static_cast<std::uint8_t>(r.get<int>(6));
        row.fail_count   = static_cast<std::uint32_t>(r.get<int>(7));
        row.ok_count     = static_cast<std::uint32_t>(r.get<int>(8));
        return row;
    }

    static std::string SelectAllSql()
    {
        return "SELECT bGroupID, bServerID, bType, szPeerName, "
               "szSecretHex, szIPAllowlist, bEnabled, "
               "dwFailCount, dwOkCount FROM TPEER_AUTH";
    }
    static std::string SelectByIdSql()
    {
        return SelectAllSql() + " WHERE bServerID = :pk";
    }
    static std::string InsertSql()
    {
        return "INSERT INTO TPEER_AUTH "
               "(bGroupID, bServerID, bType, szPeerName, szSecretHex, "
               "szIPAllowlist, bEnabled) "
               "VALUES (:g, :s, :t, :n, :sec, :ips, :en)";
    }
    static std::string UpdateSql()
    {
        return "UPDATE TPEER_AUTH SET szSecretHex=:sec, "
               "szIPAllowlist=:ips, bEnabled=:en WHERE bServerID = :pk";
    }
    static std::string DeleteSql()
    {
        return "DELETE FROM TPEER_AUTH WHERE bServerID = :pk";
    }
    static void BindInsert(soci::statement& st, const T& e)
    {
        const int g = e.group_id, s = e.server_id, t = e.type_id, en = e.enabled;
        st , soci::use(g, "g")
           , soci::use(s, "s")
           , soci::use(t, "t")
           , soci::use(e.peer_name,    "n")
           , soci::use(e.secret_hex,   "sec")
           , soci::use(e.ip_allowlist, "ips")
           , soci::use(en, "en");
    }
    static void BindUpdate(soci::statement& st, const T& e)
    {
        const int s = e.server_id, en = e.enabled;
        st , soci::use(e.secret_hex,   "sec")
           , soci::use(e.ip_allowlist, "ips")
           , soci::use(en, "en")
           , soci::use(s, "pk");
    }
    static int GetPk(const T& e) { return e.server_id; }
};

} // namespace fourstory::db::orm

namespace fourstory::security {

class PeerAuthRepository
{
public:
    explicit PeerAuthRepository(fourstory::db::SessionPool& pool)
        : m_pool(pool) {}

    // Load every row. Caller builds a fast lookup map keyed by
    // (group, server) for handshake-time matching.
    std::vector<PeerAuthRow> LoadAll()
    {
        if (NativePostgreSQL())
        {
            auto lease = m_pool.Acquire();
            soci::rowset<soci::row> rows = (lease->prepare << NativeSelect());
            std::vector<PeerAuthRow> result;
            for (const auto& row : rows) result.push_back(NativeRow(row));
            return result;
        }
        fourstory::db::orm::DbContext ctx(m_pool);
        return ctx.Set<PeerAuthRow>().All();
    }

    // Find a single peer by its full identity tuple. Used by the
    // security gate on handshake when the claimed identity is known.
    std::vector<PeerAuthRow> FindByIdentity(std::uint8_t group_id,
                                             std::uint8_t server_id,
                                             std::uint8_t type_id)
    {
        if (NativePostgreSQL())
        {
            auto lease = m_pool.Acquire();
            const int group=group_id,server=server_id,type=type_id;
            soci::rowset<soci::row> rows = (lease->prepare << NativeSelect()+
                " WHERE group_id=:g AND server_id=:s AND type_id=:t",
                soci::use(group,"g"),soci::use(server,"s"),soci::use(type,"t"));
            std::vector<PeerAuthRow> result;
            for (const auto& row : rows) result.push_back(NativeRow(row));
            return result;
        }
        fourstory::db::orm::DbContext ctx(m_pool);
        const auto where =
            "bGroupID = "  + std::to_string(group_id)  +
            " AND bServerID = " + std::to_string(server_id) +
            " AND bType = "    + std::to_string(type_id);
        return ctx.Set<PeerAuthRow>().Where(where);
    }

    // Record an auth outcome → TPEER_AUTH_LOG + counter bump on TPEER_AUTH.
    void LogOutcome(std::uint8_t group_id,
                    std::uint8_t server_id,
                    std::uint8_t type_id,
                    const std::string& remote_ip,
                    PeerAuthOutcome outcome,
                    const std::string& reason = {})
    {
        try
        {
            auto lease = m_pool.Acquire();
            auto& sql = *lease;
            const int g = group_id, s = server_id, t = type_id,
                      o = static_cast<int>(outcome);
            if (NativePostgreSQL())
            {
                soci::transaction tx(sql);
                sql << "INSERT INTO runtime_control.peer_auth_log(group_id,server_id,type_id,remote_ip,outcome,reason) "
                       "VALUES(:g,:s,:t,:ip,:o,:r)",
                    soci::use(g,"g"),soci::use(s,"s"),soci::use(t,"t"),
                    soci::use(remote_ip,"ip"),soci::use(o,"o"),soci::use(reason,"r");
                if (group_id != 0 || server_id != 0)
                {
                    const std::string counter=outcome==PeerAuthOutcome::Allow?"ok_count":"fail_count";
                    sql << "UPDATE runtime_control.peer_trust SET "+counter+"=("+counter+
                        "+1) % 4294967296,last_seen=clock_timestamp() WHERE group_id=:g AND server_id=:s",
                        soci::use(g,"g"),soci::use(s,"s");
                }
                // Audit and counters commit together. Unknown commit outcomes
                // are reported below, never automatically retried.
                tx.commit();
                return;
            }
            if (reason.empty())
            {
                sql << "INSERT INTO TPEER_AUTH_LOG "
                       "(bGroupID, bServerID, bType, szRemoteIP, bOutcome) "
                       "VALUES (:g, :s, :t, :ip, :o)",
                    soci::use(g, "g"), soci::use(s, "s"),
                    soci::use(t, "t"), soci::use(remote_ip, "ip"),
                    soci::use(o, "o");
            }
            else
            {
                sql << "INSERT INTO TPEER_AUTH_LOG "
                       "(bGroupID, bServerID, bType, szRemoteIP, bOutcome, szReason) "
                       "VALUES (:g, :s, :t, :ip, :o, :r)",
                    soci::use(g, "g"), soci::use(s, "s"),
                    soci::use(t, "t"), soci::use(remote_ip, "ip"),
                    soci::use(o, "o"), soci::use(reason, "r");
            }
            // Counter bump on the matching TPEER_AUTH row when identity
            // was resolvable (group/server > 0).
            if (group_id != 0 || server_id != 0)
            {
                const char* column = (outcome == PeerAuthOutcome::Allow)
                    ? "dwOkCount" : "dwFailCount";
                sql << std::string("UPDATE TPEER_AUTH SET ") + column +
                       " = " + column + " + 1, dtLastSeen = GETDATE() "
                       "WHERE bGroupID = :g AND bServerID = :s",
                    soci::use(g, "g"), soci::use(s, "s");
            }
        }
        catch (const std::exception& ex)
        {
            spdlog::warn("peer_auth_repo.LogOutcome failed: {}", ex.what());
        }
    }

    // Purge old TPEER_AUTH_LOG rows. Retention 30 days by default.
    void PurgeOldLogs(int days = 30)
    {
        if(days<0)throw std::invalid_argument("Peer audit retention cannot be negative");
        try
        {
            auto lease = m_pool.Acquire();
            if (NativePostgreSQL())
            {
                (*lease) << "DELETE FROM runtime_control.peer_auth_log WHERE recorded_at < "
                    "clock_timestamp()-make_interval(days => :d)",soci::use(days,"d");
                return;
            }
            const int offset_days = -days;
            (*lease) << "DELETE FROM TPEER_AUTH_LOG "
                        "WHERE dtAt < DATEADD(day, :d, GETDATE())",
                soci::use(offset_days, "d");
        }
        catch (const std::exception& ex)
        {
            spdlog::warn("peer_auth_repo.PurgeOldLogs failed: {}", ex.what());
        }
    }

private:
    bool NativePostgreSQL() const { return m_pool.GetBackend()==fourstory::db::Backend::PostgreSQL; }
    static std::string NativeSelect() {
        return "SELECT group_id,server_id,type_id,peer_name,secret_hex,ip_allowlist,"
            "CASE WHEN enabled THEN 1 ELSE 0 END,fail_count,ok_count FROM runtime_control.peer_trust";
    }
    static PeerAuthRow NativeRow(const soci::row& r) {
        PeerAuthRow row;
        row.group_id=static_cast<std::uint8_t>(r.get<int>(0));
        row.server_id=static_cast<std::uint8_t>(r.get<int>(1));
        row.type_id=static_cast<std::uint8_t>(r.get<int>(2));
        row.peer_name=r.get<std::string>(3);row.secret_hex=r.get<std::string>(4);
        row.ip_allowlist=r.get<std::string>(5);row.enabled=static_cast<std::uint8_t>(r.get<int>(6));
        row.fail_count=static_cast<std::uint32_t>(r.get<long long>(7));
        row.ok_count=static_cast<std::uint32_t>(r.get<long long>(8));
        return row;
    }
    fourstory::db::SessionPool& m_pool;
};

} // namespace fourstory::security
