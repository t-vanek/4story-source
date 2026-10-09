#include "postgresql_login_owner.h"
#include <openssl/rand.h>
#include <stdexcept>

namespace tloginsvr::services {
namespace {
constexpr int kNamespace = 0x34535459; // 4STY
constexpr int kLogin = 0x4c4f474e;     // LOGN
class OwnerBusy : public std::runtime_error {
public: OwnerBusy() : std::runtime_error("Another native Login owner is active") {}
};
}

PostgreSQLLoginOwner::PostgreSQLLoginOwner(const std::string& connection)
    : m_control(fourstory::db::Backend::PostgreSQL, connection, 1),
      m_lease(m_control.Acquire())
{
    try {
        auto& sql = *m_lease;
        sql << "SET statement_timeout='5s'";
        sql << "SET lock_timeout='3s'";
        int locked = 0;
        sql << "SELECT CASE WHEN pg_try_advisory_lock(:ns,:kind) THEN 1 ELSE 0 END",
            soci::use(kNamespace), soci::use(kLogin), soci::into(locked);
        if (!locked) throw OwnerBusy();
        unsigned char bytes[32];
        if (RAND_bytes(bytes, sizeof(bytes)) != 1) throw std::runtime_error("Random generation failed");
        constexpr char hex[] = "0123456789abcdef";
        for (auto b : bytes) { m_token += hex[b >> 4]; m_token += hex[b & 15]; }
        sql << "SELECT pg_backend_pid()", soci::into(m_backend_pid);
        // This waits for FOR SHARE locks held by any previously active writer.
        // If the old control connection died, its in-flight transactions finish
        // before the new token can be committed; subsequent old writes fail.
        soci::transaction tx(sql);
        sql << "INSERT INTO app_global.login_runtime_owner(singleton,owner_token,backend_pid) "
               "VALUES (true,:t,:p) ON CONFLICT (singleton) DO UPDATE SET "
               "owner_token=EXCLUDED.owner_token,backend_pid=EXCLUDED.backend_pid,acquired_at=CURRENT_TIMESTAMP",
            soci::use(m_token), soci::use(m_backend_pid);
        tx.commit();
    } catch (const OwnerBusy&) { throw; }
    catch (...) { throw std::runtime_error("Native Login ownership claim failed"); }
}

bool PostgreSQLLoginOwner::Healthy()
{
    try {
        int found = 0;
        *m_lease << "SELECT count(*) FROM app_global.login_runtime_owner "
                    "WHERE singleton AND owner_token=:t AND backend_pid=pg_backend_pid()",
            soci::use(m_token), soci::into(found);
        return found == 1;
    } catch (...) { return false; }
}

std::unique_ptr<soci::transaction> BeginLoginTransaction(
    fourstory::db::SessionPool& pool, soci::session& sql, const std::string& token, bool repeatable_read)
{
    if (pool.GetBackend() != fourstory::db::Backend::PostgreSQL) return {};
    if (token.empty()) throw std::runtime_error("Native Login write requires process ownership");
    auto tx = std::make_unique<soci::transaction>(sql);
    if (repeatable_read) sql << "SET TRANSACTION ISOLATION LEVEL REPEATABLE READ";
    sql << "SET LOCAL statement_timeout='5s'";
    sql << "SET LOCAL lock_timeout='3s'";
    int found = 0;
    sql << "SELECT 1 FROM app_global.login_runtime_owner WHERE singleton AND owner_token=:t FOR SHARE",
        soci::use(token), soci::into(found);
    if (!sql.got_data() || found != 1) throw std::runtime_error("Native Login ownership lost");
    return tx;
}
int ExpirePendingHandoff(soci::session& sql, int user_id) {
    int removed=0;
    // A pending reservation is proof that Map has not claimed this session.
    // Never infer staleness merely from a nonzero selected character or its age.
    sql << "WITH expired AS (DELETE FROM app_global.\"TCURRENTUSER\" s USING app_global.map_handoff h "
           "WHERE s.\"dwKEY\"=h.session_key AND s.\"dwUserID\"=h.user_id AND h.user_id=:u "
           "AND s.\"dwCharID\"=h.char_id AND s.\"bGroupID\"=h.world_id AND s.\"bChannel\"=h.channel "
           "AND h.expires_at<=clock_timestamp() RETURNING s.\"dwKEY\"), audit AS "
           "(UPDATE app_global.\"TLOG\" SET \"timeLOGOUT\"=CURRENT_TIMESTAMP WHERE \"dwKEY\" IN (SELECT \"dwKEY\" FROM expired)) "
           "SELECT count(*) FROM expired", soci::use(user_id,"u"),soci::into(removed);
    return removed;
}

} // namespace tloginsvr::services
