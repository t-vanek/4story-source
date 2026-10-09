#include "soci_registry_persistence.h"

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <utility>

namespace tcontrolsvr {

namespace {

// Inline-safe identifier check. The table name is operator-supplied
// but inlined directly into the SQL — fail fast on anything that
// isn't a plain SQL identifier so a typo can't open an injection
// path. Matches the validator pattern used by TLogSvrAsio for
// target_table.
bool IsSqlIdentifier(const std::string& s)
{
    if (s.empty() || s.size() > 128) return false;
    if (!(std::isalpha(static_cast<unsigned char>(s.front())) ||
          s.front() == '_'))
        return false;
    for (char c : s)
    {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_'))
            return false;
    }
    return true;
}

std::int64_t NowUnix()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// Convert stored unix seconds back to an in-RAM steady_clock time
// point. We synthesize "the moment in steady_clock time when the
// peer's last_heartbeat would have happened" by subtracting the age
// from now. Good enough for the expiry sweep (90s windows) — steady
// vs system_clock drift is microseconds per minute.
std::chrono::steady_clock::time_point
HydrateSteady(std::int64_t stored_unix)
{
    const auto now_unix = NowUnix();
    const auto age = now_unix > stored_unix ? now_unix - stored_unix : 0;
    return std::chrono::steady_clock::now() -
        std::chrono::seconds(age);
}

} // namespace

SociRegistryPersistence::SociRegistryPersistence(
    fourstory::db::SessionPool& pool, Options opts)
    : m_pool(pool), m_opts(std::move(opts))
{
    if (!IsSqlIdentifier(m_opts.table_name))
        throw std::runtime_error(
            "registry.persistence.table_name not a SQL identifier: '"
            + m_opts.table_name + "'");
}

void SociRegistryPersistence::Upsert(const RegistryEntry& entry)
{
    auto write = [this, entry] {
        try
        {
            const auto now_unix = NowUnix();
            auto lease = m_pool.Acquire();
            soci::session& sql = *lease;
            soci::transaction tx(sql);
            // Portable upsert: DELETE-then-INSERT inside a txn.
            // MERGE / ON CONFLICT vary by backend; DELETE+INSERT
            // works the same on MSSQL, PostgreSQL, SQLite. The
            // existing TPatchSvrAsio MarkPreVersionComplete uses the
            // same pattern when targeting non-MSSQL backends.
            const int sid_i = static_cast<int>(entry.service_id);
            sql << "DELETE FROM \"" + m_opts.table_name +
                   "\" WHERE \"dwServiceID\" = :sid",
                soci::use(sid_i, "sid");

            const int   port_i = entry.reported_port;
            const int   pid_i  = static_cast<int>(entry.pid);
            const long long start_ll = entry.start_unix;
            const int   cur_i  = static_cast<int>(entry.cur_users);
            const int   max_i  = static_cast<int>(entry.max_users);
            const long long lease_ll = static_cast<long long>(entry.lease_epoch);
            sql << "INSERT INTO \"" + m_opts.table_name + "\" ("
                "\"dwServiceID\", \"szReportedName\", \"szReportedAddr\", "
                "\"wReportedPort\", \"szVersion\", \"dwPid\", "
                "\"qwStartUnix\", \"dwCurUsers\", \"dwMaxUsers\", "
                "\"qwLeaseEpoch\", \"tRegisteredUnix\", "
                "\"tLastHeartbeatUnix\") VALUES ("
                ":sid, :name, :addr, :port, :ver, :pid, :start, "
                ":cur, :max, :lease, :reg, :hb)",
                soci::use(sid_i, "sid"),
                soci::use(entry.reported_name, "name"),
                soci::use(entry.reported_addr, "addr"),
                soci::use(port_i, "port"),
                soci::use(entry.version, "ver"),
                soci::use(pid_i, "pid"),
                soci::use(start_ll, "start"),
                soci::use(cur_i, "cur"),
                soci::use(max_i, "max"),
                soci::use(lease_ll, "lease"),
                soci::use(static_cast<long long>(now_unix), "reg"),
                soci::use(static_cast<long long>(now_unix), "hb");
            tx.commit();
        }
        catch (const std::exception& ex)
        {
            spdlog::warn("registry.persistence: Upsert sid={:#x} failed: {}",
                entry.service_id, ex.what());
        }
    };
    if (m_opts.worker_pool)
        boost::asio::post(*m_opts.worker_pool, std::move(write));
    else
        write();
}

void SociRegistryPersistence::Touch(std::uint32_t  service_id,
                                    std::uint64_t  lease_epoch,
                                    std::uint32_t  cur_users,
                                    std::uint32_t  max_users,
                                    std::int64_t   heartbeat_unix)
{
    auto write = [this, service_id, lease_epoch, cur_users, max_users,
                  heartbeat_unix] {
        try
        {
            auto lease = m_pool.Acquire();
            soci::session& sql = *lease;
            const int sid_i = static_cast<int>(service_id);
            const int cur_i = static_cast<int>(cur_users);
            const int max_i = static_cast<int>(max_users);
            const long long lease_ll = static_cast<long long>(lease_epoch);
            const long long hb_ll    = heartbeat_unix;
            // The WHERE lease_epoch=:lease guard makes Touch a no-op
            // if a stale heartbeat lands after a re-registration
            // bumped the epoch — same defensive semantics as the
            // in-RAM Heartbeat path.
            sql << "UPDATE \"" + m_opts.table_name + "\" SET "
                "\"dwCurUsers\" = :cur, "
                "\"dwMaxUsers\" = :max, "
                "\"tLastHeartbeatUnix\" = :hb "
                "WHERE \"dwServiceID\" = :sid AND \"qwLeaseEpoch\" = :lease",
                soci::use(cur_i, "cur"),
                soci::use(max_i, "max"),
                soci::use(hb_ll, "hb"),
                soci::use(sid_i, "sid"),
                soci::use(lease_ll, "lease");
        }
        catch (const std::exception& ex)
        {
            spdlog::warn("registry.persistence: Touch sid={:#x} failed: {}",
                service_id, ex.what());
        }
    };
    if (m_opts.worker_pool)
        boost::asio::post(*m_opts.worker_pool, std::move(write));
    else
        write();
}

void SociRegistryPersistence::Remove(std::uint32_t service_id)
{
    auto write = [this, service_id] {
        try
        {
            auto lease = m_pool.Acquire();
            soci::session& sql = *lease;
            const int sid_i = static_cast<int>(service_id);
            sql << "DELETE FROM \"" + m_opts.table_name +
                   "\" WHERE \"dwServiceID\" = :sid",
                soci::use(sid_i, "sid");
        }
        catch (const std::exception& ex)
        {
            spdlog::warn("registry.persistence: Remove sid={:#x} failed: {}",
                service_id, ex.what());
        }
    };
    if (m_opts.worker_pool)
        boost::asio::post(*m_opts.worker_pool, std::move(write));
    else
        write();
}

std::vector<RegistryEntry> SociRegistryPersistence::LoadAll()
{
    std::vector<RegistryEntry> out;
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT \"dwServiceID\", \"szReportedName\", \"szReportedAddr\", "
            "       \"wReportedPort\", \"szVersion\", \"dwPid\", "
            "       \"qwStartUnix\", \"dwCurUsers\", \"dwMaxUsers\", "
            "       \"qwLeaseEpoch\", \"tRegisteredUnix\", "
            "       \"tLastHeartbeatUnix\" "
            "FROM \"" + m_opts.table_name + "\"");
        for (const auto& r : rs)
        {
            RegistryEntry e{};
            e.service_id      = static_cast<std::uint32_t>(r.get<int>(0));
            e.reported_name   = r.get<std::string>(1);
            e.reported_addr   = r.get<std::string>(2);
            e.reported_port   = static_cast<std::uint16_t>(r.get<int>(3));
            e.version         = r.get<std::string>(4);
            e.pid             = static_cast<std::uint32_t>(r.get<int>(5));
            e.start_unix      = static_cast<std::int64_t>(r.get<long long>(6));
            e.cur_users       = static_cast<std::uint32_t>(r.get<int>(7));
            e.max_users       = static_cast<std::uint32_t>(r.get<int>(8));
            e.lease_epoch     = static_cast<std::uint64_t>(r.get<long long>(9));
            const auto reg_unix = static_cast<std::int64_t>(r.get<long long>(10));
            const auto hb_unix  = static_cast<std::int64_t>(r.get<long long>(11));
            e.registered_at      = HydrateSteady(reg_unix);
            e.last_heartbeat_at  = HydrateSteady(hb_unix);
            out.push_back(std::move(e));
        }
        spdlog::info("registry.persistence: loaded {} entries from '{}'",
            out.size(), m_opts.table_name);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("registry.persistence: LoadAll failed: {} — "
                      "starting with empty registry", ex.what());
        out.clear();
    }
    return out;
}

} // namespace tcontrolsvr
