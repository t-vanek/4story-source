#pragma once

// DbContext — Entity-Framework-style session coordinator.
//
// Acquires one SOCI session from the pool and provides:
//   Set<T>()         — Repository<T> for entity CRUD
//   Exec(sp_name)    — SpCall builder for stored procedures
//   Transaction()    — RAII transaction scope
//
// The session is released back to the pool when DbContext is destroyed.
// DbContext is not coroutine-safe — construct one per synchronous call
// and wrap with CoOffloadIf when calling from the io_context.
//
// Usage:
//
//   // Direct (blocking, OK on worker thread):
//   DbContext ctx(pool);
//   auto chars = ctx.Set<Character>().Where("dwUserID = " + std::to_string(uid));
//   auto login = ctx.Exec("TLogin")
//       .In("szUserID", user_id)
//       .In("szPasswd", password)
//       .Out<int>("dwKEY")
//       .WithReturn()
//       .Execute(ctx.Session());
//
//   // Async (on io_context coroutine):
//   auto chars = co_await fourstory::db::CoOffloadIf(db_pool,
//       [&pool, uid] {
//           DbContext ctx(pool);
//           return ctx.Set<Character>().Where("dwUserID = " + std::to_string(uid));
//       });

#include "repository.h"
#include "sp_call.h"
#include "transaction.h"
#include "fourstory/db/session_pool.h"

#include <string>
#include <utility>

namespace fourstory::db::orm {

class DbContext
{
public:
    explicit DbContext(fourstory::db::SessionPool& pool)
        : m_lease(pool.Acquire())
        , m_sql(*m_lease)
    {}

    // Direct access to the underlying SOCI session (for raw queries,
    // SpCall::Execute, manual SOCI use when Repository doesn't fit).
    soci::session& Session() { return m_sql; }

    // Entity CRUD — returns a Repository<T> bound to this session.
    template<typename T>
    Repository<T> Set()
    {
        return Repository<T>(m_sql);
    }

    // SP call builder — chain .In/.Out/.WithReturn().Execute(ctx.Session()).
    SpCall Exec(std::string sp_name)
    {
        return SpCall(std::move(sp_name));
    }

    // Convenience shortcut that calls Execute immediately on this session.
    SpResult ExecNow(SpCall call)
    {
        return call.Execute(m_sql);
    }

    // Begin a RAII transaction on this session.
    Transaction BeginTransaction()
    {
        return Transaction(m_sql);
    }

private:
    fourstory::db::SessionPool::Lease m_lease;
    soci::session&                    m_sql;
};

} // namespace fourstory::db::orm
