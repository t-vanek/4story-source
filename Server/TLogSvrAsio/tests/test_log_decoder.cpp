// Unit test for DecodeLogDatagram — the wire codec at the heart of
// TLogSvrAsio. Builds wire-faithful _UDPPACKET / _LOG_DATA_ frames
// using the same byte layout as the emitter
// (Lib/Own/FourStoryCommon/src/audit/udp_audit_logger.cpp) so any
// drift between sender and receiver fails this test loudly.
//
// Runs anywhere we can build the binary; no DB or network needed.

#include "log_server.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_passed = 0;
int g_failed = 0;
void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

// Mirror of the sender's wire structs. Must match
// Lib/Own/FourStoryCommon/src/audit/udp_audit_logger.cpp byte for
// byte; the receiver-side struct in log_server.cpp has its own copy
// plus offset static_asserts so any drift here also breaks the build.
#pragma pack(push, 1)
struct DbTimestamp
{
    std::uint16_t year, month, day, hour, minute, second;
    std::uint32_t fraction;
};

struct LogData
{
    DbTimestamp   timestamp;
    std::uint32_t serverId;
    char          clientIp[16];
    std::uint32_t action;
    std::uint16_t mapId;
    std::uint16_t _pad_after_mapId;
    std::int32_t  posX, posY, posZ;
    std::int64_t  searchKeyInt[11];
    char          searchKeyStr[7][50];
    std::uint16_t _pad_after_searchKeyStr;
    std::uint32_t format;
    char          logPayload[512];
};

struct UdpPacket
{
    std::uint16_t size;
    std::uint16_t command;
    std::uint32_t socketFd;
    std::uint32_t seq;
    std::uint16_t checksum;
    char          srcIp[20];
    std::uint16_t srcPort;
    char          payload[1024];
};
#pragma pack(pop)

static_assert(offsetof(UdpPacket, payload) == 36,
              "UdpPacket.payload offset must equal legacy szPacket=36");
static_assert(offsetof(LogData, logPayload) == 500,
              "LogData.logPayload offset must equal legacy szLog=500");

constexpr std::uint16_t kLpLog  = 0;
constexpr std::uint16_t kLpChat = 1;

// Copy `s` into the fixed-size char array, NUL-terminating if it fits.
void SetStr(char* dst, std::size_t cap, const std::string& s)
{
    std::memset(dst, 0, cap);
    const std::size_t n = std::min(s.size(), cap - 1);
    std::memcpy(dst, s.data(), n);
}

// Build a "full" log datagram with no payload tail (LF_TEXT shape —
// matches every emit site in udp_audit_logger.cpp). Returns the
// packed bytes ready to feed to DecodeLogDatagram.
std::vector<std::byte> BuildDatagram(LogData log, std::uint16_t command = kLpLog,
                                     std::size_t extra_payload_bytes = 0)
{
    UdpPacket pkt{};
    pkt.command = command;
    const std::size_t udp_overhead = offsetof(UdpPacket, payload);
    const std::size_t log_prefix   = offsetof(LogData, logPayload);
    const std::size_t total = udp_overhead + log_prefix + extra_payload_bytes;
    pkt.size = static_cast<std::uint16_t>(total);

    std::vector<std::byte> out(total);
    // Copy UDP envelope up to but not including the payload[] tail.
    std::memcpy(out.data(), &pkt, udp_overhead);
    // Copy LogData prefix (everything up to logPayload[]).
    std::memcpy(out.data() + udp_overhead, &log, log_prefix);
    // Trailing payload bytes (caller fills them after if extra > 0).
    if (extra_payload_bytes > 0)
    {
        for (std::size_t i = 0; i < extra_payload_bytes; ++i)
            out[udp_overhead + log_prefix + i] =
                static_cast<std::byte>(0xA0 + (i & 0x0F));
    }
    return out;
}

LogData MakeSampleLog()
{
    LogData log{};
    log.timestamp = DbTimestamp{2026, 5, 20, 14, 7, 42, 0};
    log.serverId  = 0x1234;
    SetStr(log.clientIp, sizeof(log.clientIp), "10.20.30.40");
    log.action = 0x0008;   // LOGLOGIN_GAMESTART
    log.mapId  = 31;
    log.posX = 123; log.posY = 456; log.posZ = -789;
    for (int i = 0; i < 11; ++i) log.searchKeyInt[i] = 1000 + i;
    SetStr(log.searchKeyStr[0], sizeof(log.searchKeyStr[0]), "user42");
    SetStr(log.searchKeyStr[4], sizeof(log.searchKeyStr[4]), "ok");
    log.format = 0;
    return log;
}

void TestHappyPathRoundTrip()
{
    std::printf("[log_decoder — happy-path round trip]\n");
    auto datagram = BuildDatagram(MakeSampleLog());

    tlogsvr::LogRecord rec{};
    const bool ok = tlogsvr::DecodeLogDatagram(
        datagram.data(), datagram.size(), rec);

    Check(ok, "DecodeLogDatagram returns true for a well-formed frame");
    Check(rec.timestamp_iso == "2026-05-20 14:07:42",
        "timestamp formatted as YYYY-MM-DD HH:MM:SS");
    Check(rec.server_id == 0x1234, "serverId round-trips");
    Check(rec.client_ip == "10.20.30.40", "clientIp NUL-terminated string");
    Check(rec.action == 0x0008, "action round-trips");
    Check(rec.map_id == 31, "mapId round-trips");
    Check(rec.pos_x == 123 && rec.pos_y == 456 && rec.pos_z == -789,
        "posX/Y/Z round-trip including negative");
    Check(rec.search_int[0] == 1000 && rec.search_int[10] == 1010,
        "all 11 searchKeyInt slots round-trip");
    Check(rec.search_str[0] == "user42", "searchKeyStr[0] round-trips");
    Check(rec.search_str[4] == "ok", "searchKeyStr[4] round-trips");
    Check(rec.payload.empty(), "empty payload tail decoded as empty vector");
}

