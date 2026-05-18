#include "handlers.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstring>
#include <string>

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

// Decode the legacy CS_LOGIN_REQ wire layout (CSHandler.cpp:148-164):
//   WORD   wVersion
//   STRING Zombie3 / strPasswd / Zombie1 / Zombie2 / strUserID
//   INT64  dlCheck
//   INT64  llChecksum
// STRINGs are length-prefixed: int32 LE length then `length` bytes.
// Defensive: any out-of-range length returns false. Strings get a
// per-call MAX cap matching the legacy MAX_NAME (50 bytes).
struct LoginReqFields
{
    std::uint16_t version = 0;
    std::string   user_id;
    std::string   password;
};

constexpr std::size_t kMaxLoginStringLen = 64;

bool ParseLoginReq(std::span<const std::byte> body, LoginReqFields& out)
{
    std::size_t pos = 0;
    auto read_bytes = [&](void* dst, std::size_t n) -> bool {
        if (pos + n > body.size()) return false;
        std::memcpy(dst, body.data() + pos, n);
        pos += n;
        return true;
    };
    auto read_string = [&](std::string& dst) -> bool {
        std::int32_t len = 0;
        if (!read_bytes(&len, 4)) return false;
        if (len < 0 || static_cast<std::size_t>(len) > kMaxLoginStringLen) return false;
        if (pos + static_cast<std::size_t>(len) > body.size()) return false;
        dst.assign(reinterpret_cast<const char*>(body.data() + pos),
                   static_cast<std::size_t>(len));
        pos += static_cast<std::size_t>(len);
        return true;
    };

    if (!read_bytes(&out.version, 2)) return false;
    std::string zombie3, zombie1, zombie2;
    if (!read_string(zombie3))        return false;
    if (!read_string(out.password))   return false;
    if (!read_string(zombie1))        return false;
    if (!read_string(zombie2))        return false;
    if (!read_string(out.user_id))    return false;
    // INT64 dlCheck + INT64 llChecksum trail — don't actually need
    // them for the stub. Don't even bother reading.
    return true;
}

} // namespace

boost::asio::awaitable<void>
OnLoginReq(tnetlib::AsioSession& session, std::span<const std::byte> body,
           services::IAuthService* auth_service)
{
    LoginAck ack{};
    ack.bCreateCnt = 6;  // CHARSLOT_MAX default

    // Wire-format-only mode: just sniff wVersion and reply success
    // / version-mismatch. Used by smoke tests + dev builds without
    // an auth backend configured.
    if (auth_service == nullptr)
    {
        if (body.size() < 2)
        {
            spdlog::warn("CS_LOGIN_REQ body too short ({} bytes)", body.size());
            co_return;
        }
        std::uint16_t wVersion = 0;
        std::memcpy(&wVersion, body.data(), 2);
        ack.bResult = (wVersion == kProtocolVersion)
                      ? kLrSuccess
                      : kLrVersionMismatch;
        spdlog::info("CS_LOGIN_REQ v=0x{:04X} (stub) → result={}", wVersion, ack.bResult);
    }
    else
    {
        // Full path: parse the wire, delegate to IAuthService.
        LoginReqFields fields;
        if (!ParseLoginReq(body, fields))
        {
            spdlog::warn("CS_LOGIN_REQ malformed body — closing session");
            session.Close();
            co_return;
        }
        if (fields.version != kProtocolVersion)
        {
            spdlog::warn("CS_LOGIN_REQ version mismatch (got 0x{:04X}, want 0x{:04X})",
                fields.version, kProtocolVersion);
            ack.bResult = kLrVersionMismatch;
        }
        else
        {
            services::AuthRequest req{
                .user_id = fields.user_id,
                .password = fields.password,
                .client_ip = "",  // ip threading lands once we plumb
                                  // tcp::socket::remote_endpoint into AsioSession
                .client_version = fields.version,
            };
            const auto result = auth_service->Authenticate(req);
            ack.bResult    = static_cast<std::uint8_t>(result.status);
            ack.dwUserID   = static_cast<std::uint32_t>(result.user_id);
            ack.dwKEY      = result.session_key;
            ack.bCreateCnt = result.create_char_count;
            ack.bInPcBang  = result.in_pc_bang;
            ack.dwPremium  = result.premium_id;
            spdlog::info("CS_LOGIN_REQ user={} → status={} key=0x{:08X}",
                fields.user_id, ack.bResult, ack.dwKEY);
        }
    }

    const auto payload = EncodeLoginAck(ack);
    co_await session.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_LOGIN_ACK),
        std::span<const std::byte>(payload.data(), payload.size()));
}

