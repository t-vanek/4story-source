#include "soci_peer_repository.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <ctime>
#include <stdexcept>
#include <string>
#include <limits>

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

SociPeerRepository::SociPeerRepository(fourstory::db::SessionPool& pool, std::shared_ptr<native_control::Owner> owner)
    : m_owner(std::move(owner)), m_pool(pool)
{
}

void SociPeerRepository::Upsert(const RegistryEntry& entry)
{
    if(native_control::Enabled(m_pool)) {
        if(m_commit_uncertain)throw std::runtime_error("Peer registry commit outcome unknown; restart required");
        if(!entry.lease_epoch||entry.lease_epoch>std::uint64_t(std::numeric_limits<long long>::max()))
            throw std::runtime_error("Invalid durable peer epoch");
        auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=native_control::Begin(sql,m_owner);
        const int id=entry.service_id,port=entry.reported_port;
        const long long pid=entry.pid,start=entry.start_unix,cur=entry.cur_users,max=entry.max_users,epoch=entry.lease_epoch;
        const auto name=native_control::Hex(entry.reported_name),version=native_control::Hex(entry.version);long long stored=0;
        sql << "INSERT INTO runtime_control.peer_registry(service_id,name_bytes,reported_addr,reported_port,version_bytes,pid,start_unix,"
               "cur_users,max_users,lease_epoch,registered_at,heartbeat_at) "
               "VALUES(:id,decode(:name,'hex'),:addr,:port,decode(:version,'hex'),:pid,:start,:cur,:max,:epoch,clock_timestamp(),clock_timestamp()) "
               "ON CONFLICT(service_id) DO UPDATE SET name_bytes=EXCLUDED.name_bytes,reported_addr=EXCLUDED.reported_addr,"
               "reported_port=EXCLUDED.reported_port,version_bytes=EXCLUDED.version_bytes,pid=EXCLUDED.pid,start_unix=EXCLUDED.start_unix,"
               "cur_users=EXCLUDED.cur_users,max_users=EXCLUDED.max_users,lease_epoch=EXCLUDED.lease_epoch,"
               "registered_at=EXCLUDED.registered_at,heartbeat_at=EXCLUDED.heartbeat_at "
               "WHERE runtime_control.peer_registry.lease_epoch<EXCLUDED.lease_epoch RETURNING lease_epoch",
            soci::use(id,"id"),soci::use(name,"name"),soci::use(entry.reported_addr,"addr"),soci::use(port,"port"),
            soci::use(version,"version"),soci::use(pid,"pid"),soci::use(start,"start"),soci::use(cur,"cur"),
            soci::use(max,"max"),soci::use(epoch,"epoch"),soci::into(stored);
        if(!sql.got_data()||stored!=epoch)throw std::runtime_error("Newer peer registration exists");
        native_control::Commit(*tx,m_commit_uncertain,m_owner);return;
    }
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
    if(native_control::Enabled(m_pool)) {
        if(m_commit_uncertain)throw std::runtime_error("Peer registry commit outcome unknown; restart required");
        auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=native_control::Begin(sql,m_owner);
        const int id=service_id;const long long epoch=lease_epoch,cur=cur_users,max=max_users;long long found=0;
        sql << "UPDATE runtime_control.peer_registry SET cur_users=:cur,max_users=:max,heartbeat_at=clock_timestamp() "
               "WHERE service_id=:id AND lease_epoch=:epoch RETURNING lease_epoch",
            soci::use(cur,"cur"),soci::use(max,"max"),soci::use(id,"id"),soci::use(epoch,"epoch"),soci::into(found);
        if(!sql.got_data()||found!=epoch)throw std::runtime_error("Peer heartbeat owner changed");
        native_control::Commit(*tx,m_commit_uncertain,m_owner);return;
    }
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
    if(native_control::Enabled(m_pool))throw std::runtime_error("Native peer removal requires an epoch");
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

std::uint64_t SociPeerRepository::ReserveEpoch() {
    if(!native_control::Enabled(m_pool))return 0; // Existing ODBC process-local contract.
    if(m_commit_uncertain)throw std::runtime_error("Peer registry commit outcome unknown; restart required");
    auto lease=m_pool.Acquire();auto tx=native_control::Begin(*lease,m_owner);long long epoch=0;
    (*lease) << "SELECT nextval('runtime_control.peer_epoch')",soci::into(epoch);
    if(epoch<=0)throw std::runtime_error("Peer epoch exhausted");
    native_control::Commit(*tx,m_commit_uncertain,m_owner);return static_cast<std::uint64_t>(epoch);
}
void SociPeerRepository::DeleteIfEpoch(std::uint32_t service_id,std::uint64_t lease_epoch) {
    if(!native_control::Enabled(m_pool)){Delete(service_id);return;}
    if(m_commit_uncertain)throw std::runtime_error("Peer registry commit outcome unknown; restart required");
    auto lease=m_pool.Acquire();auto tx=native_control::Begin(*lease,m_owner);
    const int id=service_id;const long long epoch=lease_epoch;
    (*lease) << "DELETE FROM runtime_control.peer_registry WHERE service_id=:id AND lease_epoch=:epoch",
        soci::use(id,"id"),soci::use(epoch,"epoch");
    native_control::Commit(*tx,m_commit_uncertain,m_owner);
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

    if(native_control::Enabled(m_pool)) {
        if(m_commit_uncertain)throw std::runtime_error("Peer registry commit outcome unknown; restart required");
        auto lease=m_pool.Acquire();auto tx=native_control::Begin(*lease,m_owner);const auto bytes=native_control::Hex(reason);
        (*lease) << "INSERT INTO runtime_control.peer_status_log(group_id,server_id,type_id,old_status,new_status,reason_bytes) "
                    "VALUES(:g,:s,:t,:old,:new,decode(:reason,'hex'))",
            soci::use(ig,"g"),soci::use(is,"s"),soci::use(it,"t"),soci::use(old_s,"old"),soci::use(new_s,"new"),soci::use(bytes,"reason");
        native_control::Commit(*tx,m_commit_uncertain,m_owner);return;
    }

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
    std::lock_guard lock(m_metrics_mutex);
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

    if(native_control::Enabled(m_pool)) {
        if(m_commit_uncertain)throw std::runtime_error("Peer registry commit outcome unknown; restart required");
        auto lease=m_pool.Acquire();auto tx=native_control::Begin(*lease,m_owner);
        const long long sessions=sample.sessions,users=sample.users,active=sample.active;
        (*lease) << "INSERT INTO runtime_control.peer_metrics(group_id,server_id,type_id,sessions,users,active_users) VALUES(:g,:s,:t,:sessions,:users,:active)",
            soci::use(ig,"g"),soci::use(is,"s"),soci::use(it,"t"),soci::use(sessions,"sessions"),soci::use(users,"users"),soci::use(active,"active");
        native_control::Commit(*tx,m_commit_uncertain,m_owner);return;
    }

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
    if(native_control::Enabled(m_pool)) {
        auto lease=m_pool.Acquire();auto tx=native_control::Begin(*lease,m_owner);
        soci::rowset<soci::row> rows=((*lease).prepare <<
            "SELECT service_id,encode(name_bytes,'hex'),reported_addr,reported_port,encode(version_bytes,'hex'),pid,start_unix,cur_users,max_users,lease_epoch,"
            "least(greatest(extract(epoch FROM clock_timestamp()-registered_at),0),86400)::bigint,"
            "least(greatest(extract(epoch FROM clock_timestamp()-heartbeat_at),0),86400)::bigint FROM runtime_control.peer_registry ORDER BY service_id");
        const auto now=std::chrono::steady_clock::now();
        for(const auto& r:rows) {
            RegistryEntry e;e.service_id=r.get<int>(0);e.reported_name=native_control::Bytes(r.get<std::string>(1));
            e.reported_addr=r.get<std::string>(2);e.reported_port=static_cast<std::uint16_t>(r.get<int>(3));
            e.version=native_control::Bytes(r.get<std::string>(4));e.pid=static_cast<std::uint32_t>(r.get<long long>(5));
            e.start_unix=r.get<long long>(6);e.cur_users=static_cast<std::uint32_t>(r.get<long long>(7));
            e.max_users=static_cast<std::uint32_t>(r.get<long long>(8));e.lease_epoch=r.get<long long>(9);
            e.registered_at=now-std::chrono::seconds(r.get<long long>(10));e.last_heartbeat_at=now-std::chrono::seconds(r.get<long long>(11));
            result.push_back(std::move(e));
        }
        tx->commit();return result;
    }
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
    if(status_log_days<0||metrics_days<0)throw std::runtime_error("Negative Control retention");
    if(native_control::Enabled(m_pool)) {
        if(m_commit_uncertain)throw std::runtime_error("Peer registry commit outcome unknown; restart required");
        auto lease=m_pool.Acquire();auto tx=native_control::Begin(*lease,m_owner);
        (*lease) << "DELETE FROM runtime_control.peer_status_log WHERE changed_at<clock_timestamp()-make_interval(days=>:days)",soci::use(status_log_days);
        (*lease) << "DELETE FROM runtime_control.peer_metrics WHERE sampled_at<clock_timestamp()-make_interval(days=>:days)",soci::use(metrics_days);
        native_control::Commit(*tx,m_commit_uncertain,m_owner);return;
    }
    try
    {
        auto lease = m_pool.Acquire();
        auto& sql  = *lease;

        const int status_offset=-status_log_days,metrics_offset=-metrics_days;
        sql << "DELETE FROM TPEER_STATUS_LOG "
               "WHERE dtChangedAt < DATEADD(day, :d, GETDATE())",
            soci::use(status_offset, "d");

        sql << "DELETE FROM TPEER_METRICS "
               "WHERE dtSampledAt < DATEADD(day, :d, GETDATE())",
            soci::use(metrics_offset, "d");

        spdlog::info("soci_peer_repository: purged status_log >{}d, "
                     "metrics >{}d", status_log_days, metrics_days);
    }
    catch (const std::exception& ex)
    {
        spdlog::warn("soci_peer_repository.PurgeOldRows: {}", ex.what());
    }
}

} // namespace tcontrolsvr
