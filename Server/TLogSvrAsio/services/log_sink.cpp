#include "log_sink.h"

#include "retry_queue.h"
#include "audit_bytes.h"
#include <array>

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

#include <algorithm>
#include <utility>
#include "db/schema_validator.h"

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
    if (!db::IsSafeAuditIdentifier(m_table)) throw std::invalid_argument("Invalid audit table identifier");
    if (!opts.drain_batch_size || opts.drain_interval.count() <= 0)
        throw std::invalid_argument("Invalid audit drain options");
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
                 "dropped_queue_full={} outcome_unknown={}",
        m_inserts.load(), m_enqueued.load(),
        m_drained.load(), m_drops_full.load(), m_unknown.load());
}

std::size_t SociLogSink::QueueDepth() const
{
    return m_queue ? m_queue->Size() : 0;
}

namespace {
std::string HexPayload(const std::vector<std::byte>& payload) {
    constexpr char hex[] = "0123456789abcdef";
    std::string out = "\\x";
    for (auto b : payload) {
        const auto v = static_cast<unsigned char>(b);
        out += hex[v >> 4]; out += hex[v & 15];
    }
    return out;
}
}

AuditWriteOutcome SociLogSink::TryInsert(const LogRecord& rec) {
    return TryBulkInsert({rec});
}

AuditWriteOutcome SociLogSink::TryBulkInsert(const std::vector<LogRecord>& batch) {
    if (batch.empty()) return AuditWriteOutcome::Committed;
    // Only failures before acquiring the lease are automatically retryable.
    // All subsequent failures go through the explicit transaction boundary.
    try {
        auto lease = m_pool.Acquire();
        auto& sql = *lease;
        return RunAuditTransaction(sql, [&] {
            const auto backend = m_pool.GetBackend();
            struct Bound {
                long long server, action, format;
                int map;
                std::string blob;
                std::string ip;
                std::array<std::string,7> keys;
                soci::indicator blob_ind;
            };
            std::vector<Bound> bound;
            bound.reserve(batch.size());
            std::string query = "INSERT INTO " + db::AuditRelation(backend, m_table) + " ("
                "LT_LOGDATE,LT_SERVERID,LT_CLIENTIP,LT_ACTION,LT_MAPID,LT_X,LT_Y,LT_Z,"
                "LT_DWKEY1,LT_DWKEY2,LT_DWKEY3,LT_DWKEY4,LT_DWKEY5,LT_DWKEY6,"
                "LT_DWKEY7,LT_DWKEY8,LT_DWKEY9,LT_DWKEY10,LT_DWKEY11,"
                "LT_KEY1,LT_KEY2,LT_KEY3,LT_KEY4,LT_KEY5,LT_KEY6,LT_KEY7,LT_FMT,LT_LOG) VALUES ";
            for (std::size_t row = 0; row < batch.size(); ++row) {
                const auto& r = batch[row];
                std::string blob;
                if (!r.payload.empty()) {
                    if (backend == fourstory::db::Backend::PostgreSQL) blob = HexPayload(r.payload);
                    else blob.assign(reinterpret_cast<const char*>(r.payload.data()), r.payload.size());
                }
                Bound b{}; b.server=r.server_id; b.action=r.action; b.format=r.format; b.map=r.map_id;
                b.blob=std::move(blob); b.blob_ind=r.payload.empty()?soci::i_null:soci::i_ok;
                b.ip=backend==fourstory::db::Backend::PostgreSQL ? "\\x"+AuditHex(r.client_ip) : r.client_ip;
                for(std::size_t n=0;n<7;++n)
                    b.keys[n]=backend==fourstory::db::Backend::PostgreSQL ? "\\x"+AuditHex(r.search_str[n]) : r.search_str[n];
                bound.push_back(std::move(b));
                if (row) query += ',';
                query += '(';
                for (unsigned col = 0; col < 28; ++col) {
                    if (col) query += ',';
                    const auto name = ":p" + std::to_string(row * 28 + col);
                    if ((col == 2 || (col >= 19 && col <= 25) || col == 27) && backend == fourstory::db::Backend::PostgreSQL)
                        query += "CAST(" + name + " AS bytea)";
                    else if (col == 27 && backend == fourstory::db::Backend::Odbc)
                        query += "CONVERT(VARBINARY(512), " + name + ")";
                    else query += name;
                }
                query += ')';
            }
            soci::statement statement(sql);
            statement.alloc(); statement.prepare(query);
            for (std::size_t row = 0; row < batch.size(); ++row) {
                const auto& r = batch[row]; auto& b = bound[row];
                statement.exchange(soci::use(r.timestamp_iso));
                statement.exchange(soci::use(b.server));
                statement.exchange(soci::use(b.ip));
                statement.exchange(soci::use(b.action));
                statement.exchange(soci::use(b.map));
                statement.exchange(soci::use(r.pos_x));
                statement.exchange(soci::use(r.pos_y));
                statement.exchange(soci::use(r.pos_z));
                for (const auto& key : r.search_int) statement.exchange(soci::use(key));
                for (const auto& key : b.keys) statement.exchange(soci::use(key));
                statement.exchange(soci::use(b.format));
                statement.exchange(soci::use(b.blob, b.blob_ind));
            }
            statement.define_and_bind(); statement.execute(true);
        });
    } catch (const fourstory::db::AcquireTimeout&) {
        return AuditWriteOutcome::Retryable;
    }
}

