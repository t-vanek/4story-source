#include "log_sink.h"

#include "retry_queue.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <functional>
#include <utility>

#include <utility>

namespace tlogsvr {

SociLogSink::SociLogSink(fourstory::db::SessionPool& pool,
                         std::string target_table)
    : SociLogSink(pool, std::move(target_table), Options{})
{
}

SociLogSink::SociLogSink(fourstory::db::SessionPool& pool,
                         std::string target_table,
                         Options opts)
    : m_pool(pool)
    , m_table(std::move(target_table))
    , m_opts(opts)
    , m_queue(std::make_unique<RetryQueue>(opts.max_retry_queue))
{
}

SociLogSink::~SociLogSink()
{
    const auto depth = m_queue ? m_queue->Size() : 0;
    if (depth > 0)
    {
        // Log loudly — anything still in the queue at shutdown is
        // genuinely lost (no on-disk spool). Operators relying on
        // audit completeness should treat this as a P2 incident.
        spdlog::warn("log_sink: shutting down with {} record(s) "
                     "still in retry queue — these are not persisted",
                     depth);
    }
    spdlog::info("log_sink: totals inserts={} enqueued={} drained={} "
                 "dropped_queue_full={}",
        m_inserts.load(), m_enqueued.load(),
        m_drained.load(), m_drops_full.load());
}

std::size_t SociLogSink::QueueDepth() const
{
    return m_queue ? m_queue->Size() : 0;
}

bool SociLogSink::TryInsert(const LogRecord& rec)
{
    fourstory::db::SessionPool::Lease lease = [&]() {
        // Acquire can throw AcquireTimeout on pool saturation; treat
        // it as a transient failure (same as a thrown query error
        // below) so we don't crash the receive coroutine.
        try { return m_pool.Acquire(); }
        catch (const std::exception&) { throw; }
    }();
    soci::session& sql = *lease;

    // LT_LOG binding (F6 in SQL_AUDIT) — VARCHAR→binary conversion
    // differs by backend. ODBC/MSSQL needs CONVERT(VARBINARY(512), …);
    // PostgreSQL accepts the bind via an explicit `::bytea` cast.
    // SQLite takes the std::string verbatim as a BLOB.
    const char* blob_expr = ":blob";
    switch (m_pool.GetBackend())
    {
        case fourstory::db::Backend::Odbc:
            blob_expr = "CONVERT(VARBINARY(512), :blob)";
            break;
        case fourstory::db::Backend::PostgreSQL:
            blob_expr = "CAST(:blob AS bytea)";
            break;
        case fourstory::db::Backend::Sqlite3:
            blob_expr = ":blob";
            break;
    }
    // Build INSERT — column list is the modern TLOG_AUDIT schema
    // (see schema/tlog-audit.sql). Table name is interpolated as a
    // constant from config (no user input), parameters all bound
    // through SOCI.
    const std::string q =
        "INSERT INTO \"" + m_table + "\" "
        "(LT_LOGDATE, LT_SERVERID, LT_CLIENTIP, LT_ACTION, LT_MAPID, "
        " LT_X, LT_Y, LT_Z, "
        " LT_DWKEY1, LT_DWKEY2, LT_DWKEY3, LT_DWKEY4, LT_DWKEY5, "
        " LT_DWKEY6, LT_DWKEY7, LT_DWKEY8, LT_DWKEY9, LT_DWKEY10, LT_DWKEY11, "
        " LT_KEY1, LT_KEY2, LT_KEY3, LT_KEY4, LT_KEY5, LT_KEY6, LT_KEY7, "
        " LT_FMT, LT_LOG) "
        "VALUES (:ts, :sid, :ip, :act, :mid, :x, :y, :z, "
        " :k1, :k2, :k3, :k4, :k5, :k6, :k7, :k8, :k9, :k10, :k11, "
        " :s1, :s2, :s3, :s4, :s5, :s6, :s7, :fmt, " +
        std::string(blob_expr) + ")";

    const int sid = static_cast<int>(rec.server_id);
    const int act = static_cast<int>(rec.action);
    const int mid = static_cast<int>(rec.map_id);
    const int fmt = static_cast<int>(rec.format);
    // payload as std::string for SOCI VARBINARY binding (binary-safe
    // because std::string stores arbitrary bytes; driver maps to the
    // backend's binary column type per the dialect branch above).
    // Empty payload binds as SQL NULL — MSSQL rejects empty-string
    // → VARBINARY implicit conversion otherwise.
    std::string blob;
    soci::indicator blob_ind = soci::i_null;
    if (!rec.payload.empty())
    {
        blob.assign(reinterpret_cast<const char*>(rec.payload.data()),
                    rec.payload.size());
        blob_ind = soci::i_ok;
    }

    sql << q,
        soci::use(rec.timestamp_iso),
        soci::use(sid),
        soci::use(rec.client_ip),
        soci::use(act),
        soci::use(mid),
        soci::use(rec.pos_x),
        soci::use(rec.pos_y),
        soci::use(rec.pos_z),
        soci::use(rec.search_int[0]),
        soci::use(rec.search_int[1]),
        soci::use(rec.search_int[2]),
        soci::use(rec.search_int[3]),
        soci::use(rec.search_int[4]),
        soci::use(rec.search_int[5]),
        soci::use(rec.search_int[6]),
        soci::use(rec.search_int[7]),
        soci::use(rec.search_int[8]),
        soci::use(rec.search_int[9]),
        soci::use(rec.search_int[10]),
        soci::use(rec.search_str[0]),
        soci::use(rec.search_str[1]),
        soci::use(rec.search_str[2]),
        soci::use(rec.search_str[3]),
        soci::use(rec.search_str[4]),
        soci::use(rec.search_str[5]),
        soci::use(rec.search_str[6]),
        soci::use(fmt),
        soci::use(blob, blob_ind);
    return true;
}

namespace {

// Standard SQL single-quote escape — '  →  ''.
std::string EscapeSqlString(std::string_view s)
{
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s)
    {
        if (c == '\'') out += "''";
        else           out += c;
    }
    return out;
}

// Hex-encode a binary payload for dialect-specific embedded literal.
std::string ToHex(const std::vector<std::byte>& payload)
{
    static const char k_hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(payload.size() * 2);
    for (auto b : payload)
    {
        const auto v = static_cast<std::uint8_t>(b);
        out.push_back(k_hex[(v >> 4) & 0xF]);
        out.push_back(k_hex[v & 0xF]);
    }
    return out;
}

// Dialect-specific binary literal:
//   MSSQL  : 0xDEADBEEF
//   PG     : '\xDEADBEEF'::bytea
//   SQLite : X'DEADBEEF'
// Empty payload → SQL NULL keyword (no quotes).
std::string EncodeBlobLiteral(const std::vector<std::byte>& payload,
                               fourstory::db::Backend backend)
{
    if (payload.empty()) return "NULL";
    const auto hex = ToHex(payload);
    switch (backend)
    {
        case fourstory::db::Backend::Odbc:
            return "0x" + hex;
        case fourstory::db::Backend::PostgreSQL:
            return "'\\x" + hex + "'::bytea";
        case fourstory::db::Backend::Sqlite3:
            return "X'" + hex + "'";
    }
    return "NULL";
}

// One "(v1, v2, ...)" row for the bulk VALUES list. Order matches the
// INSERT column list in TryBulkInsert below.
void AppendRow(std::string& sql,
               const LogRecord& r,
               fourstory::db::Backend backend)
{
    sql += '(';
    sql += "'"; sql += EscapeSqlString(r.timestamp_iso); sql += "', ";
    sql += std::to_string(static_cast<int>(r.server_id)); sql += ", ";
    sql += "'"; sql += EscapeSqlString(r.client_ip); sql += "', ";
    sql += std::to_string(static_cast<int>(r.action)); sql += ", ";
    sql += std::to_string(static_cast<int>(r.map_id)); sql += ", ";
    sql += std::to_string(r.pos_x); sql += ", ";
    sql += std::to_string(r.pos_y); sql += ", ";
    sql += std::to_string(r.pos_z); sql += ", ";
    for (int i = 0; i < 11; ++i)
    {
        sql += std::to_string(r.search_int[i]); sql += ", ";
    }
    for (int i = 0; i < 7; ++i)
    {
        sql += "'"; sql += EscapeSqlString(r.search_str[i]); sql += "', ";
    }
    sql += std::to_string(static_cast<int>(r.format)); sql += ", ";
    sql += EncodeBlobLiteral(r.payload, backend);
    sql += ')';
}

} // namespace

