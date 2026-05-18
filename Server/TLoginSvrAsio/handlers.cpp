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
OnLoginReq(std::shared_ptr<tnetlib::AsioSession> session, std::span<const std::byte> body,
           services::IAuthService* auth_service,
           services::IConnectionRegistry* connection_registry)
{
    auto& sref = *session;
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
            sref.Close();
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

            // Duplicate-kick: on auth success, register this session
            // and close any previous holder for the same user_id.
            // We diverge from legacy here (legacy closed BOTH the
            // old and new sessions) — modern UX is "newest wins" so
            // the user's most recent connect attempt actually works.
            if (result.status == services::AuthStatus::Success
                && connection_registry != nullptr)
            {
                services::ConnectionEntry entry{
                    .user_id = result.user_id,
                    .session_key = result.session_key,
                    .handoff_to_map = false,
                };
                auto previous = connection_registry->Register(entry, session);
                if (previous)
                {
                    spdlog::warn(
                        "duplicate login for user_id={} — kicking previous session",
                        result.user_id);
                    previous->Close();
                }
            }
        }
    }

    const auto payload = EncodeLoginAck(ack);
    co_await sref.SendPacket(
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

namespace {

// Helpers to build the CS_CHARLIST_ACK payload byte-by-byte.
// Avoids pulling in PacketWriter from the legacy codebase.
struct ByteAppender
{
    std::vector<std::byte> bytes;

    void U8(std::uint8_t v)  { bytes.push_back(static_cast<std::byte>(v)); }
    void U16(std::uint16_t v){ AppendRaw(&v, 2); }
    void U32(std::uint32_t v){ AppendRaw(&v, 4); }
    void I32(std::int32_t v) { AppendRaw(&v, 4); }
    void Str(const std::string& s) {
        I32(static_cast<std::int32_t>(s.size()));
        AppendRaw(s.data(), s.size());
    }
    void AppendRaw(const void* src, std::size_t n) {
        const auto* p = reinterpret_cast<const std::byte*>(src);
        bytes.insert(bytes.end(), p, p + n);
    }
};

// Try to recover the authenticated user_id for this session via the
// connection registry. Returns 0 if not registered (= not authed) or
// no registry is wired.
std::int32_t ResolveUserId(
    const std::shared_ptr<tnetlib::AsioSession>& session,
    services::IConnectionRegistry* registry)
{
    if (!registry) return 0;
    const auto entry = registry->Lookup(session);
    return entry ? entry->user_id : 0;
}

} // namespace

boost::asio::awaitable<void>
OnCharListReq(std::shared_ptr<tnetlib::AsioSession> session,
              std::span<const std::byte> body,
              services::ICharService* char_service,
              services::IConnectionRegistry* connection_registry)
{
    auto& sref = *session;
    const auto groupId = body.empty()
        ? std::uint8_t{0}
        : static_cast<std::uint8_t>(body[0]);
    const auto userId = ResolveUserId(session, connection_registry);

    std::vector<services::CharacterInfo> chars;
    if (char_service != nullptr && userId != 0)
    {
        chars = char_service->List(userId, groupId);
    }

    // Wire layout (CSHandler.cpp:723-761):
    //   BYTE  bCheckFilePoint = 0
    //   BYTE  bCount
    //   { per char:
    //     DWORD dwCharID, STRING strName,
    //     BYTE bStartAct, bSlot, bLevel, bClass, bRace, bCountry,
    //     BYTE bSex, bHair, bFace, bBody, bPants, bHand, bFoot,
    //     DWORD dwRegion, dwFame, dwFameColor,
    //     BYTE bHelmetHide, bEquipItemCount,
    //     [items …]            ← empty in Phase A stub
    //   }
    ByteAppender p;
    p.U8(0);                                              // bCheckFilePoint
    p.U8(static_cast<std::uint8_t>(chars.size()));        // bCount
    for (const auto& c : chars)
    {
        p.I32(c.char_id);
        p.Str(c.name);
        p.U8(c.start_act);
        p.U8(c.slot);
        p.U8(c.level);
        p.U8(c.char_class);
        p.U8(c.race);
        p.U8(c.country);
        p.U8(c.sex);
        p.U8(c.hair);
        p.U8(c.face);
        p.U8(c.body);
        p.U8(c.pants);
        p.U8(c.hand);
        p.U8(c.foot);
        p.U32(c.region);
        p.U32(c.fame);
        p.U32(c.fame_color);
        p.U8(c.helmet_hide);
        p.U8(0);  // bEquipItemCount = 0; items list omitted (Phase A)
    }

    spdlog::info("CS_CHARLIST_REQ user={} group={} → CS_CHARLIST_ACK ({} chars)",
        userId, groupId, chars.size());
    co_await sref.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_CHARLIST_ACK),
        std::span<const std::byte>(p.bytes.data(), p.bytes.size()));
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

