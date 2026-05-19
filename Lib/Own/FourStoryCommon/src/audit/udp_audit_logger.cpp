#include "fourstory/audit/udp_audit_logger.h"

#include <boost/asio/ip/udp.hpp>

#include <spdlog/spdlog.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <utility>

namespace fourstory::audit {

namespace {

// Portable replica of the legacy DBTIMESTAMP (oledb.h). 16 bytes,
// little-endian on the wire — 6 SHORTs (year/month/day/hour/min/sec)
// + 1 ULONG (fraction in 1e-9 sec).
#pragma pack(push, 1)
struct DbTimestamp
{
    std::uint16_t year;
    std::uint16_t month;
    std::uint16_t day;
    std::uint16_t hour;
    std::uint16_t minute;
    std::uint16_t second;
    std::uint32_t fraction;
};

// _LOG_DATA_ from Lib/Own/TProtocol/include/LogPacket.h. Fixed-size
// fields followed by szLog payload — we only fill the prefix.
//
// CRITICAL alignment note: the legacy LogPacket.h declares this
// struct WITHOUT a `#pragma pack` directive, so MSVC applies its
// default natural alignment. Two compiler-inserted padding gaps
// matter to the wire:
//
//   * 2 bytes after `wMapID` (2-byte field at offset 40) before
//     `int nX` (4-byte align → offset 44, not 42)
//   * 2 bytes after `szKey[7][50]` (350-byte char array ending at
//     offset 494) before `DWORD dwFormat` (4-byte align → offset
//     496, not 494)
//
// We declare them explicitly as `_pad*` fields so the struct stays
// `#pragma pack(1)` (no surprise alignment differences across
// compilers) while still matching the wire bytes the legacy senders
// emit. The static_asserts below pin the offsets.
struct LogData
{
    DbTimestamp   timestamp;
    std::uint32_t serverId;
    char          clientIp[16];
    std::uint32_t action;
    std::uint16_t mapId;
    std::uint16_t _pad_after_mapId;       // compiler padding on legacy MSVC
    std::int32_t  posX;
    std::int32_t  posY;
    std::int32_t  posZ;
    std::int64_t  searchKeyInt[11];
    char          searchKeyStr[7][50];
    std::uint16_t _pad_after_searchKeyStr;// compiler padding on legacy MSVC
    std::uint32_t format;
    char          logPayload[512];
};

// _UDPPACKET — legacy wraps the LogData inside this fixed-size
// envelope. We mirror it 1:1 so TLogSvr accepts the frame.
//
// CRITICAL byte-layout note: the legacy struct (LogPacket.h) declares
// the third field as `void* pSocketFD`. Every legacy server in the
// cluster (TLoginSvr, TWorldSvr, TMapSvr, …) compiles as Win32 (32-bit)
// per their vcxproj configurations, which makes that pointer 4 bytes
// wide. Using `uint64_t` here on a 64-bit host shifts every following
// field by 4 bytes — TLogSvrAsio would then read seq from the void*
// upper half, srcIp from seq + checksum, etc., and reject every frame
// as malformed (or worse, silently store garbage into the DB).
// Keep it `uint32_t` so the wire layout matches the still-Win32-only
// legacy senders. The mirroring TLogSvrAsio struct must match.
struct UdpPacket
{
    std::uint16_t size;       // total bytes written (dwSize)
    std::uint16_t command;    // LP_LOG = 0
    std::uint32_t socketFd;   // legacy `void* pSocketFD` on Win32 — 4 bytes
    std::uint32_t seq;
    std::uint16_t checksum;
    char          srcIp[20];
    std::uint16_t srcPort;
    char          payload[1024]; // szPacket — holds LogData prefix
};
#pragma pack(pop)

// Compile-time wire-layout guards. The legacy receiver expects
// these exact offsets; any drift (changing socketFd's width, adding
// fields, reordering) silently corrupts every audit row. The
// payload offset is the most load-bearing — it's what the
// `udp_overhead` runtime constant in TLogSvrAsio's DecodeRecord
// reaches via `offsetof(UdpPacket, payload)`. Match must hold on
// both sides.
static_assert(offsetof(UdpPacket, socketFd) == 4,
              "UdpPacket.socketFd offset drifted — receiver will misframe");
static_assert(offsetof(UdpPacket, seq)      == 8,
              "UdpPacket.seq offset drifted");
static_assert(offsetof(UdpPacket, srcIp)    == 14,
              "UdpPacket.srcIp offset drifted");
static_assert(offsetof(UdpPacket, payload)  == 36,
              "UdpPacket.payload offset drifted — legacy szPacket starts at 36");

// LogData offsets — keep them aligned with the legacy MSVC natural-
// alignment layout (no #pragma pack). The two padding fields above
// are the load-bearing pieces; if they're removed the wire shifts.
static_assert(offsetof(LogData, posX)         == 44,
              "LogData.posX must land at byte 44 (legacy 2-byte pad after wMapID)");
static_assert(offsetof(LogData, searchKeyInt) == 56,
              "LogData.searchKeyInt offset drifted");
static_assert(offsetof(LogData, searchKeyStr) == 144,
              "LogData.searchKeyStr offset drifted");
static_assert(offsetof(LogData, format)       == 496,
              "LogData.format must land at byte 496 (legacy 2-byte pad after szKey)");
static_assert(offsetof(LogData, logPayload)   == 500,
              "LogData.logPayload offset drifted");

static_assert(sizeof(DbTimestamp) == 16, "DbTimestamp must be 16 bytes");

// Legacy action codes (LogPacket.h:101-106).
constexpr std::uint32_t LOGLOGIN_LOGIN       = 0x0000 + 0;
constexpr std::uint32_t LOGLOGIN_CHARCREATE  = 0x0000 + 4;
constexpr std::uint32_t LOGLOGIN_CHARDELETE  = 0x0000 + 5;
constexpr std::uint32_t LOGLOGIN_GAMESTART   = 0x0000 + 8;
constexpr std::uint32_t LF_TEXT      = 0;
constexpr std::uint32_t LF_CHARBASE  = 1;
constexpr std::uint16_t LP_LOG       = 0;

DbTimestamp NowUtc()
{
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto sec = duration_cast<seconds>(now.time_since_epoch()).count();
    const auto frac_ns = duration_cast<nanoseconds>(
        now.time_since_epoch()).count() % 1'000'000'000LL;
    std::time_t t = static_cast<std::time_t>(sec);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    DbTimestamp ts{};
    ts.year       = static_cast<std::uint16_t>(tm.tm_year + 1900);
    ts.month      = static_cast<std::uint16_t>(tm.tm_mon + 1);
    ts.day        = static_cast<std::uint16_t>(tm.tm_mday);
    ts.hour       = static_cast<std::uint16_t>(tm.tm_hour);
    ts.minute     = static_cast<std::uint16_t>(tm.tm_min);
    ts.second     = static_cast<std::uint16_t>(tm.tm_sec);
    ts.fraction   = static_cast<std::uint32_t>(frac_ns);
    return ts;
}

void SetStr(char* dst, std::size_t cap, const std::string& src)
{
    const auto n = std::min(cap - 1, src.size());
    std::memcpy(dst, src.data(), n);
    dst[n] = '\0';
}

const char* NameLogin(LoginOutcome o)
{
    switch (o)
    {
    case LoginOutcome::Success:         return "SUCCESS";
    case LoginOutcome::NoUser:          return "NOUSER";
    case LoginOutcome::WrongPassword:   return "INVALIDPASSWD";
    case LoginOutcome::Duplicate:       return "DUPLICATE";
    case LoginOutcome::Banned:          return "BLOCK";
    case LoginOutcome::IpBanned:        return "IPBLOCK";
    case LoginOutcome::AgreementNeeded: return "NEEDAGREEMENT";
    case LoginOutcome::VersionMismatch: return "VERSION";
    case LoginOutcome::InternalError:   return "INTERNAL";
    }
    return "?";
}

const char* NameCreate(CreateCharOutcome o)
{
    switch (o)
    {
    case CreateCharOutcome::Success:        return "SUCCESS";
    case CreateCharOutcome::NoGroup:        return "NOGROUP";
    case CreateCharOutcome::DuplicateName:  return "DUPNAME";
    case CreateCharOutcome::InvalidSlot:    return "INVALIDSLOT";
    case CreateCharOutcome::Protected:      return "PROTECTED";
    case CreateCharOutcome::OverChar:       return "OVERCHAR";
    case CreateCharOutcome::Internal:       return "INTERNAL";
    }
    return "?";
}

const char* NameDelete(DeleteCharOutcome o)
{
    switch (o)
    {
    case DeleteCharOutcome::Success:         return "SUCCESS";
    case DeleteCharOutcome::Failed:          return "FAILED";
    case DeleteCharOutcome::InvalidPassword: return "INVALIDPASSWD";
    case DeleteCharOutcome::Internal:        return "INTERNAL";
    case DeleteCharOutcome::NoGroup:         return "NOGROUP";
    }
    return "?";
}

} // namespace

UdpAuditLogger::UdpAuditLogger(boost::asio::io_context& io,
                               std::string endpoint_host,
                               std::uint16_t endpoint_port,
                               std::unique_ptr<IAuditLogger> inner)
    : m_io(io)
    , m_endpoint_host(std::move(endpoint_host))
    , m_endpoint_port(endpoint_port)
    , m_inner(std::move(inner))
{
}

void UdpAuditLogger::SendBuffer(const void* data, std::size_t bytes)
{
    std::lock_guard<std::mutex> lock(m_send_mtx);
    if (m_resolve_failed) return;

    try
    {
        if (!m_socket)
        {
            boost::asio::ip::udp::resolver resolver(m_io);
            const auto results = resolver.resolve(
                m_endpoint_host, std::to_string(m_endpoint_port));
            if (results.empty()) { m_resolve_failed = true; return; }
            m_endpoint = *results.begin();
            m_socket = std::make_unique<boost::asio::ip::udp::socket>(m_io);
            m_socket->open(m_endpoint.protocol());
        }
        boost::system::error_code ec;
        m_socket->send_to(boost::asio::buffer(data, bytes), m_endpoint, 0, ec);
        if (ec)
        {
            spdlog::debug("udp_audit: send_to {} failed: {}",
                m_endpoint_host, ec.message());
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::debug("udp_audit: send failed: {}", ex.what());
        // Mark resolver as failed so we don't pay the lookup cost on
        // every event when DNS is misconfigured.
        if (!m_socket) m_resolve_failed = true;
    }
}

void UdpAuditLogger::LogLogin(LoginOutcome outcome,
                              const std::string& user_id_str,
                              std::int32_t db_user_id,
                              const std::string& client_ip,
                              std::uint32_t session_key)
{
    UdpPacket pkt{};
    LogData log{};
    log.timestamp = NowUtc();
    log.action = LOGLOGIN_LOGIN;
    log.format = LF_TEXT;
    SetStr(log.clientIp, sizeof(log.clientIp), client_ip);
    log.searchKeyInt[0] = db_user_id;
    SetStr(log.searchKeyStr[0], sizeof(log.searchKeyStr[0]), user_id_str);
    SetStr(log.searchKeyStr[4], sizeof(log.searchKeyStr[4]), NameLogin(outcome));

    // Mirror legacy size calc: sizeof(_UDPPACKET) -
    //   sizeof(szPacket) + sizeof(_LOG_DATA_) - sizeof(szLog) + pLogSize.
    // For LF_TEXT with no text payload (legacy used szLog tail), the
    // pLogSize is 0 from `lstrlen("")`.
    const std::size_t udp_overhead = offsetof(UdpPacket, payload);
    const std::size_t log_overhead = offsetof(LogData, logPayload);
    const std::uint16_t total = static_cast<std::uint16_t>(
        udp_overhead + log_overhead);
    pkt.size = total;
    pkt.command = LP_LOG;
    pkt.seq = ++m_seq;
    std::memcpy(pkt.payload, &log, log_overhead);
    SendBuffer(&pkt, total);

    if (m_inner) m_inner->LogLogin(outcome, user_id_str, db_user_id,
        client_ip, session_key);
}

void UdpAuditLogger::LogCharCreate(CreateCharOutcome outcome,
                                   std::int32_t db_user_id,
                                   std::uint8_t group_id,
                                   const std::string& char_name,
                                   std::int32_t db_char_id)
{
    // Legacy includes LF_CHARBASE payload here; we ship the same
    // outer envelope with names + result code, sized to the
    // prefix-only LF_TEXT shape. TLogSvr accepts both formats per the
    // dwFormat dispatch in its parser.
    UdpPacket pkt{};
    LogData log{};
    log.timestamp = NowUtc();
    log.action = LOGLOGIN_CHARCREATE;
    log.serverId = group_id;
    log.format = LF_TEXT;
    log.searchKeyInt[0] = db_user_id;
    SetStr(log.searchKeyStr[1], sizeof(log.searchKeyStr[1]), char_name);
    SetStr(log.searchKeyStr[4], sizeof(log.searchKeyStr[4]),
        NameCreate(outcome));

    const std::size_t udp_overhead = offsetof(UdpPacket, payload);
    const std::size_t log_overhead = offsetof(LogData, logPayload);
    const std::uint16_t total = static_cast<std::uint16_t>(
        udp_overhead + log_overhead);
    pkt.size = total;
    pkt.command = LP_LOG;
    pkt.seq = ++m_seq;
    std::memcpy(pkt.payload, &log, log_overhead);
    SendBuffer(&pkt, total);

    if (m_inner) m_inner->LogCharCreate(outcome, db_user_id, group_id,
        char_name, db_char_id);
}

void UdpAuditLogger::LogCharDelete(DeleteCharOutcome outcome,
                                   std::int32_t db_user_id,
                                   std::uint8_t group_id,
                                   std::int32_t db_char_id)
{
    UdpPacket pkt{};
    LogData log{};
    log.timestamp = NowUtc();
    log.action = LOGLOGIN_CHARDELETE;
    log.serverId = group_id;
    log.format = LF_TEXT;
    log.searchKeyInt[0] = db_user_id;
    log.searchKeyInt[1] = db_char_id;
    SetStr(log.searchKeyStr[4], sizeof(log.searchKeyStr[4]),
        NameDelete(outcome));

    const std::size_t udp_overhead = offsetof(UdpPacket, payload);
    const std::size_t log_overhead = offsetof(LogData, logPayload);
    const std::uint16_t total = static_cast<std::uint16_t>(
        udp_overhead + log_overhead);
    pkt.size = total;
    pkt.command = LP_LOG;
    pkt.seq = ++m_seq;
    std::memcpy(pkt.payload, &log, log_overhead);
    SendBuffer(&pkt, total);

    if (m_inner) m_inner->LogCharDelete(outcome, db_user_id, group_id,
        db_char_id);
}

void UdpAuditLogger::LogGameStart(bool success,
                                  std::int32_t db_user_id,
                                  std::uint8_t group_id,
                                  std::uint8_t channel,
                                  std::int32_t db_char_id,
                                  std::uint8_t map_server_id)
{
    UdpPacket pkt{};
    LogData log{};
    log.timestamp = NowUtc();
    log.action = LOGLOGIN_GAMESTART;
    log.serverId = group_id;
    log.format = LF_TEXT;
    log.searchKeyInt[0] = db_user_id;
    log.searchKeyInt[1] = db_char_id;
    log.searchKeyInt[2] = map_server_id;
    log.searchKeyInt[3] = channel;
    SetStr(log.searchKeyStr[4], sizeof(log.searchKeyStr[4]),
        success ? "SUCCESS" : "NOSERVER");

    const std::size_t udp_overhead = offsetof(UdpPacket, payload);
    const std::size_t log_overhead = offsetof(LogData, logPayload);
    const std::uint16_t total = static_cast<std::uint16_t>(
        udp_overhead + log_overhead);
    pkt.size = total;
    pkt.command = LP_LOG;
    pkt.seq = ++m_seq;
    std::memcpy(pkt.payload, &log, log_overhead);
    SendBuffer(&pkt, total);

    if (m_inner) m_inner->LogGameStart(success, db_user_id, group_id, channel,
        db_char_id, map_server_id);
}

} // namespace fourstory::audit