bool SociLogSink::TryBulkInsert(const std::vector<LogRecord>& batch)
{
    if (batch.empty()) return true;

    // For very large batches we'd split per dialect parameter limits;
    // SociLogSink's drain_batch_size caps at 64 by default, well under
    // any backend's per-statement limit even with embedded literals.
    const auto backend = m_pool.GetBackend();

    std::string sql;
    sql.reserve(256 + batch.size() * 512);
    sql += "INSERT INTO \"";
    sql += m_table;
    sql += "\" ("
        "LT_LOGDATE, LT_SERVERID, LT_CLIENTIP, LT_ACTION, LT_MAPID, "
        "LT_X, LT_Y, LT_Z, "
        "LT_DWKEY1, LT_DWKEY2, LT_DWKEY3, LT_DWKEY4, LT_DWKEY5, "
        "LT_DWKEY6, LT_DWKEY7, LT_DWKEY8, LT_DWKEY9, LT_DWKEY10, LT_DWKEY11, "
        "LT_KEY1, LT_KEY2, LT_KEY3, LT_KEY4, LT_KEY5, LT_KEY6, LT_KEY7, "
        "LT_FMT, LT_LOG) VALUES ";
    for (std::size_t i = 0; i < batch.size(); ++i)
    {
        if (i > 0) sql += ", ";
        AppendRow(sql, batch[i], backend);
    }

    auto lease = m_pool.Acquire();
    soci::session& s = *lease;
    // One round-trip for the whole batch. Wrapped in transaction so a
    // partial failure doesn't leave half the batch persisted (caller
    // pushes the entire batch back to the queue head on exception).
    soci::transaction tx(s);
    s << sql;
    tx.commit();
    return true;
}

