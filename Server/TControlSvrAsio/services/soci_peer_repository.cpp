#include "soci_peer_repository.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <ctime>
#include <stdexcept>
#include <string>

namespace tcontrolsvr {

namespace {

// Decompose synthetic service_id → (group, type, server) bytes.
// Matches MakeServiceId in soci_service_inventory.cpp:
//   service_id = (group << 16) | (type << 8) | server
struct ServiceIdParts
{
    std::uint8_t group_id;
    std::uint8_t type_id;
    std::uint8_t server_id;
};

ServiceIdParts Decompose(std::uint32_t id)
{
    return {
        static_cast<std::uint8_t>((id >> 16) & 0xFF),
        static_cast<std::uint8_t>((id >>  8) & 0xFF),
        static_cast<std::uint8_t>( id        & 0xFF)
    };
}

std::tm UnixToTm(std::int64_t unix_sec)
{
    auto t = static_cast<std::time_t>(unix_sec);
    std::tm result{};
#ifdef _WIN32
    gmtime_s(&result, &t);
#else
    gmtime_r(&t, &result);
#endif
    return result;
}

} // namespace

SociPeerRepository::SociPeerRepository(fourstory::db::SessionPool& pool)
    : m_pool(pool)
{
}

void SociPeerRepository::Upsert(const RegistryEntry& entry)
{
    const auto [g, t, s] = Decompose(entry.service_id);
    const int ig = g, it = t, is = s;

    auto start_tm = UnixToTm(entry.start_unix);
    const int    port      = entry.reported_port;
    const int    pid       = static_cast<int>(entry.pid);
    const int    cur       = static_cast<int>(entry.cur_users);
    const int    max_u     = static_cast<int>(entry.max_users);
    const long long epoch  = static_cast<long long>(entry.lease_epoch);

    try
    {
        auto lease = m_pool.Acquire();
        auto& sql  = *lease;

        int exists = 0;
        sql << "SELECT COUNT(*) FROM TPEER_REGISTRY "
               "WHERE bGroupID=:g AND bServerID=:s",
            soci::use(ig, "g"), soci::use(is, "s"),
            soci::into(exists);

        if (exists > 0)
        {
            sql << "UPDATE TPEER_REGISTRY SET "
                   "bType=:t, szReportedAddr=:a, wReportedPort=:p, "
                   "szVersion=:v, dwPID=:pid, dtStartTime=:st, "
                   "dwCurUsers=:cu, dwMaxUsers=:mu, llLeaseEpoch=:ep, "
                   "dtRegisteredAt=GETDATE(), dtHeartbeatAt=GETDATE() "
                   "WHERE bGroupID=:g AND bServerID=:s",
                soci::use(it,                "t"),
                soci::use(entry.reported_addr, "a"),
                soci::use(port,              "p"),
                soci::use(entry.version,     "v"),
                soci::use(pid,               "pid"),
                soci::use(start_tm,          "st"),
                soci::use(cur,               "cu"),
                soci::use(max_u,             "mu"),
                soci::use(epoch,             "ep"),
                soci::use(ig,                "g"),
                soci::use(is,                "s");
        }
        else
        {
            sql << "INSERT INTO TPEER_REGISTRY "
                   "(bGroupID, bServerID, bType, szReportedAddr, wReportedPort, "
                   "szVersion, dwPID, dtStartTime, dwCurUsers, dwMaxUsers, "
                   "llLeaseEpoch, dtRegisteredAt, dtHeartbeatAt) "
                   "VALUES (:g, :s, :t, :a, :p, :v, :pid, :st, :cu, :mu, :ep, "
                   "GETDATE(), GETDATE())",
                soci::use(ig,                "g"),
                soci::use(is,                "s"),
                soci::use(it,                "t"),
                soci::use(entry.reported_addr, "a"),
                soci::use(port,              "p"),
                soci::use(entry.version,     "v"),
                soci::use(pid,               "pid"),
                soci::use(start_tm,          "st"),
                soci::use(cur,               "cu"),
                soci::use(max_u,             "mu"),
                soci::use(epoch,             "ep");
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_peer_repository.Upsert service_id={:#x}: {}",
            entry.service_id, ex.what());
    }
}

void SociPeerRepository::UpdateHeartbeat(std::uint32_t service_id,
                                          std::uint64_t lease_epoch,
                                          std::uint32_t cur_users,
                                          std::uint32_t max_users)
{
    const auto [g, t, s] = Decompose(service_id);
    const int ig = g, is = s;
    const int cu = static_cast<int>(cur_users);
    const int mu = static_cast<int>(max_users);
    const long long ep = static_cast<long long>(lease_epoch);

    try
    {
        auto lease = m_pool.Acquire();
        (*lease) << "UPDATE TPEER_REGISTRY SET "
                    "dtHeartbeatAt=GETDATE(), dwCurUsers=:cu, "
                    "dwMaxUsers=:mu, llLeaseEpoch=:ep "
                    "WHERE bGroupID=:g AND bServerID=:s",
            soci::use(cu,  "cu"),
            soci::use(mu,  "mu"),
            soci::use(ep,  "ep"),
            soci::use(ig,  "g"),
            soci::use(is,  "s");
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_peer_repository.UpdateHeartbeat service_id={:#x}: {}",
            service_id, ex.what());
    }
}

void SociPeerRepository::Delete(std::uint32_t service_id)
{
    const auto [g, t, s] = Decompose(service_id);
    const int ig = g, is = s;

    try
    {
        auto lease = m_pool.Acquire();
        (*lease) << "DELETE FROM TPEER_REGISTRY "
                    "WHERE bGroupID=:g AND bServerID=:s",
            soci::use(ig, "g"), soci::use(is, "s");
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_peer_repository.Delete service_id={:#x}: {}",
            service_id, ex.what());
    }
}

void SociPeerRepository::InsertStatusLog(std::uint8_t  group_id,
                                          std::uint8_t  server_id,
                                          std::uint8_t  type_id,
                                          ServiceStatus old_status,
                                          ServiceStatus new_status,
                                          const std::string& reason)
{
    const int ig  = group_id;
    const int is  = server_id;
    const int it  = type_id;
    const int old_s = static_cast<int>(old_status);
    const int new_s = static_cast<int>(new_status);

    try
    {
        auto lease = m_pool.Acquire();
        auto& sql  = *lease;

        if (reason.empty())
        {
            sql << "INSERT INTO TPEER_STATUS_LOG "
                   "(bGroupID, bServerID, bType, bOldStatus, bNewStatus) "
                   "VALUES (:g, :s, :t, :os, :ns)",
                soci::use(ig,    "g"),
                soci::use(is,    "s"),
                soci::use(it,    "t"),
                soci::use(old_s, "os"),
                soci::use(new_s, "ns");
        }
        else
        {
            sql << "INSERT INTO TPEER_STATUS_LOG "
                   "(bGroupID, bServerID, bType, bOldStatus, bNewStatus, szReason) "
                   "VALUES (:g, :s, :t, :os, :ns, :r)",
                soci::use(ig,     "g"),
                soci::use(is,     "s"),
                soci::use(it,     "t"),
                soci::use(old_s,  "os"),
                soci::use(new_s,  "ns"),
                soci::use(reason, "r");
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_peer_repository.InsertStatusLog group={} server={}: {}",
            group_id, server_id, ex.what());
    }
}

void SociPeerRepository::InsertMetrics(const PeerMetricsSample& sample)
{
    // One INSERT per peer per minute — throttle checked here so
    // OnServiceMonitorReq can call unconditionally.
    const auto now = std::chrono::steady_clock::now();
    auto& last = m_metrics_last[
        (static_cast<std::uint32_t>(sample.group_id) << 16) |
        (static_cast<std::uint32_t>(sample.type_id)  <<  8) |
         static_cast<std::uint32_t>(sample.server_id)];
    if (last != std::chrono::steady_clock::time_point{} &&
        now - last < std::chrono::minutes(1))
    {
        return;
    }
    last = now;

    const int ig  = sample.group_id;
    const int is  = sample.server_id;
    const int it  = sample.type_id;
    const int ses = static_cast<int>(sample.sessions);
    const int usr = static_cast<int>(sample.users);
    const int act = static_cast<int>(sample.active);

    try
    {
        auto lease = m_pool.Acquire();
        (*lease) << "INSERT INTO TPEER_METRICS "
                    "(bGroupID, bServerID, bType, dwSession, dwUser, dwActiveUser) "
                    "VALUES (:g, :s, :t, :ses, :usr, :act)",
            soci::use(ig,  "g"),
            soci::use(is,  "s"),
            soci::use(it,  "t"),
            soci::use(ses, "ses"),
            soci::use(usr, "usr"),
            soci::use(act, "act");
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_peer_repository.InsertMetrics group={} server={}: {}",
            sample.group_id, sample.server_id, ex.what());
    }
}

std::vector<RegistryEntry> SociPeerRepository::LoadAll()
{
    std::vector<RegistryEntry> result;
    try
    {
        auto lease = m_pool.Acquire();
        soci::rowset<soci::row> rs = ((*lease).prepare <<
            "SELECT bGroupID, bServerID, bType, szReportedAddr, "
            "wReportedPort, szVersion, dwPID, dtStartTime, "
            "dwCurUsers, dwMaxUsers, llLeaseEpoch "
            "FROM TPEER_REGISTRY");

        const auto now = std::chrono::steady_clock::now();
        for (const auto& row : rs)
        {
            const int ig  = row.get<int>(0);
            const int is  = row.get<int>(1);
            const int it  = row.get<int>(2);

            RegistryEntry e{};
            e.service_id = (static_cast<std::uint32_t>(ig & 0xFF) << 16) |
                           (static_cast<std::uint32_t>(it & 0xFF) <<  8) |
                            static_cast<std::uint32_t>(is & 0xFF);
            e.reported_addr  = row.get<std::string>(3);
            e.reported_port  = static_cast<std::uint16_t>(row.get<int>(4));
            e.version        = row.get<std::string>(5);
            e.pid            = static_cast<std::uint32_t>(row.get<int>(6));
            // dtStartTime → start_unix: SOCI returns std::tm for datetime columns
            std::tm start_tm = row.get<std::tm>(7);
#ifdef _WIN32
            e.start_unix = static_cast<std::int64_t>(_mkgmtime(&start_tm));
#else
            e.start_unix = static_cast<std::int64_t>(timegm(&start_tm));
#endif
            e.cur_users      = static_cast<std::uint32_t>(row.get<int>(8));
            e.max_users      = static_cast<std::uint32_t>(row.get<int>(9));
            e.lease_epoch    = static_cast<std::uint64_t>(row.get<long long>(10));
            // Both time_points set to now — if the peer doesn't
            // heartbeat within the lease window it will be expired.
            e.registered_at    = now;
            e.last_heartbeat_at = now;
            result.push_back(std::move(e));
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_peer_repository.LoadAll: {}", ex.what());
    }
    spdlog::info("soci_peer_repository: loaded {} registration(s) from TPEER_REGISTRY",
        result.size());
    return result;
}

void SociPeerRepository::PurgeOldRows(int status_log_days, int metrics_days)
{
    try
    {
        auto lease = m_pool.Acquire();
        auto& sql  = *lease;

        sql << "DELETE FROM TPEER_STATUS_LOG "
               "WHERE dtChangedAt < DATEADD(day, :d, GETDATE())",
            soci::use(-status_log_days, "d");

        sql << "DELETE FROM TPEER_METRICS "
               "WHERE dtSampledAt < DATEADD(day, :d, GETDATE())",
            soci::use(-metrics_days, "d");

        spdlog::info("soci_peer_repository: purged status_log >{}d, "
                     "metrics >{}d", status_log_days, metrics_days);
    }
    catch (const std::exception& ex)
    {
        spdlog::warn("soci_peer_repository.PurgeOldRows: {}", ex.what());
    }
}

} // namespace tcontrolsvr
