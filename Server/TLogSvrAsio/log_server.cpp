#include "log_server.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <spdlog/spdlog.h>

#include <array>
#include <cstring>
#include <sstream>

namespace tlogsvr {

namespace {

// Mirror of Lib/Own/TProtocol/include/LogPacket.h, portable replicas
// so this binary builds on Linux. Identical layout to what
// TLoginSvrAsio's UdpAuditLogger emits, so the audit shim there
// round-trips with this collector.
#pragma pack(push, 1)
struct DbTimestamp
{
    std::uint16_t year, month, day, hour, minute, second;
    std::uint32_t fraction;
};

struct LogData
{
    // Legacy LogPacket.h declares this struct WITHOUT #pragma pack,
    // so MSVC's natural alignment inserts 2 bytes of padding after
    // `wMapID` (before `int nX`) and 2 bytes after `szKey[7][50]`
    // (before `DWORD dwFormat`). We declare those gaps explicitly
    // so #pragma pack(1) still yields the legacy wire layout —
    // sender side does the same. static_asserts below pin offsets.
    DbTimestamp    timestamp;
    std::uint32_t  serverId;
    char           clientIp[16];
    std::uint32_t  action;
    std::uint16_t  mapId;
    std::uint16_t  _pad_after_mapId;
    std::int32_t   posX, posY, posZ;
    std::int64_t   searchKeyInt[11];
    char           searchKeyStr[7][50];
    std::uint16_t  _pad_after_searchKeyStr;
    std::uint32_t  format;
    char           logPayload[512];
};

struct UdpPacket
{
    std::uint16_t  size;
    std::uint16_t  command;
    // Legacy `void* pSocketFD` (LogPacket.h). Every legacy server
    // compiles as Win32 (32-bit) per its vcxproj — so the pointer is
    // 4 bytes on the wire, not 8. Using uint64_t here would shift
    // every following field by 4 bytes and silently corrupt every
    // log record from a legacy sender. The mirroring sender in
    // FourStoryCommon UdpAuditLogger must match.
    std::uint32_t  socketFd;
    std::uint32_t  seq;
    std::uint16_t  checksum;
    char           srcIp[20];
    std::uint16_t  srcPort;
    char           payload[1024];
};
#pragma pack(pop)

static_assert(sizeof(DbTimestamp) == 16, "DbTimestamp must be 16 bytes");

// Wire-layout invariants — keep in sync with the mirroring sender in
// FourStoryCommon::udp_audit_logger.cpp. Any drift here silently
// corrupts every received log record.
static_assert(offsetof(UdpPacket, socketFd) == 4,
              "UdpPacket.socketFd offset drifted from sender");
static_assert(offsetof(UdpPacket, seq)      == 8,
              "UdpPacket.seq offset drifted");
static_assert(offsetof(UdpPacket, srcIp)    == 14,
              "UdpPacket.srcIp offset drifted");
static_assert(offsetof(UdpPacket, payload)  == 36,
              "UdpPacket.payload offset drifted from legacy szPacket=36");

// LogData offsets must match the legacy MSVC natural-alignment
// layout. Pinning these so a future "remove the unused pad field"
// refactor fails loudly instead of silently corrupting every row.
static_assert(offsetof(LogData, posX)         == 44,
              "LogData.posX must land at byte 44 (legacy 2-byte pad after wMapID)");
static_assert(offsetof(LogData, searchKeyInt) == 56,
              "LogData.searchKeyInt offset drifted");
static_assert(offsetof(LogData, format)       == 496,
              "LogData.format must land at byte 496 (legacy pad after szKey)");
static_assert(offsetof(LogData, logPayload)   == 500,
              "LogData.logPayload offset drifted");

constexpr std::uint16_t LP_LOG = 0;

// Pull a NUL-or-buffer-bounded string off a fixed C array.
std::string ReadFixedString(const char* p, std::size_t cap)
{
    const auto n = std::find(p, p + cap, '\0') - p;
    return std::string(p, static_cast<std::size_t>(n));
}

std::string FormatTimestamp(const DbTimestamp& ts)
{
    // MSSQL DATETIME accepts "YYYY-MM-DD HH:MM:SS"; if fields are
    // zero (uninitialized timestamp from an older sender), insert a
    // sentinel "1970-01-01 00:00:00" the DB will accept.
    std::ostringstream os;
    if (ts.year < 1900 || ts.year > 9999)
    {
        os << "1970-01-01 00:00:00";
        return os.str();
    }
    auto pad2 = [&os](unsigned v) {
        if (v < 10) os << '0';
        os << v;
    };
    os << ts.year << '-';
    pad2(ts.month); os << '-';
    pad2(ts.day); os << ' ';
    pad2(ts.hour); os << ':';
    pad2(ts.minute); os << ':';
    pad2(ts.second);
    return os.str();
}

bool DecodeRecord(const std::byte* data, std::size_t len, LogRecord& out)
{
    // The wire-faithful sender (UdpAuditLogger) trims the datagram to
    // exactly udp_overhead + log_prefix [+ optional blob], not the
    // full 1064-byte sizeof(UdpPacket). Decode against the declared
    // pkt.size, not the static struct size.
    constexpr std::size_t udp_overhead = offsetof(UdpPacket, payload);
    constexpr std::size_t log_prefix   = offsetof(LogData, logPayload);
    if (len < udp_overhead + log_prefix) return false;

    UdpPacket pkt{};
    std::memcpy(&pkt, data, udp_overhead);
    if (pkt.command != LP_LOG) return false;
    if (pkt.size < udp_overhead + log_prefix) return false;
    if (pkt.size > len) return false;  // declared larger than received

    LogData ld{};
    std::memcpy(&ld, data + udp_overhead, log_prefix);
    out.timestamp_iso = FormatTimestamp(ld.timestamp);
    out.server_id     = ld.serverId;
    out.client_ip     = ReadFixedString(ld.clientIp, sizeof(ld.clientIp));
    out.action        = ld.action;
    out.map_id        = ld.mapId;
    out.pos_x         = ld.posX;
    out.pos_y         = ld.posY;
    out.pos_z         = ld.posZ;
    for (int i = 0; i < 11; ++i) out.search_int[i] = ld.searchKeyInt[i];
    for (int i = 0; i < 7;  ++i)
        out.search_str[i] = ReadFixedString(ld.searchKeyStr[i],
                                            sizeof(ld.searchKeyStr[i]));
    out.format        = ld.format;
    // Payload tail — only the bytes actually transmitted past the prefix.
    const std::size_t total_payload = pkt.size - udp_overhead;
    const std::size_t blob_len = (total_payload > log_prefix)
        ? (total_payload - log_prefix) : 0;
    if (blob_len > 0 && blob_len <= sizeof(ld.logPayload))
    {
        const std::byte* blob_start = data + udp_overhead + log_prefix;
        out.payload.assign(blob_start, blob_start + blob_len);
    }
    return true;
}

} // namespace

LogServer::LogServer(boost::asio::io_context& io, LogServerConfig config)
    : m_io(io)
    , m_socket(io)
    , m_port(config.port)
    , m_sink(config.sink)
{
    using boost::asio::ip::udp;
    udp::endpoint ep(boost::asio::ip::make_address(config.bind_address),
                     m_port);
    m_socket.open(ep.protocol());
    m_socket.bind(ep);
    m_port = m_socket.local_endpoint().port();
}

boost::asio::awaitable<void>
LogServer::Run()
{
    using namespace boost::asio;
    std::array<std::byte, 4096> buf{};
    ip::udp::endpoint sender;
    while (m_socket.is_open())
    {
        boost::system::error_code ec;
        const auto n = co_await m_socket.async_receive_from(
            buffer(buf.data(), buf.size()), sender,
            redirect_error(use_awaitable, ec));
        if (ec) break;

        m_received.fetch_add(1);
        LogRecord rec{};
        if (!DecodeRecord(buf.data(), n, rec))
        {
            m_drops_format.fetch_add(1);
            spdlog::debug("log_server: dropped {} bytes from {} (bad format)",
                n, sender.address().to_string());
            continue;
        }
        if (m_sink != nullptr)
            m_sink->Write(rec);
    }
}

} // namespace tlogsvr