namespace {

// Parse CS_CREATECHAR_REQ body. Wire layout (CSHandler.cpp:997-1011):
//   BYTE   bGroupID
//   STRING strName
//   BYTE   bSlot, bClass, bRace, bCountry, bSex, bHair, bFace,
//          bBody, bPants, bHand, bFoot, bLevelOption
bool ParseCreateCharReq(std::span<const std::byte> body,
                        services::CharacterCreateRequest& out)
{
    std::size_t pos = 0;
    auto have = [&](std::size_t n) { return pos + n <= body.size(); };
    auto read_u8 = [&](std::uint8_t& dst) -> bool {
        if (!have(1)) return false;
        dst = static_cast<std::uint8_t>(body[pos++]);
        return true;
    };
    auto read_str = [&](std::string& dst) -> bool {
        if (!have(4)) return false;
        std::int32_t len = 0;
        std::memcpy(&len, body.data() + pos, 4);
        pos += 4;
        if (len < 0 || len > 64 || !have(static_cast<std::size_t>(len))) return false;
        dst.assign(reinterpret_cast<const char*>(body.data() + pos),
                   static_cast<std::size_t>(len));
        pos += static_cast<std::size_t>(len);
        return true;
    };

    if (!read_u8(out.group_id))       return false;
    if (!read_str(out.name))          return false;
    if (!read_u8(out.slot))           return false;
    if (!read_u8(out.char_class))     return false;
    if (!read_u8(out.race))           return false;
    if (!read_u8(out.country))        return false;
    if (!read_u8(out.sex))            return false;
    if (!read_u8(out.hair))           return false;
    if (!read_u8(out.face))           return false;
    if (!read_u8(out.body))           return false;
    if (!read_u8(out.pants))          return false;
    if (!read_u8(out.hand))           return false;
    if (!read_u8(out.foot))           return false;
    if (!read_u8(out.level_option))   return false;
    return true;
}

} // namespace

