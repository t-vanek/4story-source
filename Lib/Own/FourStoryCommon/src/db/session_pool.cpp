#include "fourstory/db/session_pool.h"

#include <soci/odbc/soci-odbc.h>
#ifdef FOURSTORY_HAS_POSTGRESQL
#include <soci/postgresql/soci-postgresql.h>
#endif

#include <spdlog/spdlog.h>

#include <stdexcept>
#include <charconv>
#include <memory>
#include <string_view>

namespace fourstory::db {

namespace {

const soci::backend_factory& BackendFactory(Backend b)
{
    switch (b)
    {
    case Backend::PostgreSQL:
#ifdef FOURSTORY_HAS_POSTGRESQL
        return soci::postgresql;
#else
        throw std::runtime_error(
            "PostgreSQL backend disabled; enable FOURSTORY_ENABLE_POSTGRESQL "
            "and install the SOCI PostgreSQL backend");
#endif
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

#ifdef FOURSTORY_HAS_POSTGRESQL
std::string PostgreSQLConnectionOptions(const std::string& input)
{
    // libpq parses keyword strings and URIs. Parser errors can contain
    // supplied secrets, so only fixed diagnostics may leave this boundary.
    char* raw_error = nullptr;
    std::unique_ptr<PQconninfoOption, decltype(&PQconninfoFree)> options(
        PQconninfoParse(input.c_str(), &raw_error), &PQconninfoFree);
    std::unique_ptr<char, decltype(&PQfreemem)> error(raw_error, &PQfreemem);
    if (!options)
        throw ConnectionError("Invalid PostgreSQL connection options");
    std::string result;
    bool has_timeout = false, has_tls = false;
    for (auto* option = options.get(); option->keyword; ++option)
    {
        if (!option->val || !*option->val) continue;
        const std::string_view key(option->keyword), value(option->val);
        if (key == "connect_timeout")
        {
            int seconds = 0;
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), seconds);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()
                || seconds < 1 || seconds > 300)
                throw ConnectionError("PostgreSQL connect_timeout must be between 1 and 300 seconds");
            has_timeout = true;
        }
        if (key == "sslmode")
        {
            if (value != "verify-full" && value != "verify-ca" && value != "require" && value != "disable")
                throw ConnectionError("PostgreSQL sslmode must explicitly require TLS or disable it");
            has_tls = true;
        }
        result += std::string(key) + "='";
        for (char c : value)
        {
            if (c == '\\' || c == '\'') result += '\\';
            result += c;
        }
        result += "' ";
    }
    if (!has_timeout) result += "connect_timeout=5 ";
    if (!has_tls) result += "sslmode=verify-full ";
    return result;
}
#endif

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
                         std::size_t pool_size,
                         std::chrono::milliseconds default_acquire_timeout)
    : m_backend(backend)
    , m_pool_size(pool_size > 0 ? pool_size : 1)
    , m_default_acquire_timeout(default_acquire_timeout)
    , m_pool(m_pool_size)
{
    const auto& factory = BackendFactory(m_backend);
    std::string options = conn_string;
#ifdef FOURSTORY_HAS_POSTGRESQL
    if (m_backend == Backend::PostgreSQL)
        options = PostgreSQLConnectionOptions(conn_string);
#endif
    for (std::size_t i = 0; i < m_pool_size; ++i)
    {
        soci::session& sess = m_pool.at(i);
        try
        {
            sess.open(factory, options);
        }
        catch (const soci::soci_error&)
        {
            // Caller-side startup logging must not disclose backend options.
            // Failed connections are surfaced; writes are never auto-retried.
            throw ConnectionError(std::string("Database connection failed (backend=")
                                  + BackendName(m_backend) + ")");
        }
    }
    spdlog::info("SessionPool ready: backend={} pool_size={} acquire_timeout_ms={}",
        BackendName(m_backend), m_pool_size,
        static_cast<long long>(m_default_acquire_timeout.count()));
}

SessionPool::~SessionPool() = default;

SessionPool::Lease SessionPool::Acquire()
{
    return Acquire(m_default_acquire_timeout);
}

SessionPool::Lease SessionPool::Acquire(std::chrono::milliseconds timeout)
{
    std::size_t idx = 0;
    if (timeout.count() <= 0)
    {
        // Legacy blocking behavior — kept for tests that intentionally
        // want to deadlock on an exhausted pool. Production deploys
        // should always pass a positive timeout.
        idx = m_pool.lease();
        return Lease{ this, idx, &m_pool.at(idx) };
    }

    // SOCI 4.0 try_lease(pos, timeout_ms) returns false if no session
    // became available within the deadline. We split the wait into
    // 100ms chunks so the calling io_context thread can still field
    // ctrl-C / io.stop() in a bounded time when the pool is exhausted
    // under steady load. (Services call Acquire() synchronously from
    // their coroutines, so the calling reactor thread does block for
    // each chunk — a longer chunk would make signal response laggy.)
    using clock = std::chrono::steady_clock;
    const auto deadline = clock::now() + timeout;
    constexpr int kChunkMs = 100;

    for (;;)
    {
        const auto now = clock::now();
        if (now >= deadline)
        {
            throw AcquireTimeout(
                "SessionPool::Acquire timed out after "
                + std::to_string(timeout.count())
                + "ms (pool_size=" + std::to_string(m_pool_size)
                + ", backend=" + BackendName(m_backend) + ")");
        }
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - now).count();
        const int wait_ms = static_cast<int>(
            remaining < kChunkMs ? remaining : kChunkMs);
        if (m_pool.try_lease(idx, wait_ms))
            return Lease{ this, idx, &m_pool.at(idx) };
    }
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
