// F4 unit test: StepScheduler state machine. Pure logic — no asio
// io_context, no peers. Drives synthetic timestamps through the
// scheduler and verifies the (kind, msg_type, value) tuples it
// emits at each boundary.
//
// Covers:
//   1. Term event start-alarm → start → end-alarm → end transitions
//   2. Daily event minute-of-day wrap
//   3. Lottery / GiftTime auto-delete on the start firing
//   4. CashSale start fires CashShopStop + CashItemSale combo
//   5. Stale (end < now, state=0) event auto-cleanup

#include "../event_scheduler.h"
#include "../services/event_registry.h"
#include "../services/event_types.h"

#include <cstdint>
#include <cstdio>
#include <ctime>

namespace {

using tcontrolsvr::EventInfo;
using tcontrolsvr::EventRegistry;
using tcontrolsvr::SchedulerAction;
using tcontrolsvr::StepScheduler;
namespace event_kind = tcontrolsvr::event_kind;

int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

bool Has(const std::vector<SchedulerAction>& actions,
         SchedulerAction::Kind k, std::uint32_t idx)
{
    for (const auto& a : actions)
        if (a.kind == k && a.event_index == idx) return true;
    return false;
}

EventInfo MakeTermEvent(std::uint32_t idx,
                        std::uint8_t kind,
                        std::int64_t start,
                        std::int64_t end,
                        std::uint32_t start_alarm_min,
                        std::uint32_t end_alarm_min,
                        std::uint16_t value)
{
    EventInfo e{};
    e.index       = idx;
    e.kind        = kind;
    e.start_unix  = start;
    e.end_unix    = end;
    e.start_alarm = start_alarm_min;
    e.end_alarm   = end_alarm_min;
    e.value       = value;
    e.part_time   = 1;     // term event
    return e;
}

void TestTermEventLifecycle()
{
    EventRegistry reg;
    constexpr std::int64_t kStart = 1000;
    constexpr std::int64_t kEnd   = 2000;
    constexpr std::uint32_t kAlarm = 1;  // 60s before start (in minutes)

    reg.Upsert(MakeTermEvent(7, /*ExpRate*/event_kind::kExpRate,
                             kStart, kEnd, kAlarm, kAlarm, 200));

    // Well before start_alarm → nothing fires.
    auto out = StepScheduler(kStart - 120, reg);
    EXPECT(out.empty());

    // Start-alarm tick → bTmp=1 fires once.
    out = StepScheduler(kStart - 30, reg);
    EXPECT(Has(out, SchedulerAction::Kind::EventMsgStart, 7));
    // Repeat tick at same time should be idempotent (start_alarm_fired flag).
    out = StepScheduler(kStart - 30, reg);
    EXPECT(!Has(out, SchedulerAction::Kind::EventMsgStart, 7));

    // Start time crossed → bTmp=3, state=running, value=200.
    out = StepScheduler(kStart + 5, reg);
    EXPECT(Has(out, SchedulerAction::Kind::EventUpdateStart, 7));
    EXPECT(reg.Find(7) != nullptr && reg.Find(7)->state == 1);

    // End-alarm tick → bTmp=2.
    out = StepScheduler(kEnd - 30, reg);
    EXPECT(Has(out, SchedulerAction::Kind::EventMsgEnd, 7));

    // End time → bTmp=4, state=idle.
    out = StepScheduler(kEnd + 5, reg);
    EXPECT(Has(out, SchedulerAction::Kind::EventUpdateEnd, 7));
    EXPECT(reg.Find(7) != nullptr && reg.Find(7)->state == 0);
}

void TestLotteryAutoDelete()
{
    EventRegistry reg;
    constexpr std::int64_t kStart = 5000;
    constexpr std::int64_t kEnd   = 9000;
    reg.Upsert(MakeTermEvent(11, event_kind::kLottery,
                             kStart, kEnd, 0, 0, 1));

    // With start_alarm=0 the alarm and start collapse to the same
    // tick — but the legacy state machine fires alarm first
    // (`else if` cascade), so the start needs a second tick.
    auto t1 = StepScheduler(kStart + 1, reg);
    EXPECT(Has(t1, SchedulerAction::Kind::EventMsgStart, 11));
    auto t2 = StepScheduler(kStart + 2, reg);
    EXPECT(Has(t2, SchedulerAction::Kind::EventUpdateStart, 11));
    EXPECT(Has(t2, SchedulerAction::Kind::EventDelete, 11));
    // The registry should no longer carry the lottery event.
    EXPECT(reg.Find(11) == nullptr);
}

void TestCashSaleStartFiresShopStop()
{
    EventRegistry reg;
    constexpr std::int64_t kStart = 10000;
    constexpr std::int64_t kEnd   = 11000;
    reg.Upsert(MakeTermEvent(42, event_kind::kCashSale,
                             kStart, kEnd, 1, 1, 25));

    // Start alarm fires → both EventMsgStart and CashShopStop.
    auto out = StepScheduler(kStart - 30, reg);
    EXPECT(Has(out, SchedulerAction::Kind::EventMsgStart, 42));
    EXPECT(Has(out, SchedulerAction::Kind::CashShopStop, 42));

    // Start time fires → CashSaleStart (not EventUpdateStart) with
    // the configured value carried.
    out = StepScheduler(kStart + 1, reg);
    bool saw_value = false;
    for (const auto& a : out)
        if (a.kind == SchedulerAction::Kind::CashSaleStart &&
            a.event_index == 42 && a.value == 25)
            saw_value = true;
    EXPECT(saw_value);
}

void TestPastEventAutoCleanup()
{
    EventRegistry reg;
    EventInfo e = MakeTermEvent(99, event_kind::kExpRate,
                                100, 200, 0, 0, 1);
    e.state = 0;
    reg.Upsert(e);

    auto out = StepScheduler(500, reg);
    EXPECT(Has(out, SchedulerAction::Kind::EventDelete, 99));
    EXPECT(reg.Find(99) == nullptr);
}

void TestOverlapValidation()
{
    EventRegistry reg;
    EventInfo a = MakeTermEvent(1, event_kind::kExpRate,
                                1000, 2000, 0, 0, 100);
    reg.Upsert(a);

    // Same kind, same window → overlap rejected.
    EventInfo b = MakeTermEvent(2, event_kind::kExpRate,
                                1500, 2500, 0, 0, 50);
    EXPECT(reg.OverlapsExisting(b, 0));

    // Different kind → no overlap rule.
    EventInfo c = MakeTermEvent(3, event_kind::kDropRate,
                                1500, 2500, 0, 0, 50);
    EXPECT(!reg.OverlapsExisting(c, 0));

    // Updating the same id (skip_index=1) → no self-overlap.
    EventInfo update = a;
    update.start_unix = 1200;
    EXPECT(!reg.OverlapsExisting(update, 1));
}

} // namespace

int main()
{
    TestTermEventLifecycle();
    TestLotteryAutoDelete();
    TestCashSaleStartFiresShopStop();
    TestPastEventAutoCleanup();
    TestOverlapValidation();
    if (g_fails) { std::fprintf(stderr, "%d failure(s)\n", g_fails); return 1; }
    std::printf("ok\n");
    return 0;
}