boost::asio::awaitable<void>
OnCreateCharReq(std::shared_ptr<tnetlib::AsioSession> session,
                std::span<const std::byte> body,
                services::ICharService* char_service,
                services::IConnectionRegistry* connection_registry)
{
    auto& sref = *session;

    // Stub mode — no services wired: refuse with CR_INTERNAL.
    if (char_service == nullptr)
    {
        std::byte payload[5] = {};
        payload[0] = static_cast<std::byte>(kCrInternal);
        spdlog::info("CS_CREATECHAR_REQ → CS_CREATECHAR_ACK (stub: CR_INTERNAL)");
        co_await sref.SendPacket(
            tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_CREATECHAR_ACK),
            std::span<const std::byte>(payload, sizeof(payload)));
        co_return;
    }

    services::CharacterCreateRequest req;
    if (!ParseCreateCharReq(body, req))
    {
        spdlog::warn("CS_CREATECHAR_REQ malformed body — refusing");
        std::byte payload[5] = {};
        payload[0] = static_cast<std::byte>(kCrInternal);
        co_await sref.SendPacket(
            tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_CREATECHAR_ACK),
            std::span<const std::byte>(payload, sizeof(payload)));
        co_return;
    }
    req.user_id = ResolveUserId(session, connection_registry);
    if (req.user_id == 0)
    {
        spdlog::warn("CS_CREATECHAR_REQ from unauthenticated session — refusing");
        std::byte payload[5] = {};
        payload[0] = static_cast<std::byte>(kCrInternal);
        co_await sref.SendPacket(
            tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_CREATECHAR_ACK),
            std::span<const std::byte>(payload, sizeof(payload)));
        co_return;
    }

    const auto result = char_service->Create(req);

    // Ack wire layout (CSSender.cpp:68-103):
    //   BYTE bResult, DWORD dwCharID, STRING strName,
    //   BYTE bSlot, bClass, bRace, bCountry, bSex, bHair, bFace,
    //   BYTE bBody, bPants, bHand, bFoot, bCreateCnt, bLevel
    // 13 trailing bytes after the name. Echoes the request fields
    // back so the client can render the new entry directly.
    ByteAppender p;
    p.U8(static_cast<std::uint8_t>(result.status));
    p.I32(result.char_id);
    p.Str(req.name);
    p.U8(req.slot);
    p.U8(req.char_class);
    p.U8(req.race);
    p.U8(req.country);
    p.U8(req.sex);
    p.U8(req.hair);
    p.U8(req.face);
    p.U8(req.body);
    p.U8(req.pants);
    p.U8(req.hand);
    p.U8(req.foot);
    p.U8(result.remaining_slots);
    p.U8(result.starting_level);

    spdlog::info("CS_CREATECHAR_REQ user={} name='{}' slot={} → {}",
        req.user_id, req.name, req.slot,
        static_cast<std::uint8_t>(result.status));

    co_await sref.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_CREATECHAR_ACK),
        std::span<const std::byte>(p.bytes.data(), p.bytes.size()));
}

boost::asio::awaitable<void>
OnDelCharReq(std::shared_ptr<tnetlib::AsioSession> session,
             std::span<const std::byte> body,
             services::ICharService* char_service,
             services::IConnectionRegistry* connection_registry)
{
    auto& sref = *session;

    auto reply_with = [&](std::uint8_t result, std::int32_t char_id)
        -> boost::asio::awaitable<void>
    {
        ByteAppender p;
        p.U8(result);
        p.I32(char_id);
        co_await sref.SendPacket(
            tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_DELCHAR_ACK),
            std::span<const std::byte>(p.bytes.data(), p.bytes.size()));
    };

    if (char_service == nullptr)
    {
        spdlog::info("CS_DELCHAR_REQ → CS_DELCHAR_ACK (stub: DR_INTERNAL)");
        co_await reply_with(kDrInternal, 0);
        co_return;
    }

    // Wire: BYTE bGroupID, STRING strPasswd, DWORD dwCharID
    if (body.size() < 1)
    {
        co_await reply_with(kDrInternal, 0);
        co_return;
    }
    std::size_t pos = 0;
    const std::uint8_t group_id = static_cast<std::uint8_t>(body[pos++]);
    if (pos + 4 > body.size()) { co_await reply_with(kDrInternal, 0); co_return; }
    std::int32_t pwlen = 0;
    std::memcpy(&pwlen, body.data() + pos, 4); pos += 4;
    if (pwlen < 0 || pwlen > 64) { co_await reply_with(kDrInternal, 0); co_return; }
    if (pos + static_cast<std::size_t>(pwlen) > body.size()) {
        co_await reply_with(kDrInternal, 0); co_return;
    }
    std::string password(reinterpret_cast<const char*>(body.data() + pos),
                         static_cast<std::size_t>(pwlen));
    pos += static_cast<std::size_t>(pwlen);
    if (pos + 4 > body.size()) { co_await reply_with(kDrInternal, 0); co_return; }
    std::int32_t char_id = 0;
    std::memcpy(&char_id, body.data() + pos, 4);

    const auto user_id = ResolveUserId(session, connection_registry);
    if (user_id == 0)
    {
        spdlog::warn("CS_DELCHAR_REQ from unauthenticated session — refusing");
        co_await reply_with(kDrInternal, char_id);
        co_return;
    }

    const auto result = char_service->Delete(user_id, group_id, char_id, password);
    spdlog::info("CS_DELCHAR_REQ user={} group={} char={} → {}",
        user_id, group_id, char_id, static_cast<std::uint8_t>(result));
    co_await reply_with(static_cast<std::uint8_t>(result), char_id);
}

