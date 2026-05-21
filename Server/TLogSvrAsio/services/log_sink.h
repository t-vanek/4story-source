#pragma once

// LogSink — abstracts the destination of a parsed _LOG_DATA_ record.
//   * SociLogSink — INSERTs into TLOG_AUDIT via SOCI (production),
//                   with a RAM-buffered retry queue + drain loop for
//                   transient DB outages (mirrors the legacy
//                   `m_listReadCompleted` requeue behavior).
//   * StdoutLogSink — pretty-prints to spdlog (dev / fallback when DB
//                     is down or no [database] is configured).
//
// The wire decoder feeds whichever sink is wired; switching is a
// main.cpp wire-up change with no impact on the receive path.

#include "fourstory/db/session_pool.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace boost::asio {
class io_context;
class thread_pool;
} // namespace boost::asio

namespace tlogsvr {

// Decoded _LOG_DATA_ matching the legacy LogPacket.h layout
// (same struct UdpAuditLogger in TLoginSvrAsio emits). All fields
// owned by the value; opaque payload after the search keys is a
// variable-length blob (LF_CHARBASE, LF_ITEM, LF_SKILL, LF_PET).
struct LogRecord
{
    std::string                  timestamp_iso;  // YYYY-MM-DD HH:MM:SS
    std::uint32_t                server_id    = 0;
    std::string                  client_ip;
    std::uint32_t                action       = 0;
    std::uint16_t                map_id       = 0;
    std::int32_t                 pos_x        = 0;
    std::int32_t                 pos_y        = 0;
    std::int32_t                 pos_z        = 0;
    std::int64_t                 search_int[11]{};
    std::string                  search_str[7];
    std::uint32_t                format       = 0;
    std::vector<std::byte>       payload;        // 0..512 raw bytes
};

class ILogSink
{
public:
    virtual ~ILogSink() = default;
    // Persist the record. Synchronous from the caller's perspective;
    // SociLogSink may move the record onto its internal retry queue
    // when the DB is unavailable, in which case the actual INSERT is
    // deferred to the drain loop.
    virtual void Write(const LogRecord& rec) = 0;
};

// Forward decl — defined in retry_queue.h, included only in the .cpp.
class RetryQueue;

// SOCI-backed sink writing to LT_* columns in `target_table`, with a
// bounded retry buffer for transient DB outages.
class SociLogSink : public ILogSink
{
public:
    struct Options
    {
        // Max records held in the retry queue when the DB is
        // unavailable. Matches the legacy bounded IO pool
        // (`MAX_IO_CONTEXT = 1000`). 0 disables queueing entirely —
        // failed records are dropped immediately (counter-bumped).
        std::size_t max_retry_queue = 1000;

        // How often the drain loop wakes up to try flushing pending
        // records back to the DB. Legacy WorkTickProc tick = 30 s.
        // Smaller intervals trade CPU for faster recovery.
        std::chrono::seconds drain_interval = std::chrono::seconds(30);

        // Per-tick cap on the number of records the drain loop tries
        // to flush in one pass. Keeps a 1000-record backlog from
        // monopolizing the io_context on a single timer fire.
        std::size_t drain_batch_size = 64;
    };

    // Two overloads (rather than a default arg) — GCC rejects
    // `Options opts = Options{}` here because the NSDMIs on Options
    // aren't yet visible inside the enclosing class declaration.
    SociLogSink(fourstory::db::SessionPool& pool,
                std::string target_table);
    SociLogSink(fourstory::db::SessionPool& pool,
                std::string target_table,
                Options opts);
    ~SociLogSink() override;
    SociLogSink(const SociLogSink&) = delete;
    SociLogSink& operator=(const SociLogSink&) = delete;

    // Spawn the drain coroutine on `io`. Must be called once after
    // construction if the operator wants buffered records flushed —
    // if you skip this, Write() still works but failed records stay
    // in the queue until destruction (and are logged then).
    void StartDrainLoop(boost::asio::io_context& io);

    // Opt into off-loop INSERTs: every Write() call's SOCI work is
    // posted to `pool` instead of running on the caller's thread.
    // Use a single-thread pool to preserve FIFO ordering between
    // arriving datagrams; multi-threaded pools work but may reorder
    // INSERTs (the retry-queue + drain semantics still hold so this
    // is safe, just no longer per-source-IP ordered).
    //
    // Must be called before the receive loop spawns. The pool must
    // outlive this sink — typically a member of the same main()
    // scope. Pass nullptr (or skip the call entirely) to keep the
    // legacy in-line behavior on the caller's executor.
    void SetWorkerPool(boost::asio::thread_pool* pool);

    void Write(const LogRecord& rec) override;

    // Counters for ops + shutdown summary. Cheap, lock-free reads.
    std::uint64_t Inserts() const          { return m_inserts.load(); }
    std::uint64_t EnqueuedOnError() const  { return m_enqueued.load(); }
    std::uint64_t DroppedQueueFull() const { return m_drops_full.load(); }
    std::uint64_t DrainedAfterRetry() const{ return m_drained.load(); }
    std::size_t   QueueDepth() const;

private:
    // Run one INSERT against the DB. Returns true on success, false
    // if the operation hit a transient failure (connection lost,
    // pool exhausted, …) the caller should retry later. Permanent
    // errors (constraint violations, schema drift) are also reported
    // as false here — audit logs have no foreign keys and the boot
    // validator already caught the schema, so any exception is
    // treated as transient.
    bool TryInsert(const LogRecord& rec);

    // Bulk INSERT for the drain path. Builds a single multi-row
    // `INSERT … VALUES (…), (…), …` statement with dialect-aware BLOB
    // literal encoding (MSSQL `0xHEX`, PG `'\xHEX'::bytea`, SQLite
    // `X'HEX'`). On a hot drain after DB outage, 64 records → 1 wire
    // round-trip instead of 64. Caller treats `false` / exceptions
    // the same way as TryInsert — push the whole batch back to the
    // queue head and wait for the next tick. Empty batch returns true.
    bool TryBulkInsert(const std::vector<LogRecord>& batch);

    fourstory::db::SessionPool&  m_pool;
    std::string                  m_table;
    Options                      m_opts;
    std::unique_ptr<RetryQueue>  m_queue;
    // Optional off-loop worker pool. nullptr = legacy behavior:
    // INSERTs run synchronously on the caller's thread (typically
    // the receive coroutine). Set via SetWorkerPool. Non-owning.
    boost::asio::thread_pool*    m_worker_pool = nullptr;

    std::atomic<std::uint64_t>   m_inserts{0};
    std::atomic<std::uint64_t>   m_enqueued{0};
    std::atomic<std::uint64_t>   m_drops_full{0};
    std::atomic<std::uint64_t>   m_drained{0};
};

// Dev fallback sink — emits each record through spdlog. Useful when
// no DB is configured (audit lands on stderr).
class StdoutLogSink : public ILogSink
{
public:
    void Write(const LogRecord& rec) override;
};

} // namespace tlogsvr
