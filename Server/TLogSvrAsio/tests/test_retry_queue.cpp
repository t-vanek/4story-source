// Unit test for RetryQueue — the bounded FIFO that buffers LogRecord
// values while the audit DB is unavailable. Mirrors the legacy
// `m_listReadCompleted` requeue behavior; semantics asserted here:
//
//   * FIFO order: PushBack/PopFront round-trip preserves insertion order.
//   * Bounded: PushBack past capacity returns false and leaves the queue
//     unchanged (NEW arrivals dropped, old entries kept — matches the
//     legacy TMemPool<_UDPPACKET>(1000) "fail on alloc" behavior).
//   * PushFront: failed drain retries put the record back on the head;
//     never drops on cap (a drain failure shouldn't lose a record that
//     was already accepted into the queue).
//   * Empty / Size / Capacity accessors return sane values.
//   * Concurrent PushBack from multiple threads doesn't lose records
//     and respects the cap exactly.
//
// Runs anywhere we can build the binary; no DB or network needed.

#include "services/retry_queue.h"
#include "services/log_sink.h"

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace {

int g_passed = 0;
int g_failed = 0;
void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

tlogsvr::LogRecord MakeRec(int tag)
{
    tlogsvr::LogRecord r{};
    r.timestamp_iso  = "2026-05-20 14:07:42";
    r.client_ip      = "10.0.0." + std::to_string(tag & 0xFF);
    r.search_int[0]  = tag;
    return r;
}

void TestEmptyAtStart()
{
    std::printf("[retry_queue — fresh queue is empty]\n");
    tlogsvr::RetryQueue q(4);
    Check(q.Empty(), "Empty() true on construction");
    Check(q.Size() == 0, "Size() 0 on construction");
    Check(q.Capacity() == 4, "Capacity() reflects ctor arg");
    tlogsvr::LogRecord pop_into{};
    Check(!q.PopFront(pop_into), "PopFront on empty returns false");
}

void TestFifoOrdering()
{
    std::printf("[retry_queue — FIFO ordering preserved]\n");
    tlogsvr::RetryQueue q(8);
    for (int i = 0; i < 5; ++i)
        Check(q.PushBack(MakeRec(i)),
            ("PushBack #" + std::to_string(i) + " accepted").c_str());
    Check(q.Size() == 5, "Size() == 5 after 5 PushBacks");

    for (int i = 0; i < 5; ++i)
    {
        tlogsvr::LogRecord out{};
        Check(q.PopFront(out), "PopFront returns true for non-empty");
        Check(out.search_int[0] == i,
            ("popped record carries tag " + std::to_string(i)).c_str());
    }
    Check(q.Empty(), "empty again after draining");
}

void TestCapBlocksNewArrivals()
{
    std::printf("[retry_queue — cap drops new arrivals, keeps old]\n");
    tlogsvr::RetryQueue q(3);
    Check(q.PushBack(MakeRec(100)), "first push accepted");
    Check(q.PushBack(MakeRec(101)), "second push accepted");
    Check(q.PushBack(MakeRec(102)), "third push accepted (at cap)");
    Check(!q.PushBack(MakeRec(103)),
        "fourth push REJECTED (would exceed cap)");
    Check(q.Size() == 3,
        "size still 3 after rejected push — old entries kept");

    tlogsvr::LogRecord out{};
    Check(q.PopFront(out) && out.search_int[0] == 100,
        "head is still the OLDEST (tag 100), not the new arrival");
}

void TestPushFrontNeverDrops()
{
    std::printf("[retry_queue — PushFront ignores cap (failed-retry path)]\n");
    tlogsvr::RetryQueue q(2);
    Check(q.PushBack(MakeRec(10)), "push tag=10");
    Check(q.PushBack(MakeRec(11)), "push tag=11 (at cap)");
    Check(!q.PushBack(MakeRec(12)), "push tag=12 REJECTED at cap");

    // Simulate a drain attempt: pop, try to insert (fails), push back
    // to head. PushFront must succeed even though we're already at cap
    // — otherwise a failed drain would silently lose a record that
    // had already been accepted.
    tlogsvr::LogRecord drained{};
    Check(q.PopFront(drained), "pop for drain attempt");
    Check(drained.search_int[0] == 10, "popped the oldest");
    q.PushFront(std::move(drained));
    Check(q.Size() == 2, "size restored to 2");

    tlogsvr::LogRecord head_again{};
    Check(q.PopFront(head_again), "second pop");
    Check(head_again.search_int[0] == 10,
        "PushFront put the record back at the HEAD (not the tail)");
}

void TestConcurrentPushBackRespectsCap()
{
    std::printf("[retry_queue — concurrent PushBack respects cap]\n");
    constexpr std::size_t kCap     = 100;
    constexpr std::size_t kThreads = 8;
    constexpr std::size_t kPerThr  = 50;  // total attempts = 400, cap = 100

    tlogsvr::RetryQueue q(kCap);
    std::atomic<std::size_t> accepted{0};
    std::atomic<std::size_t> rejected{0};
    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (std::size_t t = 0; t < kThreads; ++t)
    {
        workers.emplace_back([&, t]() {
            for (std::size_t i = 0; i < kPerThr; ++i)
            {
                if (q.PushBack(MakeRec(static_cast<int>(t * 1000 + i))))
                    accepted.fetch_add(1);
                else
                    rejected.fetch_add(1);
            }
        });
    }
    for (auto& w : workers) w.join();

    Check(accepted.load() == kCap,
        "exactly cap-many PushBacks succeeded across threads");
    Check(rejected.load() == kThreads * kPerThr - kCap,
        "remaining PushBacks rejected exactly as expected");
    Check(q.Size() == kCap, "final size == cap");
}

void TestZeroCapacityDropsEverything()
{
    std::printf("[retry_queue — capacity 0 disables queueing]\n");
    tlogsvr::RetryQueue q(0);
    Check(!q.PushBack(MakeRec(1)),
        "PushBack with cap=0 always rejects");
    Check(q.Empty(), "queue stays empty");
}

} // namespace

int main()
{
    std::printf("=== tlogsvr_asio retry_queue unit test ===\n");

    try
    {
        TestEmptyAtStart();
        TestFifoOrdering();
        TestCapBlocksNewArrivals();
        TestPushFrontNeverDrops();
        TestConcurrentPushBackRespectsCap();
        TestZeroCapacityDropsEverything();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }

    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