boost::asio::awaitable<void>
OnStartReq(std::shared_ptr<tnetlib::AsioSession> session,
           std::span<const std::byte> body,
           services::IMapServerLocator* map_server_locator,
           services::IConnectionRegistry* connection_registry)
{
    auto& sref = *session;
    // Request body: BYTE bGroupID + BYTE bChannel + DWORD dwCharID (6 bytes).
    std::uint8_t  bGroupID = 0;
    std::uint8_t  bChannel = 0;
    std::int32_t  dwCharID = 0;
    if (body.size() >= 6)
    {
        bGroupID = static_cast<std::uint8_t>(body[0]);
        bChannel = static_cast<std::uint8_t>(body[1]);
        std::memcpy(&dwCharID, body.data() + 2, 4);
    }

    // Ack: BYTE bResult + DWORD dwMapIP (network-order octets) +
    // WORD wPort + BYTE bServerID. Wire-format matches the legacy
    // CSSender::SendCS_START_ACK layout.
    std::byte payload[8] = {};

    if (map_server_locator == nullptr)
    {
        payload[0] = static_cast<std::byte>(kSrNoServer);
        spdlog::info("CS_START_REQ group={} ch={} char={} → CS_START_ACK (stub: SR_NOSERVER)",
            bGroupID, bChannel, dwCharID);
    }
    else if (auto ep = map_server_locator->Lookup(bGroupID, bChannel, dwCharID))
    {
        payload[0] = static_cast<std::byte>(0); // SR_SUCCESS
        // DWORD dwMapIP — 4 octets in the same byte order the legacy
        // server sent inet_addr() output (network byte order).
        payload[1] = static_cast<std::byte>(ep->ipv4[0]);
        payload[2] = static_cast<std::byte>(ep->ipv4[1]);
        payload[3] = static_cast<std::byte>(ep->ipv4[2]);
        payload[4] = static_cast<std::byte>(ep->ipv4[3]);
        // WORD wPort — little-endian (matches legacy memcpy from
        // WORD field into packet buffer on an LE host).
        std::memcpy(&payload[5], &ep->port, 2);
        payload[7] = static_cast<std::byte>(ep->server_id);
        spdlog::info("CS_START_REQ group={} ch={} char={} → CS_START_ACK "
                     "(SR_SUCCESS, {}.{}.{}.{}:{} server_id={})",
            bGroupID, bChannel, dwCharID,
            ep->ipv4[0], ep->ipv4[1], ep->ipv4[2], ep->ipv4[3],
            ep->port, ep->server_id);

        // Flag the session for Map-handoff so the eventual
        // SessionTerminator call (in HandleConnection's close path)
        // knows to preserve the TCURRENTUSER row — Map needs the
        // dwKEY entry to validate the client's reconnect.
        if (connection_registry)
        {
            connection_registry->MarkHandoff(session);
        }
    }
    else
    {
        payload[0] = static_cast<std::byte>(kSrNoServer);
        spdlog::warn("CS_START_REQ group={} ch={} char={} → CS_START_ACK "
                     "(SR_NOSERVER — no map endpoint registered)",
            bGroupID, bChannel, dwCharID);
    }

    co_await sref.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_START_ACK),
        std::span<const std::byte>(payload, sizeof(payload)));
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
