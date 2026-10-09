#pragma once
#include "fourstory/db/session_pool.h"
#include <cstdint>
#include <memory>
#include <string>

namespace tmapsvr {
class PostgreSQLMapOwner {
public:
    PostgreSQLMapOwner(const std::string& connection, std::uint8_t world, std::uint8_t server);
    const std::string& Token() const { return m_token; }
    int BackendPid() const { return m_backend_pid; }
    bool Healthy();
    int OrphanedSessions() const { return m_orphaned; }
    int RecoveredSessions() const { return m_recovered; }
private:
    fourstory::db::SessionPool m_control;
    fourstory::db::SessionPool::Lease m_lease;
    std::string m_token;
    int m_world{},m_server{},m_backend_pid{},m_orphaned{},m_recovered{};
};
std::unique_ptr<soci::transaction> BeginMapTransaction(soci::session& sql,
    int world, int server, const std::string& token, bool repeatable_read=false);
int RecoverPreparedMapTransfers(soci::session&,int world,int server,const std::string& new_token);
}