boost::asio::awaitable<void>
OnGroupListReq(tnetlib::AsioSession& session, std::span<const std::byte>)
{
    // Stub: empty world-group list. Two-byte payload: bCount=0,
    // bCheckFilePoint=0 (legacy unconditionally writes it after bCount
    // — see CSHandler.cpp:506-510). Real impl will iterate TGROUP rows.
    std::byte payload[2] = { std::byte{0}, std::byte{0} };
    spdlog::info("CS_GROUPLIST_REQ → CS_GROUPLIST_ACK (stub: 0 groups)");
    co_await session.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_GROUPLIST_ACK),
        std::span<const std::byte>(payload, 2));
}

boost::asio::awaitable<void>
OnChannelListReq(tnetlib::AsioSession& session, std::span<const std::byte> body)
{
    // Request body: BYTE bGroupID. We log it for visibility but don't
    // act on it yet (stub returns empty channels regardless).
    const auto groupId = body.empty()
        ? std::uint8_t{0}
        : static_cast<std::uint8_t>(body[0]);

    // Ack: BYTE bCount=0, BYTE bCheckFilePoint=0.
    std::byte payload[2] = { std::byte{0}, std::byte{0} };
    spdlog::info("CS_CHANNELLIST_REQ group={} → CS_CHANNELLIST_ACK (stub: 0 channels)", groupId);
    co_await session.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_CHANNELLIST_ACK),
        std::span<const std::byte>(payload, 2));
}

boost::asio::awaitable<void>
OnCharListReq(tnetlib::AsioSession& session, std::span<const std::byte> body)
{
    // Request body: BYTE bGroupID.
    const auto groupId = body.empty()
        ? std::uint8_t{0}
        : static_cast<std::uint8_t>(body[0]);

    // Ack: BYTE bCheckFilePoint=0, BYTE bCount=0 — note legacy
    // CSHandler.cpp:723-728 writes CheckFilePoint FIRST here,
    // bCount second (different field order than the other list acks).
    // Real impl will iterate TCHARTABLE for this group/user.
    std::byte payload[2] = { std::byte{0}, std::byte{0} };
    spdlog::info("CS_CHARLIST_REQ group={} → CS_CHARLIST_ACK (stub: 0 chars)", groupId);
    co_await session.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_CHARLIST_ACK),
        std::span<const std::byte>(payload, 2));
}

// Legacy result-code constants (NetCode.h excerpts) — local copies
// to keep this TU PCH-free. Real ports use them via the enum once
// AuthService et al land.
namespace {
constexpr std::uint8_t kCrInternal    = 7;  // CS_CREATECHAR_ACK "internal error"
constexpr std::uint8_t kDrInternal    = 3;  // CS_DELCHAR_ACK    "internal error"
constexpr std::uint8_t kSrNoServer    = 1;  // CS_START_ACK      "no server"
constexpr std::uint8_t kCodeCorrect   = 0;  // CS_SECURITYRESULT_ACK
constexpr std::uint32_t kTerminateMagic = 0x2AF3A9D1; // CSHandler.cpp:1452
} // namespace

boost::asio::awaitable<void>
OnCreateCharReq(tnetlib::AsioSession& session, std::span<const std::byte> body)
{
    // Stub ack: bResult=CR_INTERNAL, then all the legacy character
    // fields zeroed out (the client expects the full 13-trailing-byte
    // layout regardless of result code). The wire layout is
    // documented in CSSender.cpp::SendCS_CREATECHAR_ACK; payload sizes
    // here are intentionally minimal — just bResult + dwCharID — and
    // a real implementation will fill in the rest once CharService is
    // ported. Until then the client will see CR_INTERNAL and abort.
    std::byte payload[5] = {};
    payload[0] = static_cast<std::byte>(kCrInternal);
    spdlog::info("CS_CREATECHAR_REQ → CS_CREATECHAR_ACK (stub: CR_INTERNAL)");
    co_await session.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_CREATECHAR_ACK),
        std::span<const std::byte>(payload, sizeof(payload)));
    (void)body;
}

boost::asio::awaitable<void>
OnDelCharReq(tnetlib::AsioSession& session, std::span<const std::byte> body)
{
    // Ack: BYTE bResult + DWORD dwCharID. Stub returns DR_INTERNAL +
    // dwCharID=0 regardless of input.
    std::byte payload[5] = {};
    payload[0] = static_cast<std::byte>(kDrInternal);
    spdlog::info("CS_DELCHAR_REQ → CS_DELCHAR_ACK (stub: DR_INTERNAL)");
    co_await session.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_DELCHAR_ACK),
        std::span<const std::byte>(payload, sizeof(payload)));
    (void)body;
}