void TestPayloadTailRoundTrip()
{
    std::printf("[log_decoder — payload tail (LF_CHARBASE-style blob)]\n");
    constexpr std::size_t kBlobBytes = 64;
    auto datagram = BuildDatagram(MakeSampleLog(), kLpLog, kBlobBytes);

    tlogsvr::LogRecord rec{};
    const bool ok = tlogsvr::DecodeLogDatagram(
        datagram.data(), datagram.size(), rec);

    Check(ok, "decode succeeds with non-empty payload tail");
    Check(rec.payload.size() == kBlobBytes,
        "payload size matches declared trailing bytes");
    Check(static_cast<unsigned char>(rec.payload.front()) == 0xA0 &&
          static_cast<unsigned char>(rec.payload.back()) == (0xA0 + ((kBlobBytes - 1) & 0x0F)),
        "payload bytes match the BuildDatagram fill pattern");
}

void TestRejectsWrongCommand()
{
    std::printf("[log_decoder — command != LP_LOG is rejected]\n");
    auto datagram = BuildDatagram(MakeSampleLog(), kLpChat);

    tlogsvr::LogRecord rec{};
    const bool ok = tlogsvr::DecodeLogDatagram(
        datagram.data(), datagram.size(), rec);
    Check(!ok, "LP_CHAT (legacy unused) frame is dropped");
}

void TestRejectsShortDatagram()
{
    std::printf("[log_decoder — short datagram below minimum size]\n");
    std::array<std::byte, 16> tiny{};
    tlogsvr::LogRecord rec{};
    const bool ok = tlogsvr::DecodeLogDatagram(tiny.data(), tiny.size(), rec);
    Check(!ok, "16-byte datagram (less than UdpPacket+prefix) is dropped");
}

void TestRejectsSizeLargerThanReceived()
{
    std::printf("[log_decoder — declared size > received length]\n");
    auto datagram = BuildDatagram(MakeSampleLog());
    // Bump the declared size past the buffer length.
    UdpPacket hdr{};
    std::memcpy(&hdr, datagram.data(), offsetof(UdpPacket, payload));
    hdr.size = static_cast<std::uint16_t>(datagram.size() + 64);
    std::memcpy(datagram.data(), &hdr, offsetof(UdpPacket, payload));

    tlogsvr::LogRecord rec{};
    const bool ok = tlogsvr::DecodeLogDatagram(
        datagram.data(), datagram.size(), rec);
    Check(!ok, "declared size larger than received bytes is dropped");
}

void TestRejectsOversizedPayload()
{
    std::printf("[log_decoder — payload tail > 512 bytes]\n");
    // Build a frame that *claims* to carry 600 bytes of payload — past
    // the legacy szLog[512] cap. The receiver must reject the whole
    // datagram rather than silently truncate.
    constexpr std::size_t kOversized = 600;
    auto datagram = BuildDatagram(MakeSampleLog(), kLpLog, kOversized);
    tlogsvr::LogRecord rec{};
    const bool ok = tlogsvr::DecodeLogDatagram(
        datagram.data(), datagram.size(), rec);
    Check(!ok, "payload tail past 512-byte cap is dropped");
}

void TestUninitializedTimestampSentinel()
{
    std::printf("[log_decoder — out-of-range timestamp → sentinel]\n");
    LogData log = MakeSampleLog();
    log.timestamp = DbTimestamp{};   // all zeros
    auto datagram = BuildDatagram(log);

    tlogsvr::LogRecord rec{};
    const bool ok = tlogsvr::DecodeLogDatagram(
        datagram.data(), datagram.size(), rec);
    Check(ok, "decode still succeeds with year=0 timestamp");
    Check(rec.timestamp_iso == "1970-01-01 00:00:00",
        "year=0 falls back to MSSQL-safe sentinel");
}

void TestPaddedStringNotPickedUpFromNextSlot()
{
    std::printf("[log_decoder — embedded NULs terminate fixed-width strings]\n");
    LogData log = MakeSampleLog();
    SetStr(log.searchKeyStr[0], sizeof(log.searchKeyStr[0]), "alice");
    // Pollute the rest of slot 0 *after* the NUL to make sure
    // ReadFixedString stops at the first \0.
    std::memset(log.searchKeyStr[0] + 6, 'X', 30);
    auto datagram = BuildDatagram(log);

    tlogsvr::LogRecord rec{};
    Check(tlogsvr::DecodeLogDatagram(datagram.data(), datagram.size(), rec),
        "decode succeeds");
    Check(rec.search_str[0] == "alice",
        "trailing garbage past the NUL is dropped");
}

} // namespace

int main()
{
    std::printf("=== tlogsvr_asio log_decoder unit test ===\n");

    try
    {
        TestHappyPathRoundTrip();
        TestPayloadTailRoundTrip();
        TestRejectsWrongCommand();
        TestRejectsShortDatagram();
        TestRejectsSizeLargerThanReceived();
        TestRejectsOversizedPayload();
        TestUninitializedTimestampSentinel();
        TestPaddedStringNotPickedUpFromNextSlot();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }

    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