namespace {
// Body of the SOCI-INSERT-then-fall-back-to-queue logic. Shared
// between the in-line path and the worker-pool path so behavior
// stays identical regardless of which thread runs it.
void RunInsertOrQueue(SociLogSink& self,
                      LogRecord rec,
                      RetryQueue& queue,
                      std::atomic<std::uint64_t>& inserts,
                      std::atomic<std::uint64_t>& enqueued,
                      std::atomic<std::uint64_t>& drops_full,
                      std::function<bool(const LogRecord&)> try_insert)
{
    (void)self;
    // If we already have a backlog the drain loop is working through,
    // don't interleave a fresh INSERT — keep ordering and put the
    // new record on the tail. The drain loop picks it up in turn
    // once the DB recovers.
    if (!queue.Empty())
    {
        if (queue.PushBack(rec)) enqueued.fetch_add(1);
        else                     drops_full.fetch_add(1);
        return;
    }
    try
    {
        if (try_insert(rec))
        {
            inserts.fetch_add(1);
            return;
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::warn("log_sink: INSERT failed (action={} uid={} ip={}) "
                     "— buffering for retry: {}",
            rec.action, rec.search_int[0], rec.client_ip, ex.what());
    }
    if (queue.PushBack(std::move(rec))) enqueued.fetch_add(1);
    else                                drops_full.fetch_add(1);
}
} // namespace

void SociLogSink::SetWorkerPool(boost::asio::thread_pool* pool)
{
    m_worker_pool = pool;
}

void SociLogSink::Write(const LogRecord& rec)
{
    // Off-loop path: post the INSERT work to the worker pool so the
    // caller's executor (typically the UDP receive coroutine on the
    // io_context) doesn't block on DB latency. A single-thread pool
    // preserves FIFO ordering; multi-thread pools may reorder but
    // the retry queue still maintains correctness.
    if (m_worker_pool != nullptr)
    {
        boost::asio::post(*m_worker_pool,
            [this, rec]() mutable {
                RunInsertOrQueue(*this, std::move(rec), *m_queue,
                    m_inserts, m_enqueued, m_drops_full,
                    [this](const LogRecord& r) { return TryInsert(r); });
            });
        return;
    }

    // Legacy in-line path: caller's thread runs SOCI directly.
    RunInsertOrQueue(*this, rec, *m_queue,
        m_inserts, m_enqueued, m_drops_full,
        [this](const LogRecord& r) { return TryInsert(r); });
}

void SociLogSink::StartDrainLoop(boost::asio::io_context& io)
{
    using namespace boost::asio;
    co_spawn(io,
        [this, &io]() -> awaitable<void> {
            steady_timer timer(io);
            while (true)
            {
                timer.expires_after(m_opts.drain_interval);
                boost::system::error_code ec;
                co_await timer.async_wait(redirect_error(use_awaitable, ec));
                // io.stop() cancels the timer → ec=operation_aborted;
                // any other error → exit too.
                if (ec) co_return;

                if (m_queue->Empty()) continue;

                // Drain in bulk: pop up to drain_batch_size records,
                // attempt one multi-row INSERT. On any failure (DB
                // still down, schema drift, etc.) push the ENTIRE
                // batch back to the queue head in original order and
                // wait for the next tick. Single round-trip per tick
                // when healthy → 50× fewer DB calls during recovery
                // after a DB outage compared to the per-row path.
                std::vector<LogRecord> batch;
                batch.reserve(m_opts.drain_batch_size);
                for (std::size_t i = 0; i < m_opts.drain_batch_size; ++i)
                {
                    LogRecord rec{};
                    if (!m_queue->PopFront(rec)) break;
                    batch.push_back(std::move(rec));
                }
                if (batch.empty()) continue;

                std::size_t drained_this_tick = 0;
                bool bulk_ok = false;
                try
                {
                    bulk_ok = TryBulkInsert(batch);
                }
                catch (const std::exception& ex)
                {
                    spdlog::warn("log_sink: bulk drain INSERT failed "
                                 "(batch={}) — will retry next tick: {}",
                        batch.size(), ex.what());
                    bulk_ok = false;
                }

                if (bulk_ok)
                {
                    drained_this_tick = batch.size();
                    m_drained.fetch_add(batch.size());
                }
                else
                {
                    // Push back to the head in REVERSE order so the
                    // original FIFO sequence is preserved (last popped
                    // = first to push-front, etc.).
                    for (auto it = batch.rbegin(); it != batch.rend(); ++it)
                        m_queue->PushFront(std::move(*it));
                }

                if (drained_this_tick > 0)
                {
                    spdlog::info("log_sink: bulk drained {} record(s); "
                                 "queue_depth={} remaining",
                        drained_this_tick, m_queue->Size());
                }
            }
        },
        detached);
}

void StdoutLogSink::Write(const LogRecord& rec)
{
    spdlog::info("audit action=0x{:04X} server={} uid={} ip={} key='{}' '{}'",
        rec.action, rec.server_id, rec.search_int[0], rec.client_ip,
        rec.search_str[0], rec.search_str[4]);
}

} // namespace tlogsvr