void SociLogSink::SetWorkerPool(boost::asio::thread_pool* pool) {
    m_worker_pool = pool;
}

void SociLogSink::WriteNow(const LogRecord& rec) {
    std::lock_guard lock(m_write_mutex);
    if (m_queue->Empty()) {
        const auto outcome = TryInsert(rec);
        if (outcome == AuditWriteOutcome::Committed) { ++m_inserts; return; }
        if (outcome == AuditWriteOutcome::Unknown) {
            ++m_unknown;
            spdlog::error("log_sink: audit outcome unknown; 1 record will not be retried");
            return;
        }
        // Deliberately omit backend exception text: it can contain bound
        // account names, raw payloads, SQL and connection credentials.
        spdlog::warn("log_sink: audit write not committed; buffering for retry");
    }
    if (m_queue->PushBack(rec)) ++m_enqueued;
    else ++m_drops_full;
}

void SociLogSink::Write(const LogRecord& rec) {
    if (m_worker_pool) boost::asio::post(*m_worker_pool, [this, rec] { WriteNow(rec); });
    else WriteNow(rec);
}

void SociLogSink::DrainPending() {
    std::lock_guard lock(m_write_mutex);
    std::vector<LogRecord> batch;
    // Bound parameters to 2016: below SQL Server's 2100 limit as well as
    // PostgreSQL's. Larger configured drains continue on subsequent ticks.
    const auto size = std::min<std::size_t>(m_opts.drain_batch_size, 72);
    batch.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
        LogRecord rec;
        if (!m_queue->PopFront(rec)) break;
        batch.push_back(std::move(rec));
    }
    if (batch.empty()) return;
    const auto outcome = TryBulkInsert(batch);
    if (outcome == AuditWriteOutcome::Committed) {
        m_drained.fetch_add(batch.size());
    } else if (outcome == AuditWriteOutcome::Unknown) {
        m_unknown.fetch_add(batch.size());
        spdlog::error("log_sink: audit outcome unknown; {} records will not be retried", batch.size());
    } else {
        for (auto it = batch.rbegin(); it != batch.rend(); ++it) m_queue->PushFront(std::move(*it));
    }
}

void SociLogSink::StartDrainLoop(boost::asio::io_context& io) {
    using namespace boost::asio;
    co_spawn(io, [this, &io]() -> awaitable<void> {
        steady_timer timer(io);
        for (;;) {
            timer.expires_after(m_opts.drain_interval);
            boost::system::error_code ec;
            co_await timer.async_wait(redirect_error(use_awaitable, ec));
            if (ec) co_return;
            if (m_queue->Empty() || m_drain_pending.exchange(true)) continue;
            auto drain = [this] {
                // Always release the scheduling guard, including exceptional
                // local failures, without turning them into blind write retries.
                struct Reset { std::atomic<bool>& flag; ~Reset() { flag = false; } } reset{m_drain_pending};
                DrainPending();
            };
            if (m_worker_pool) post(*m_worker_pool, std::move(drain));
            else drain();
        }
    }, detached);
}

void StdoutLogSink::Write(const LogRecord& rec)
{
    spdlog::info("audit action=0x{:04X} server={} uid={} ip={} key='{}' '{}'",
        rec.action, rec.server_id, rec.search_int[0], rec.client_ip,
        rec.search_str[0], rec.search_str[4]);
}

} // namespace tlogsvr
