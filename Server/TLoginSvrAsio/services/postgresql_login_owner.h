#pragma once

#include "fourstory/db/session_pool.h"
#include <memory>
#include <string>

namespace tloginsvr::services {

// One control connection holds the process advisory lock for its lifetime.
// The persisted token also fences application writes if that connection dies
// while other pooled connections from the old process are still alive.
class PostgreSQLLoginOwner
{
public:
    explicit PostgreSQLLoginOwner(const std::string& connection);
    const std::string& Token() const { return m_token; }
    int BackendPid() const { return m_backend_pid; }
    // Call serially, outside the I/O reactor. False requires process shutdown.
    bool Healthy();
private:
    fourstory::db::SessionPool m_control;
    fourstory::db::SessionPool::Lease m_lease;
    std::string m_token;
    int m_backend_pid = 0;
};

// Non-PG backends retain their existing contract. Every native Login writer
// requires an owned token and a row lock lasting for the complete transaction.
std::unique_ptr<soci::transaction> BeginLoginTransaction(
    fourstory::db::SessionPool& pool, soci::session& sql, const std::string& token, bool repeatable_read = false);

// Caller holds the account lock before invoking this selective recovery.
int ExpirePendingHandoff(soci::session& sql, int user_id);

} // namespace tloginsvr::services
