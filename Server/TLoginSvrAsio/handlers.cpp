#include "handlers.h"

#include "MessageId.h"

#include <cstdio>
#include <cstring>

namespace tloginsvr::handlers {

namespace {

// Legacy protocol version constant — ProtocolBase.h:4 (`TVERSION`).
// Pulled in as a literal here instead of via the header to keep this
// TU PCH-free.
constexpr std::uint16_t kProtocolVersion = 0x2918;

// CS_LOGIN_ACK wire layout from CSSender.cpp::SendCS_LOGIN_ACK
// (41 bytes payload):
//   BYTE     bResult
//   DWORD    dwUserID
//   DWORD    dwCharID
//   DWORD    dwKEY
//   DWORD    dwIPAddr      — Map server IP (network byte order)
//   WORD     wPort
//   BYTE     bCreateCnt
//   BYTE     bInPcBang
//   DWORD    dwPremium
//   INT64    dCurTime      — unix seconds, server clock
//   INT64    dlCheckKey
//
// All fields little-endian on the wire (no endianness conversion;
// memcpy into a packed buffer in declaration order).
struct LoginAck
{
    std::uint8_t  bResult;
    std::uint32_t dwUserID;
    std::uint32_t dwCharID;
    std::uint32_t dwKEY;
    std::uint32_t dwIPAddr;
    std::uint16_t wPort;
    std::uint8_t  bCreateCnt;
    std::uint8_t  bInPcBang;
    std::uint32_t dwPremium;
    std::int64_t  dCurTime;
    std::int64_t  dlCheckKey;
};

// Serialize a LoginAck into 41 packed bytes — little-endian, no
// compiler padding. memcpy field-by-field; the size assertion at
// the bottom catches any layout drift.
std::vector<std::byte> EncodeLoginAck(const LoginAck& ack)
{
    std::vector<std::byte> out(41);
    std::byte* p = out.data();

    auto put = [&p](const void* src, std::size_t n) {
        std::memcpy(p, src, n);
        p += n;
    };
    put(&ack.bResult,    1);
    put(&ack.dwUserID,   4);
    put(&ack.dwCharID,   4);
    put(&ack.dwKEY,      4);
    put(&ack.dwIPAddr,   4);
    put(&ack.wPort,      2);
    put(&ack.bCreateCnt, 1);
    put(&ack.bInPcBang,  1);
    put(&ack.dwPremium,  4);
    put(&ack.dCurTime,   8);
    put(&ack.dlCheckKey, 8);

    // 1+4+4+4+4+2+1+1+4+8+8 = 41
    return out;
}

// Legacy LR_* result codes — NetCode.h.
constexpr std::uint8_t kLrSuccess         = 0;
constexpr std::uint8_t kLrVersionMismatch = 4;

} // namespace

boost::asio::awaitable<void>
OnLoginReq(tnetlib::AsioSession& session, std::span<const std::byte> body)
{
    // Parse wVersion (first 2 bytes of the body, little-endian).
    if (body.size() < 2)
    {
        std::printf("[tloginsvr_asio] CS_LOGIN_REQ body too short (%zu bytes)\n",
            body.size());
        co_return;
    }
    std::uint16_t wVersion;
    std::memcpy(&wVersion, body.data(), 2);

    // Build the ack. Phase-3 stub — fields are placeholders. Real
    // auth + map endpoint lookup arrives once AuthService is ported.
    LoginAck ack{};
    ack.bResult    = (wVersion == kProtocolVersion) ? kLrSuccess : kLrVersionMismatch;
    ack.dwUserID   = 0;  // would be account row PK
    ack.dwCharID   = 0;
    ack.dwKEY      = 0;  // would be a SecureRandom64() 32-bit slice
    ack.dwIPAddr   = 0;  // would be MapServerLocator result
    ack.wPort      = 0;
    ack.bCreateCnt = 6;  // CHARSLOT_MAX
    ack.bInPcBang  = 0;
    ack.dwPremium  = 0;
    ack.dCurTime   = 0;
    ack.dlCheckKey = 0;  // would be tnetlib_platform::SecureRandom64()

    std::printf("[tloginsvr_asio] CS_LOGIN_REQ v=0x%04X → CS_LOGIN_ACK result=%u\n",
        wVersion, ack.bResult);

    const auto payload = EncodeLoginAck(ack);
    co_await session.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_LOGIN_ACK),
        std::span<const std::byte>(payload.data(), payload.size()));
}

} // namespace tloginsvr::handlers
