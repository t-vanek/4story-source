#pragma once

// SOCI connection-pool wrapper. Owns N sessions to the configured DB
// backend; service impls acquire a session, run their query, and the
// session returns to the pool on Lease destruction.
//
// Backend-agnostic: same code targets PostgreSQL, MSSQL (via ODBC),
// SQLite. Connection-string format differs per backend — see
// services/db/CONFIG.md for examples.
//
// Thread-safety: the underlying soci::connection_pool serializes
// acquisitions across threads. Each leased session is exclusive to
// the caller until the Lease goes out of scope.
//
// Asio integration: services running inside the io_context use
// `asio::post(thread_pool, [&] { … pool.Acquire() … })` so the
// blocking DB I/O doesn't stall the coroutine reactor. The Pool
// itself is io_context-unaware — just a thread-safe primitive.

#include <soci/soci.h>

#include <cstddef>
#include <memory>
#include <string>

namespace fourstory::db {

enum class Backend : std::uint8_t
{
    PostgreSQL,
    Sqlite3,
    Odbc,       // for MSSQL via msodbcsql18
};

class SessionPool
{
public:
    // Open `pool_size` sessions to the named backend with the given
    // connection string. Throws soci::soci_error on connect failure.
    //
    // Connection string examples (SOCI dialect):
    //   PostgreSQL: "host=localhost port=5432 dbname=tloginsvr_dev
    //                user=tloginsvr password=devpass"
    //   SQLite:     "dbname=/tmp/test.sqlite" or ":memory:"
    //   ODBC:       "DSN=MSSQL_PROD;UID=login;PWD=…"
    SessionPool(Backend backend,
                const std::string& conn_string,
                std::size_t pool_size = 8);

    ~SessionPool();
    SessionPool(const SessionPool&) = delete;
    SessionPool& operator=(const SessionPool&) = delete;

    // RAII handle for a leased session. Auto-releases on destruction.
    class Lease
    {
    public:
        ~Lease();
        Lease(Lease&&) noexcept;
        Lease& operator=(Lease&&) = delete;
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;

        soci::session&       operator*()       { return *m_session; }
        const soci::session& operator*() const { return *m_session; }
        soci::session*       operator->()      { return m_session; }
        const soci::session* operator->() const{ return m_session; }

    private:
        friend class SessionPool;
        Lease(SessionPool* pool, std::size_t idx, soci::session* sess);

        SessionPool*   m_pool;
        std::size_t    m_idx;
        soci::session* m_session; // non-owning view into the pool
    };

    // Acquire a session. Blocks if all sessions are in use.
    Lease Acquire();

    Backend GetBackend() const { return m_backend; }
    std::size_t PoolSize() const { return m_pool_size; }

private:
    Backend                       m_backend;
    std::size_t                   m_pool_size;
    soci::connection_pool         m_pool;
};

// Map enum → SOCI backend factory. Header-public so config code
// can validate backend strings.
Backend ParseBackend(const std::string& s);
const char* BackendName(Backend b);

} // namespace fourstory::db
