#include "fourstory/db/session_pool.h"

#include <soci/odbc/soci-odbc.h>

#include <spdlog/spdlog.h>

#include <stdexcept>

namespace fourstory::db {

namespace {

const soci::backend_factory& BackendFactory(Backend b)
{
    switch (b)
    {
    case Backend::PostgreSQL:
        // The vcpkg soci[postgresql] feature was dropped because libpq's
        // Windows build is unreliable. Code keeps the enum + dialect
        // branches so re-enabling is a single feature flip in vcpkg.json
        // + restoring the soci-postgresql include here.
        throw std::runtime_error(
            "postgresql backend not compiled in (re-add soci[postgresql] "
            "feature to vcpkg.json to enable)");
    case Backend::Sqlite3:
        // Similarly, sqlite3 isn't in the vcpkg feature set right now —
        // re-enable by adding the feature + restoring the include.
        throw std::runtime_error(
            "sqlite3 backend not compiled in (re-add soci[sqlite3] feature "
            "to vcpkg.json to enable)");
    case Backend::Odbc:       return soci::odbc;
    }
    throw std::runtime_error("unknown DB backend");
}

} // namespace

const char* BackendName(Backend b)
{
    switch (b)
    {
    case Backend::PostgreSQL: return "postgresql";
    case Backend::Sqlite3:    return "sqlite3";
    case Backend::Odbc:       return "odbc";
    }
    return "unknown";
}

Backend ParseBackend(const std::string& s)
{
    if (s == "postgresql" || s == "postgres" || s == "pg") return Backend::PostgreSQL;
    if (s == "sqlite3"    || s == "sqlite")                return Backend::Sqlite3;
    if (s == "odbc"       || s == "mssql")                 return Backend::Odbc;
    throw std::runtime_error("unknown DB backend name: " + s);
}

SessionPool::SessionPool(Backend backend,
                         const std::string& conn_string,
                         std::size_t pool_size)
    : m_backend(backend)
    , m_pool_size(pool_size > 0 ? pool_size : 1)
    , m_pool(m_pool_size)
{
    const auto& factory = BackendFactory(m_backend);
    for (std::size_t i = 0; i < m_pool_size; ++i)
    {
        soci::session& sess = m_pool.at(i);
        sess.open(factory, conn_string);
    }
    spdlog::info("SessionPool ready: backend={} pool_size={}",
        BackendName(m_backend), m_pool_size);
}

SessionPool::~SessionPool() = default;

SessionPool::Lease SessionPool::Acquire()
{
    // SOCI 4.0: lease() blocks if all sessions are in use and returns
    // the leased index. (The deprecated two-arg form took the idx by
    // reference; the modern API returns it directly.)
    const std::size_t idx = m_pool.lease();
    return Lease{ this, idx, &m_pool.at(idx) };
}

SessionPool::Lease::Lease(SessionPool* pool, std::size_t idx, soci::session* sess)
    : m_pool(pool)
    , m_idx(idx)
    , m_session(sess)
{
}

SessionPool::Lease::Lease(Lease&& other) noexcept
    : m_pool(other.m_pool)
    , m_idx(other.m_idx)
    , m_session(other.m_session)
{
    other.m_pool = nullptr;
    other.m_session = nullptr;
}

SessionPool::Lease::~Lease()
{
    if (m_pool != nullptr)
    {
        m_pool->m_pool.give_back(m_idx);
    }
}

} // namespace fourstory::db
