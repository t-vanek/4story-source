#pragma once
#include "fourstory/db/session_pool.h"
#include "fourstory/db/native_map_topology.h"
#include <cstdint>
#include <memory>
#include <string>

namespace tmapsvr {
class PostgreSQLMapOwner {
public:
    PostgreSQLMapOwner(const std::string& connection, std::uint8_t world, std::uint8_t server,
                       const std::string& routing_manifest={});
    const std::string& Token() const { return m_token; }
    int BackendPid() const { return m_backend_pid; }
    bool Healthy();
    int OrphanedSessions() const { return m_orphaned; }
    int RecoveredSessions() const { return m_recovered; }
    const fourstory::db::NativeMapTopology& Topology() const {return m_topology;}
private:
    fourstory::db::SessionPool m_control;
    fourstory::db::SessionPool::Lease m_lease;
    std::string m_token;
    fourstory::db::NativeMapTopology m_topology;
    int m_world{},m_server{},m_backend_pid{},m_orphaned{},m_recovered{};
};
std::unique_ptr<soci::transaction> BeginMapTransaction(soci::session& sql,
    int world, int server, const std::string& token, bool repeatable_read=false, bool world_fence=false);
int RecoverPreparedMapTransfers(soci::session&,int world,int server,const std::string& new_token);
}