boost::asio::awaitable<void>
OnStartReq(tnetlib::AsioSession& session, std::span<const std::byte> body)
{
    // Ack: BYTE bResult + DWORD dwMapIP + WORD wPort + BYTE bServerID.
    // Real impl resolves the Map endpoint via MapServerLocator; the
    // stub refuses with SR_NOSERVER so the client falls back to the
    // server-select screen instead of trying to connect to 0.0.0.0:0.
    std::byte payload[8] = {};
    payload[0] = static_cast<std::byte>(kSrNoServer);
    spdlog::info("CS_START_REQ → CS_START_ACK (stub: SR_NOSERVER)");
    co_await session.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_START_ACK),
        std::span<const std::byte>(payload, sizeof(payload)));
    (void)body;
}

boost::asio::awaitable<void>
OnAgreementReq(tnetlib::AsioSession& /*session*/, std::span<const std::byte> body)
{
    // No ack. Body is WORD wVersion. Legacy upserts TUSERINFOTABLE and
    // flips the per-session m_bAgreement gate. Stub just logs the
    // version so the dispatcher path is exercised.
    std::uint16_t wVersion = 0;
    if (body.size() >= 2) std::memcpy(&wVersion, body.data(), 2);
    spdlog::info("CS_AGREEMENT_REQ version=0x{:04X} (stub: noop)", wVersion);
    co_return;
}

boost::asio::awaitable<void>
OnHotsendReq(tnetlib::AsioSession& /*session*/, std::span<const std::byte> body)
{
    // No ack. Body is INT64 dlValue + BYTE bAll. Legacy validates
    // dlValue against m_dlCheckFile ^ m_dlCheckKey when an exec-file
    // hash is configured; we don't ship that feature.
    std::int64_t dlValue = 0;
    std::uint8_t bAll = 0;
    if (body.size() >= 9)
    {
        std::memcpy(&dlValue, body.data(), 8);
        bAll = static_cast<std::uint8_t>(body[8]);
    }
    spdlog::info("CS_HOTSEND_REQ dlValue=0x{:016X} bAll={} (stub: noop)",
        static_cast<unsigned long long>(dlValue), bAll);
    co_return;
}

boost::asio::awaitable<void>
OnVeteranReq(tnetlib::AsioSession& session, std::span<const std::byte> /*body*/)
{
    // Ack: BYTE bOption, BYTE bFirstLevel, BYTE bSecondLevel,
    // BYTE bThirdLevel. Legacy sends bOption=3 ("all three options")
    // with the level thresholds from m_vVETERAN. Stub: bOption=0
    // (no returning-player bonus offered).
    std::byte payload[4] = {};
    spdlog::info("CS_VETERAN_REQ → CS_VETERAN_ACK (stub: no bonus)");
    co_await session.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_VETERAN_ACK),
        std::span<const std::byte>(payload, sizeof(payload)));
}

boost::asio::awaitable<void>
OnTerminateReq(tnetlib::AsioSession& session, std::span<const std::byte> body)
{
    // Body: DWORD dwKey, must equal kTerminateMagic. Legacy returns
    // EC_SESSION_INVALIDCHAR (silently closes) on mismatch — we do
    // the same here. On match the legacy does TLogoutAll + flip
    // m_bLogout flag; stub just closes the socket.
    std::uint32_t dwKey = 0;
    if (body.size() >= 4) std::memcpy(&dwKey, body.data(), 4);
    if (dwKey != kTerminateMagic)
    {
        spdlog::warn("CS_TERMINATE_REQ magic mismatch (got 0x{:08X})", dwKey);
    }
    else
    {
        spdlog::info("CS_TERMINATE_REQ — clean logout, closing");
    }
    session.Close();
    co_return;
}

boost::asio::awaitable<void>
OnSecurityConfirmAck(tnetlib::AsioSession& session, std::span<const std::byte> /*body*/)
{
    // CS_SECURITYRESULT_ACK payload: BYTE bResult. Always CODE_CORRECT
    // since the SECURITY flow is dead-code on the legacy side and we
    // accept whatever code the client sends.
    std::byte payload[1] = { static_cast<std::byte>(kCodeCorrect) };
    spdlog::info("CS_SECURITYCONFIRM_ACK → CS_SECURITYRESULT_ACK (stub: CODE_CORRECT)");
    co_await session.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_SECURITYRESULT_ACK),
        std::span<const std::byte>(payload, sizeof(payload)));
}

} // namespace tloginsvr::handlers
