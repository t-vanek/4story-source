// Unit test: the quest wire encoders — CS_QUESTUPDATE_ACK (11 bytes) and
// CS_QUESTCOMPLETE_ACK (14 bytes), byte-exact to the legacy
// SendCS_QUESTUPDATE_ACK / SendCS_QUESTCOMPLETE_ACK (CSSender.cpp:1825 /
// :1843) — plus the in-memory quest log ops.

#include "services/client_senders.h"
#include "services/quest_log.h"
#include "domain/quest.h"
#include "domain/quest_def.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {
int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

std::uint8_t RdU8(const std::vector<std::byte>& b, std::size_t off)
{
    return std::to_integer<std::uint8_t>(b[off]);
}
std::uint32_t RdU32(const std::vector<std::byte>& b, std::size_t off)
{
    return  std::to_integer<std::uint32_t>(b[off])
         | (std::to_integer<std::uint32_t>(b[off + 1]) << 8)
         | (std::to_integer<std::uint32_t>(b[off + 2]) << 16)
         | (std::to_integer<std::uint32_t>(b[off + 3]) << 24);
}
} // namespace

int main()
{
    using namespace tmapsvr;

    // --- CS_QUESTUPDATE_ACK: u32 quest, u32 term, u8 type, u8 count, u8 status
    {
        const auto b = EncodeQuestUpdateAck(
            0x11223344u, 0x55667788u, QTT_HUNT, 2, QTS_RUN);
        EXPECT(b.size() == 11);
        EXPECT(RdU32(b, 0) == 0x11223344u);
        EXPECT(RdU32(b, 4) == 0x55667788u);
        EXPECT(RdU8(b, 8)  == static_cast<std::uint8_t>(QTT_HUNT));
        EXPECT(RdU8(b, 9)  == 2);
        EXPECT(RdU8(b, 10) == static_cast<std::uint8_t>(QTS_RUN));
    }

    // --- CS_QUESTCOMPLETE_ACK: u8 result, u32 quest, u32 term, u8 type, u32 drop
    {
        const auto b = EncodeQuestCompleteAck(
            QR_SUCCESS, 0xAABBCCDDu, 0x01020304u, QTT_HUNT, 0x0A0B0C0Du);
        EXPECT(b.size() == 14);
        EXPECT(RdU8(b, 0)  == static_cast<std::uint8_t>(QR_SUCCESS));
        EXPECT(RdU32(b, 1) == 0xAABBCCDDu);
        EXPECT(RdU32(b, 5) == 0x01020304u);
        EXPECT(RdU8(b, 9)  == static_cast<std::uint8_t>(QTT_HUNT));
        EXPECT(RdU32(b, 10) == 0x0A0B0C0Du);
    }

    // --- InMemoryQuestLog: seed / find / add / remove ----------------
    {
        InMemoryQuestLog log;
        EXPECT(!log.Has(7));

        QuestProgressRow q; q.dwQuestID = 100;
        log.Seed(7, { q });
        EXPECT(log.Has(7));
        EXPECT(log.Find(7, 100) != nullptr);
        EXPECT(log.Find(7, 999) == nullptr);

        QuestProgressRow q2; q2.dwQuestID = 200;
        log.Add(7, q2);
        EXPECT(log.ForChar(7).size() == 2);

        EXPECT(log.Remove(7, 100));
        EXPECT(log.Find(7, 100) == nullptr);
        EXPECT(!log.Remove(7, 100));        // already gone
        EXPECT(log.ForChar(7).size() == 1);
    }

    if (g_fails == 0)
        std::printf("test_quest_senders: update/complete ack bytes + quest log OK\n");
    return g_fails == 0 ? 0 : 1;
}
