#pragma once

// RetryQueue — bounded thread-safe FIFO of LogRecord values held in
// RAM while the audit DB is unavailable. Mirrors the legacy
// `m_listReadCompleted` requeue behavior in
// `Server/TLogSvr/CUdpSocket.cpp::ReadPacket`: failed records sit
// here until the drain loop walks them back to the DB.
//
// Cap policy matches legacy receive-side drop (`TMemPool<_UDPPACKET>`
// returning null when MAX_IO_CONTEXT=1000 is exhausted): on overflow
// the NEW arrival is rejected, existing records keep their slot.
// This favors the records that have already been waiting — the
// alternative (ring-buffer drop-oldest) would lose the records most
// likely about to succeed when the DB comes back.
//
// The queue is intentionally a separate class so its semantics are
// unit-testable without touching SOCI or the network layer.

#include "log_sink.h"

#include <cstddef>
#include <deque>
#include <mutex>

namespace tlogsvr {

class RetryQueue
{
public:
    explicit RetryQueue(std::size_t capacity);

    // Push a record onto the tail. Returns true if accepted, false if
    // the cap was hit (in which case `rec` is dropped on the caller
    // side — the queue keeps existing entries).
    bool PushBack(LogRecord rec);

    // Push a record back onto the FRONT — used by the drain loop
    // when a popped record fails to insert and we want it to be the
    // next thing retried. Never drops: capacity is intentionally a
    // soft cap on PushBack only, so a failed drain doesn't lose a
    // record that was already accepted into the queue.
    void PushFront(LogRecord rec);

    // Pop the oldest record into `out`. Returns true on success,
    // false if the queue is empty.
    bool PopFront(LogRecord& out);

    std::size_t Size() const;
    std::size_t Capacity() const { return m_capacity; }
    bool Empty() const;

private:
    mutable std::mutex      m_mtx;
    std::deque<LogRecord>   m_q;
    std::size_t             m_capacity;
};

} // namespace tlogsvr
