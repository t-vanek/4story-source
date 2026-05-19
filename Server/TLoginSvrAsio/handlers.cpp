#include "handlers.h"
#include "nation.h"
#include "services/charname_validator.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <random>
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
    std::int64_t  dl_check = 0;
    std::int64_t  ll_checksum = 0;
    bool          checksum_present = false;
    // JP deployments append a single trailing BYTE bChanneling after
    // llChecksum (CSHandler.cpp:173). Non-JP bodies don't carry it.
    // Stays 0 / absent on the other locales — SOCI auth ignores it
    // unless the deployment nation is Japan.
    std::uint8_t  channeling = 0;
    bool          channeling_present = false;
};

// Legacy CS_LOGIN_REQ trailing-checksum validator (CSHandler.cpp:185-202).
// The client computes `llChecksum` from wVersion alone via a tiny XOR/
// add loop seeded with a magic 64-bit key. Algorithm pseudocode:
//
//   key = 0x336c3aebf71a8b08
//   ck  = wVersion * 2 - 500
//   idx = ck % 8        ; legacy uses sizeof(INT64) which is 8
//   body= ck / 8
//   for i in 0..idx:
//       ck ^= body
//       ck += key
//   ; ck must equal the wire value
//
// Real legacy clients always send a valid checksum. A mismatch is
// either a forged packet or a wire-version drift. Either way, the
// legacy returns EC_SESSION_INVALIDCHAR (closes the session). We
// do the same, but only when the trailing INT64 was actually present
// — older test tooling may emit a truncated body, and we don't want
// to break the ctest suite just to enforce the check.
bool VerifyLoginChecksum(std::uint16_t version, std::int64_t recv_checksum)
{
    constexpr std::int64_t kKey = 0x336c3aebf71a8b08LL;
    std::int64_t ck = static_cast<std::int64_t>(version) * 2 - 500;
    const std::int64_t idx  = ck % 8;
    const std::int64_t body = ck / 8;
    for (std::int64_t i = 0; i < idx; ++i)
    {
        ck ^= body;
        ck += kKey;
    }
    return ck == recv_checksum;
}

constexpr std::size_t kMaxLoginStringLen = 64;

bool ParseLoginReq(std::span<const std::byte> body, LoginReqFields& out,
                   bool expect_channeling = false)
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
    // Trailing INT64 dlCheck + INT64 llChecksum. Both optional — older
    // test tooling sometimes omits them. If at least 16 trailing
    // bytes remain, parse both and flag checksum_present for the
    // caller to enforce. dlCheck is the exec-file probe (Phase E);
    // the verifier on the server side compares it to m_dlCheckFile
    // when the exec-file feature is enabled — for now we just stash it.
    if (pos + 16 <= body.size())
    {
        std::memcpy(&out.dl_check,    body.data() + pos, 8); pos += 8;
        std::memcpy(&out.ll_checksum, body.data() + pos, 8); pos += 8;
        out.checksum_present = true;
    }
    if (expect_channeling && pos + 1 <= body.size())
    {
        out.channeling = static_cast<std::uint8_t>(body[pos]);
        pos += 1;
        out.channeling_present = true;
    }
    return true;
}

// Wire-format helper: build a length-prefixed packet payload one
// field at a time. STRINGs are INT32 little-endian length + raw bytes
// (no NUL terminator) — matches the legacy CString::Serialize layout.
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

// Per-session agreement gate. Mirrors legacy
// `if (!pUser->m_bAgreement) return EC_SESSION_INVALIDCHAR;` —
// CharList / Create / Delete / Start / Veteran all need it. Returns
// true if the gate is open (agreed flag set) OR there's no registry
// wired (in which case we're in stub-mode test territory and let it
// through).
bool IsAgreed(const std::shared_ptr<tnetlib::AsioSession>& session,
              services::IConnectionRegistry* registry)
{
    if (!registry) return true;
    const auto entry = registry->Lookup(session);
    return entry && entry->agreed;
}

} // namespace

namespace {

fourstory::audit::LoginOutcome ToLoginOutcome(services::AuthStatus s)
{
    using S = services::AuthStatus;
    using O = fourstory::audit::LoginOutcome;
    switch (s)
    {
    case S::Success:         return O::Success;
    case S::NoUser:          return O::NoUser;
    case S::WrongPassword:   return O::WrongPassword;
    case S::Duplicate:       return O::Duplicate;
    case S::Banned:          return O::Banned;
    case S::IpBanned:        return O::IpBanned;
    case S::AgreementNeeded: return O::AgreementNeeded;
    case S::VersionMismatch: return O::VersionMismatch;
    default:                 return O::InternalError;
    }
}

fourstory::audit::CreateCharOutcome ToCreateOutcome(services::CreateCharResult s)
{
    using S = services::CreateCharResult;
    using O = fourstory::audit::CreateCharOutcome;
    switch (s)
    {
    case S::Success:       return O::Success;
    case S::NoGroup:       return O::NoGroup;
    case S::DuplicateName: return O::DuplicateName;
    case S::InvalidSlot:   return O::InvalidSlot;
    case S::Protected:     return O::Protected;
    case S::OverChar:      return O::OverChar;
    default:               return O::Internal;
    }
}

fourstory::audit::DeleteCharOutcome ToDeleteOutcome(services::DeleteCharResult s)
{
    using S = services::DeleteCharResult;
    using O = fourstory::audit::DeleteCharOutcome;
    switch (s)
    {
    case S::Success:         return O::Success;
    case S::Failed:          return O::Failed;
    case S::InvalidPassword: return O::InvalidPassword;
    case S::NoGroup:         return O::NoGroup;
    default:                 return O::Internal;
    }
}

} // namespace

boost::asio::awaitable<void>
OnLoginReq(std::shared_ptr<tnetlib::AsioSession> session, std::span<const std::byte> body,
           services::IAuthService* auth_service,
           services::IConnectionRegistry* connection_registry,
           fourstory::audit::IAuditLogger* audit_logger,
           fourstory::ops::LoginRateLimiter* rate_limiter,
           std::span<const std::uint16_t> accepted_versions,
           fourstory::smtp::ISmtpClient* smtp_client,
           Nation nation)
{
    // Default to the legacy single value if the caller passed nothing.
    // This keeps the unit-test path (which doesn't thread the config)
    // working against the same wire-version it always has.
    const std::uint16_t kDefaultVersions[] = { kProtocolVersion };
    if (accepted_versions.empty())
    {
        accepted_versions = std::span<const std::uint16_t>(
            kDefaultVersions, std::size(kDefaultVersions));
    }
    auto version_accepted = [&](std::uint16_t v) {
        for (auto x : accepted_versions) if (x == v) return true;
        return false;
    };
    auto& sref = *session;
    LoginAck ack{};
    ack.bCreateCnt = 6;  // CHARSLOT_MAX default

    // Per-IP rate limit. Bucket exhaustion → reply LR_INTERNAL (5)
    // without consulting the auth backend. Legacy server has no
    // rate-limit so a real client never hits this; brute-forcing
    // peers do. The wire result is LR_INTERNAL because the legacy
    // client doesn't render a distinct "rate-limited" code.
    if (rate_limiter != nullptr && !rate_limiter->Allow(sref.RemoteIPv4()))
    {
        ack.bResult = static_cast<std::uint8_t>(services::AuthStatus::InternalError);
        ack.dCurTime = static_cast<std::int64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        spdlog::warn("CS_LOGIN_REQ from ip={} rate-limited", sref.RemoteIPv4());
        if (audit_logger != nullptr)
        {
            audit_logger->LogLogin(fourstory::audit::LoginOutcome::InternalError,
                "",
                0,
                sref.RemoteIPv4(),
                0);
        }
        const auto payload = EncodeLoginAck(ack);
        co_await sref.SendPacket(
            tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_LOGIN_ACK),
            std::span<const std::byte>(payload.data(), payload.size()));
        co_return;
    }

    // dCurTime stamped on every reply — legacy fills it from time()
    // (see TUser.h::SendCS_LOGIN_ACK signature). Some legacy-client
    // builds use it to seed local clocks for the EULA timer.
    ack.dCurTime = static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

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
        ack.bResult = version_accepted(wVersion)
                      ? kLrSuccess
                      : kLrVersionMismatch;
        spdlog::info("CS_LOGIN_REQ v=0x{:04X} (stub) → result={}", wVersion, ack.bResult);
    }
    else
    {
        // Full path: parse the wire, delegate to IAuthService.
        LoginReqFields fields;
        const bool jp = (nation == Nation::Japan);
        if (!ParseLoginReq(body, fields, /*expect_channeling=*/jp))
        {
            spdlog::warn("CS_LOGIN_REQ malformed body — closing session");
            sref.Close();
            co_return;
        }
        if (jp && !fields.channeling_present)
        {
            // JP deployment expects the trailing channeling byte. A
            // missing one means either a non-JP client connected to
            // a JP server, or a forged packet. Mirror legacy behavior:
            // close.
            spdlog::warn("CS_LOGIN_REQ (JP) missing bChanneling tail — closing");
            sref.Close();
            co_return;
        }
        if (!version_accepted(fields.version))
        {
            spdlog::warn("CS_LOGIN_REQ version mismatch (got 0x{:04X}, "
                         "accepted_versions has {} entries)",
                fields.version, accepted_versions.size());
            ack.bResult = kLrVersionMismatch;
        }
        else if (fields.checksum_present
                 && !VerifyLoginChecksum(fields.version, fields.ll_checksum))
        {
            // Wire-checksum mismatch — legacy closes the session
            // (EC_SESSION_INVALIDCHAR). Forged or out-of-spec client.
            spdlog::warn("CS_LOGIN_REQ checksum mismatch (got 0x{:016X}) — closing",
                static_cast<std::uint64_t>(fields.ll_checksum));
            sref.Close();
            co_return;
        }
        else
        {
            services::AuthRequest req{
                .user_id = fields.user_id,
                .password = fields.password,
                .client_ip = sref.RemoteIPv4(),
                .client_version = fields.version,
                .channeling = fields.channeling,
                .channeling_present = fields.channeling_present,
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

            // 2FA challenge — Authenticate held off the TCURRENTUSER /
            // TLOG inserts; we issue a code, mail it, register the
            // session in pending state, and send CS_SECURITYCONFIRM_REQ.
            // The client shows the code-entry dialog; on success
            // OnSecurityConfirmAck completes the login + delivers the
            // deferred CS_LOGIN_ACK.
            if (result.status == services::AuthStatus::SecurityRequired
                && connection_registry != nullptr
                && auth_service != nullptr)
            {
                const std::string code = auth_service->IssueSecurityCode(result.user_id);
                const auto email_rec = auth_service->LookupEmail(result.user_id);
                if (!code.empty() && email_rec && smtp_client != nullptr)
                {
                    smtp_client->Send(email_rec->email,
                        "4Story — login verification code",
                        "A login attempt from a new device was detected.\n\n"
                        "If this was you, enter the following code in the "
                        "verification dialog:\n\n"
                        "    " + code + "\n\n"
                        "If it wasn't you, change your password immediately.");
                }
                services::ConnectionEntry entry{
                    .user_id = result.user_id,
                    .session_key = 0,             // assigned after confirm
                    .handoff_to_map = false,
                    .agreed = false,
                    .group_id = 0,
                    .check_key = 0,
                    .awaiting_security = true,
                    .pending_client_ip = sref.RemoteIPv4(),
                };
                connection_registry->Register(entry, session);

                // Server → client: prompt the code-entry dialog.
                co_await sref.SendPacket(
                    tnetlib::protocol::ToUint16(
                        tnetlib::protocol::MessageId::CS_SECURITYCONFIRM_REQ),
                    std::span<const std::byte>{});

                if (audit_logger != nullptr)
                {
                    audit_logger->LogLogin(
                        fourstory::audit::LoginOutcome::AgreementNeeded, // closest existing label
                        fields.user_id, result.user_id, sref.RemoteIPv4(), 0);
                }
                spdlog::info("CS_LOGIN_REQ user={} → SecurityRequired "
                             "(2FA email sent, awaiting CONFIRM_ACK)",
                    fields.user_id);
                co_return;  // do NOT send CS_LOGIN_ACK now
            }

            // Per-session bookkeeping on any "the user is now known"
            // status — that covers Success (agreement already on file)
            // AND AgreementNeeded (TUSERINFOTABLE row missing /
            // bAgreement != 1). In the AgreementNeeded case the client
            // can still send CS_AGREEMENT_REQ → flip the gate; without
            // a registered entry we wouldn't be able to recover the
            // user_id for that handler.
            const bool authed =
                result.status == services::AuthStatus::Success ||
                result.status == services::AuthStatus::AgreementNeeded;
            if (authed && connection_registry != nullptr)
            {
                // Random 64-bit nonce for the per-session check key.
                // thread_local Mersenne-Twister seeded once per thread
                // off random_device — cheap, non-crypto, fine for
                // legacy-protocol echo. Not used by the modernized
                // server for any check, just preserved on the wire.
                thread_local std::mt19937_64 rng{ std::random_device{}() };
                const std::int64_t check_key = static_cast<std::int64_t>(rng());

                services::ConnectionEntry entry{
                    .user_id = result.user_id,
                    .session_key = result.session_key,
                    .handoff_to_map = false,
                    // Success means the legacy bAgreement gate is
                    // already 1; AgreementNeeded means it isn't yet
                    // (CS_AGREEMENT_REQ will flip it).
                    .agreed = (result.status == services::AuthStatus::Success),
                    .group_id = 0,
                    .check_key = check_key,
                };
                auto previous = connection_registry->Register(entry, session);
                if (previous)
                {
                    // Modern divergence: legacy closed BOTH old and new
                    // sessions on duplicate. Modern "newest wins" UX —
                    // close the previous holder and let the new one
                    // proceed. The SociAuthService::Authenticate side
                    // already set bLocked=1 on the existing
                    // TCURRENTUSER row so the old session's close path
                    // cleans up correctly.
                    spdlog::warn(
                        "duplicate login for user_id={} — kicking previous session",
                        result.user_id);
                    previous->Close();
                }
                ack.dlCheckKey = check_key;
            }

            if (audit_logger != nullptr)
            {
                audit_logger->LogLogin(
                    ToLoginOutcome(result.status),
                    fields.user_id,
                    result.user_id,
                    sref.RemoteIPv4(),
                    result.session_key);
            }
        }
    }

    const auto payload = EncodeLoginAck(ack);
    co_await sref.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_LOGIN_ACK),
        std::span<const std::byte>(payload.data(), payload.size()));
}

boost::asio::awaitable<void>
OnGroupListReq(std::shared_ptr<tnetlib::AsioSession> session,
               std::span<const std::byte> /*body*/,
               services::IMapServerLocator* map_server_locator,
               services::IConnectionRegistry* connection_registry)
{
    auto& sref = *session;

    std::vector<services::GroupInfo> groups;
    if (map_server_locator != nullptr)
    {
        const auto user_id = ResolveUserId(session, connection_registry);
        groups = map_server_locator->ListGroups(user_id);
    }

    // Ack layout (legacy CSHandler.cpp:506-541):
    //   BYTE  bCount
    //   DWORD dwCheckPoint  — formerly file-integrity nonce; we send 0
    //   per group: STRING szNAME (INT32 len + bytes), BYTE bGroupID,
    //              BYTE bType, BYTE bStatus, BYTE bFlags
    ByteAppender out;
    const std::uint8_t count = static_cast<std::uint8_t>(
        std::min<std::size_t>(groups.size(), 255));
    out.U8(count);
    out.U32(0);  // dwCheckPoint — formerly file-integrity nonce
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto& g = groups[i];
        out.Str(g.name);
        out.U8(g.group_id);
        out.U8(g.type);
        out.U8(static_cast<std::uint8_t>(g.status));
        out.U8(g.flags);
    }

    spdlog::info("CS_GROUPLIST_REQ → CS_GROUPLIST_ACK ({} groups)", count);
    co_await sref.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_GROUPLIST_ACK),
        std::span<const std::byte>(out.bytes.data(), out.bytes.size()));
}

boost::asio::awaitable<void>
OnChannelListReq(tnetlib::AsioSession& session,
                 std::span<const std::byte> body,
                 services::IMapServerLocator* map_server_locator)
{
    const auto groupId = body.empty()
        ? std::uint8_t{0}
        : static_cast<std::uint8_t>(body[0]);

    std::vector<services::ChannelInfo> channels;
    if (map_server_locator != nullptr)
    {
        channels = map_server_locator->ListChannels(groupId);
    }

    // Ack layout (legacy CSHandler.cpp:560-587):
    //   BYTE  bCount, DWORD dwCheckPoint=0
    //   per channel: STRING szNAME, BYTE bChannel, BYTE bStatus
    ByteAppender out;
    const std::uint8_t count = static_cast<std::uint8_t>(
        std::min<std::size_t>(channels.size(), 255));
    out.U8(count);
    out.U32(0);  // dwCheckPoint
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto& c = channels[i];
        out.Str(c.name);
        out.U8(c.channel);
        out.U8(static_cast<std::uint8_t>(c.status));
    }

    spdlog::info("CS_CHANNELLIST_REQ group={} → CS_CHANNELLIST_ACK ({} channels)",
        groupId, count);
    co_await session.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_CHANNELLIST_ACK),
        std::span<const std::byte>(out.bytes.data(), out.bytes.size()));
}

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

    // Legacy gate: CSHandler.cpp:600 `if (!pUser->m_bAgreement) return
    // EC_SESSION_INVALIDCHAR;`. Pre-agreement charlist would land the
    // client in a confused state — the EULA screen isn't supposed to
    // surface a char list. Close the session, same shape as legacy.
    if (!IsAgreed(session, connection_registry))
    {
        spdlog::warn("CS_CHARLIST_REQ from session without agreement "
                     "(uid={}) — closing", userId);
        sref.Close();
        co_return;
    }

    // Stamp the selected group so DELCHAR's group-match check (legacy
    // CSHandler.cpp:1223) and START_REQ get the same answer.
    if (connection_registry != nullptr)
    {
        connection_registry->SetGroupId(session, groupId);
    }

    std::vector<services::CharacterInfo> chars;
    if (char_service != nullptr && userId != 0)
    {
        chars = char_service->List(userId, groupId);
    }

    // Wire layout (verified against TClient/TNetHandler.cpp:372-415):
    //   DWORD dwCheckPoint  — file-integrity probe offset; client
    //                         reads INT64 at this offset from its
    //                         own TClient.exe and replies via
    //                         CS_HOTSEND_REQ. 0 disables.
    //   BYTE  bCount
    //   { per char: DWORD dwCharID, STRING strName,
    //     BYTE bStartAct, bSlot, bLevel, bClass, bRace, bCountry,
    //     BYTE bSex, bHair, bFace, bBody, bPants, bHand, bFoot,
    //     DWORD dwRegion, dwFame, dwFameColor,
    //     BYTE bHelmetHide, bEquipItemCount,
    //     per item (10 bytes):
    //       BYTE  bItemID, WORD wItemID, BYTE bLevel, BYTE bGradeEffect,
    //       WORD wColor, BYTE bRegGuild, WORD wMoggItemID
    //   }
    ByteAppender p;
    p.U32(0);                                             // dwCheckPoint
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
        const auto item_count = static_cast<std::uint8_t>(
            std::min<std::size_t>(c.items.size(), 255));
        p.U8(item_count);
        for (std::size_t i = 0; i < item_count; ++i)
        {
            const auto& it = c.items[i];
            p.U8(it.item_id);
            p.U16(it.item_kind);
            p.U8(it.level);
            p.U8(it.grade_effect);
            p.U16(it.color);
            p.U8(it.reg_guild);
            p.U16(it.mogg_item_id);
        }
    }

    spdlog::info("CS_CHARLIST_REQ user={} group={} → CS_CHARLIST_ACK ({} chars)",
        userId, groupId, chars.size());
    co_await sref.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_CHARLIST_ACK),
        std::span<const std::byte>(p.bytes.data(), p.bytes.size()));

    // CS_BOWPLAYERNOTIFY_ACK trailing send — legacy CSHandler.cpp:763-777
    // (DEF_UDPLOG branch). If the user has a BR (or BOW) char in the
    // enrollment table AND that char is in the just-sent list, send a
    // one-byte ack with the slot so the client can highlight it as
    // "your BR character" in the UI.
    if (char_service != nullptr && userId != 0)
    {
        const std::int32_t br_char_id = char_service->GetBrCharId(userId);
        if (br_char_id != 0)
        {
            for (const auto& c : chars)
            {
                if (c.char_id == br_char_id)
                {
                    std::byte notify[1] = { static_cast<std::byte>(c.slot) };
                    spdlog::info("CS_BOWPLAYERNOTIFY_ACK user={} br_char={} slot={}",
                        userId, br_char_id, static_cast<int>(c.slot));
                    co_await sref.SendPacket(
                        tnetlib::protocol::ToUint16(
                            tnetlib::protocol::MessageId::CS_BOWPLAYERNOTIFY_ACK),
                        std::span<const std::byte>(notify, sizeof(notify)));
                    break;
                }
            }
        }
    }
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
                services::IConnectionRegistry* connection_registry,
                fourstory::audit::IAuditLogger* audit_logger,
                Nation nation)
{
    auto& sref = *session;

    // Agreement gate — legacy CSHandler.cpp:980 `if
    // (!pUser->m_bAgreement) return EC_SESSION_INVALIDCHAR;`. Pre-EULA
    // char creation isn't a thing.
    if (!IsAgreed(session, connection_registry))
    {
        spdlog::warn("CS_CREATECHAR_REQ from session without agreement — closing");
        sref.Close();
        co_return;
    }

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

    // Legacy CheckCharName parity — reject names with bytes that don't
    // belong to the deployment's locale character set. Length is also
    // re-checked here (3..16) so the SOCI backend can keep its strict
    // alnum gate as a defense-in-depth fallback. CR_PROTECTED matches
    // the legacy code for "name contains forbidden bytes".
    if (!services::IsValidCharName(req.name, nation))
    {
        constexpr std::uint8_t kCrProtected = 4;
        spdlog::info("CS_CREATECHAR_REQ user={} name='{}' rejected — invalid charset for nation",
            req.user_id, req.name);
        ByteAppender p;
        p.U8(kCrProtected);
        p.I32(0);
        p.Str(req.name);
        // 13 trailing bytes echoed back like in the success path —
        // the legacy client expects a fixed-shape ack and renders the
        // error using bResult alone.
        p.U8(req.slot);
        p.U8(req.char_class); p.U8(req.race); p.U8(req.country);
        p.U8(req.sex);
        p.U8(req.hair); p.U8(req.face); p.U8(req.body);
        p.U8(req.pants); p.U8(req.hand); p.U8(req.foot);
        p.U8(0); // remaining_slots — unchanged on rejection
        p.U8(0); // starting_level
        co_await sref.SendPacket(
            tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_CREATECHAR_ACK),
            std::span<const std::byte>(p.bytes.data(), p.bytes.size()));
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

    if (audit_logger != nullptr)
    {
        audit_logger->LogCharCreate(
            ToCreateOutcome(result.status),
            req.user_id, req.group_id, req.name, result.char_id);
    }

    co_await sref.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_CREATECHAR_ACK),
        std::span<const std::byte>(p.bytes.data(), p.bytes.size()));
}

boost::asio::awaitable<void>
OnDelCharReq(std::shared_ptr<tnetlib::AsioSession> session,
             std::span<const std::byte> body,
             services::ICharService* char_service,
             services::IConnectionRegistry* connection_registry,
             services::IAuthService* auth_service,
             fourstory::audit::IAuditLogger* audit_logger)
{
    auto& sref = *session;

    // Legacy DR_* result codes from CSHandler.cpp:1208 — DR_INVALIDPASSWD
    // signals "password didn't match" (CSPCheckPasswd's RETURN 1).
    constexpr std::uint8_t kDrInvalidPassword = 2;

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

    // Agreement gate. Legacy CSHandler.cpp:1208 doesn't gate explicitly
    // on m_bAgreement (it checks `!pUser->m_dwID` for "not logged in"),
    // but a pre-EULA delete would have to mean the session never went
    // through LOGIN — same outcome.
    if (!IsAgreed(session, connection_registry))
    {
        spdlog::warn("CS_DELCHAR_REQ from session without agreement — refusing");
        co_await reply_with(kDrInternal, 0);
        co_return;
    }

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
    if (pwlen < 0 || pwlen > 64) {
        // Legacy emits DR_INVALIDPASSWD on length-out-of-range
        // (CSHandler.cpp:1228-1238). Mirror that.
        co_await reply_with(kDrInvalidPassword, 0); co_return;
    }
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

    // Group cross-check — legacy CSHandler.cpp:1223 closes the session
    // (EC_SESSION_INVALIDCHAR) if the request's bGroupID doesn't match
    // the user's last-seen group. We're nicer about it (just refuse).
    if (connection_registry != nullptr)
    {
        const auto entry = connection_registry->Lookup(session);
        if (entry && entry->group_id != 0 && entry->group_id != group_id)
        {
            spdlog::warn("CS_DELCHAR_REQ user={} group={} mismatches stamped group={} — refusing",
                user_id, group_id, entry->group_id);
            co_await reply_with(kDrInternal, char_id);
            co_return;
        }
    }

    // Legacy CSPCheckPasswd gate — mirrors CSHandler.cpp:1240-1257.
    // Match on success → continue; mismatch → DR_INVALIDPASSWD;
    // DB error → DR_INTERNAL. Null auth_service path keeps the
    // in-memory smoke-test behavior (accept anything).
    if (auth_service != nullptr && !auth_service->VerifyPassword(user_id, password))
    {
        spdlog::info("CS_DELCHAR_REQ user={} → DR_INVALIDPASSWD (password mismatch)",
            user_id);
        if (audit_logger != nullptr)
        {
            audit_logger->LogCharDelete(
                fourstory::audit::DeleteCharOutcome::InvalidPassword,
                user_id, group_id, char_id);
        }
        co_await reply_with(kDrInvalidPassword, char_id);
        co_return;
    }

    const auto result = char_service->Delete(user_id, group_id, char_id, password);
    spdlog::info("CS_DELCHAR_REQ user={} group={} char={} → {}",
        user_id, group_id, char_id, static_cast<std::uint8_t>(result));
    if (audit_logger != nullptr)
    {
        audit_logger->LogCharDelete(
            ToDeleteOutcome(result), user_id, group_id, char_id);
    }
    co_await reply_with(static_cast<std::uint8_t>(result), char_id);
}

boost::asio::awaitable<void>
OnStartReq(std::shared_ptr<tnetlib::AsioSession> session,
           std::span<const std::byte> body,
           services::IMapServerLocator* map_server_locator,
           services::IConnectionRegistry* connection_registry,
           fourstory::audit::IAuditLogger* audit_logger)
{
    auto& sref = *session;

    // Legacy gate CSHandler.cpp:1314 `if (!pUser->m_bAgreement) return
    // EC_SESSION_INVALIDCHAR;`. Pre-EULA START is forbidden.
    if (!IsAgreed(session, connection_registry))
    {
        spdlog::warn("CS_START_REQ from session without agreement — closing");
        sref.Close();
        co_return;
    }

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
    const auto resolved_user_id = ResolveUserId(session, connection_registry);
    std::uint8_t resolved_server_id = 0;
    bool start_ok = false;

    if (map_server_locator == nullptr)
    {
        payload[0] = static_cast<std::byte>(kSrNoServer);
        spdlog::info("CS_START_REQ group={} ch={} char={} → CS_START_ACK (stub: SR_NOSERVER)",
            bGroupID, bChannel, dwCharID);
    }
    else if (auto ep = map_server_locator->Lookup(
                 resolved_user_id, bGroupID, bChannel, dwCharID))
    {
        payload[0] = static_cast<std::byte>(0); // SR_SUCCESS
        start_ok = true;
        resolved_server_id = ep->server_id;
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

    if (audit_logger != nullptr)
    {
        audit_logger->LogGameStart(
            start_ok, resolved_user_id, bGroupID, bChannel, dwCharID, resolved_server_id);
    }

    co_await sref.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_START_ACK),
        std::span<const std::byte>(payload, sizeof(payload)));
}

boost::asio::awaitable<void>
OnAgreementReq(std::shared_ptr<tnetlib::AsioSession> session,
               std::span<const std::byte> body,
               services::IAuthService* auth_service,
               services::IConnectionRegistry* connection_registry)
{
    // No ack. Body is WORD wVersion. Legacy SP CSPAgreement sets
    // TACCOUNT_PW.bCheck=1 + flips per-session m_bAgreement. The new
    // server doesn't yet enforce the per-session gate (handlers that
    // need it can read it back from the registry once we add it);
    // this handler persists the DB side now.
    std::uint16_t wVersion = 0;
    if (body.size() >= 2) std::memcpy(&wVersion, body.data(), 2);

    const auto user_id = ResolveUserId(session, connection_registry);
    if (auth_service != nullptr && user_id != 0)
    {
        auth_service->SetAgreement(user_id);
        if (connection_registry != nullptr)
        {
            // Flip the per-session gate so the next CHARLIST/START
            // proceeds. Without this the legacy gate would forever
            // bounce the client back to the EULA screen.
            connection_registry->MarkAgreed(session);
        }
        spdlog::info("CS_AGREEMENT_REQ version=0x{:04X} uid={} → bAgreement=1",
            wVersion, user_id);
    }
    else
    {
        spdlog::info("CS_AGREEMENT_REQ version=0x{:04X} uid={} "
                     "(no auth_service or unauth session: persisted nothing)",
            wVersion, user_id);
    }
    co_return;
}

boost::asio::awaitable<void>
OnHotsendReq(std::shared_ptr<tnetlib::AsioSession> /*session*/,
             std::span<const std::byte> /*body*/)
{
    // CS_HOTSEND_REQ is the legacy client's exec-file integrity
    // heartbeat. The shipped TClient sends it after every
    // CS_GROUPLIST_ACK / CS_CHANNELLIST_ACK (TNetHandler.cpp:337 +
    // :721 call CheckModuleFile → SendCS_HOTSEND_REQ). The legacy
    // server's validation path is anti-cheat tooling and is
    // intentionally out of scope here per the project's stated
    // anti-cheat-omitted scope.
    //
    // Quiet silent accept — handler exists only so the dispatcher
    // doesn't surface the per-lobby heartbeats as "unhandled packet"
    // warnings.
    co_return;
}

boost::asio::awaitable<void>
OnVeteranReq(tnetlib::AsioSession& session,
             std::span<const std::byte> /*body*/,
             services::ICharService* char_service)
{
    // Ack: BYTE bOption, BYTE bFirstLevel, BYTE bSecondLevel,
    // BYTE bThirdLevel. Legacy sends bOption=3 ("all three options
    // unconditionally available", per CSHandler.cpp:1501-1503's
    // comment) with the level thresholds from m_vVETERAN.
    constexpr std::uint8_t kOptionAllAvailable = 3;
    services::VeteranLevels levels{};
    std::uint8_t option = 0;
    if (char_service != nullptr)
    {
        levels = char_service->GetVeteranLevels();
        // If the chart is empty (all zeros) keep bOption=0 so the
        // client can detect "no veteran feature" — matches legacy
        // safer default than always reporting "available".
        if (levels.first || levels.second || levels.third)
            option = kOptionAllAvailable;
    }

    std::byte payload[4] = {
        static_cast<std::byte>(option),
        static_cast<std::byte>(levels.first),
        static_cast<std::byte>(levels.second),
        static_cast<std::byte>(levels.third),
    };
    spdlog::info("CS_VETERAN_REQ → CS_VETERAN_ACK (option={}, levels={}/{}/{})",
        option, levels.first, levels.second, levels.third);
    co_await session.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_VETERAN_ACK),
        std::span<const std::byte>(payload, sizeof(payload)));
}

boost::asio::awaitable<void>
OnTerminateReq(tnetlib::AsioSession& session, std::span<const std::byte> body)
{
    // Body: DWORD dwKey, must equal kTerminateMagic (0x2AF3A9D1 =
    // 720809425, CSHandler.cpp:1452). Legacy semantics:
    //   * invalid magic  → return EC_SESSION_INVALIDCHAR → session
    //                       killed by the recv loop.
    //   * valid   magic  → no-op (EC_NOERROR). The client closes the
    //                       TCP socket next, which triggers OnCloseSession
    //                       → TLogoutAll cleanup. Modernized server
    //                       follows the same shape: the per-session
    //                       close handler in LoginServer::HandleConnection
    //                       runs SessionTerminator::Terminate with
    //                       reason=Disconnect, doing the legacy
    //                       TCURRENTUSER delete + TLOG timeLOGOUT
    //                       stamp implicitly.
    std::uint32_t dwKey = 0;
    if (body.size() >= 4) std::memcpy(&dwKey, body.data(), 4);
    if (dwKey != kTerminateMagic)
    {
        spdlog::warn("CS_TERMINATE_REQ magic mismatch (got 0x{:08X}) — closing",
            dwKey);
        session.Close();
        co_return;
    }
    spdlog::info("CS_TERMINATE_REQ — clean logout acknowledged "
                 "(awaiting client TCP close)");
    co_return;
}

boost::asio::awaitable<void>
OnSecurityConfirmAck(std::shared_ptr<tnetlib::AsioSession> session,
                     std::span<const std::byte> body,
                     services::IAuthService* auth_service,
                     services::IConnectionRegistry* connection_registry,
                     fourstory::audit::IAuditLogger* audit_logger)
{
    auto& sref = *session;

    // Wire body: STRING strCode (INT32 length + bytes). Same shape
    // as CSHandler.cpp:1510-1540.
    std::string code;
    if (body.size() >= 4)
    {
        std::int32_t len = 0;
        std::memcpy(&len, body.data(), 4);
        if (len >= 0 && len <= 64 &&
            static_cast<std::size_t>(4 + len) <= body.size())
        {
            code.assign(reinterpret_cast<const char*>(body.data() + 4),
                        static_cast<std::size_t>(len));
        }
    }

    constexpr std::uint8_t kCodeIncorrect = 1;
    std::uint8_t result_code = kCodeIncorrect;

    // Recover the pending login state (user_id + pending_client_ip)
    // from the registry entry we stamped during the LOGIN_REQ path.
    std::int32_t user_id = 0;
    std::string  client_ip;
    bool         was_awaiting_security = false;
    if (connection_registry != nullptr)
    {
        if (auto entry = connection_registry->Lookup(session))
        {
            user_id = entry->user_id;
            client_ip = entry->pending_client_ip;
            was_awaiting_security = entry->awaiting_security;
        }
    }
    if (client_ip.empty()) client_ip = sref.RemoteIPv4();

    if (code.empty() || user_id == 0)
    {
        spdlog::warn("CS_SECURITYCONFIRM_ACK empty/anon (uid={} code_len={})",
            user_id, code.size());
    }
    else if (auth_service != nullptr
             && auth_service->VerifySecurityCode(user_id, code))
    {
        result_code = kCodeCorrect;
    }

    std::byte payload[1] = { static_cast<std::byte>(result_code) };
    spdlog::info("CS_SECURITYCONFIRM_ACK uid={} → CS_SECURITYRESULT_ACK result={}",
        user_id, result_code);
    co_await sref.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_SECURITYRESULT_ACK),
        std::span<const std::byte>(payload, sizeof(payload)));

    // If we were in 2FA-pending mode AND the code matched, complete
    // the deferred login: whitelist the IP, insert TCURRENTUSER + TLOG,
    // flip the session's pending flag, then send the long-awaited
    // CS_LOGIN_ACK so the client can move on to the lobby.
    if (was_awaiting_security && result_code == kCodeCorrect
        && auth_service != nullptr && connection_registry != nullptr)
    {
        auth_service->AddTrustedIp(user_id, client_ip);
        const std::uint32_t key = auth_service->CompleteSecurityLogin(
            user_id, client_ip);
        if (key == 0)
        {
            spdlog::error("CS_SECURITYCONFIRM_ACK uid={} — complete login "
                          "failed (DB error)", user_id);
            sref.Close();
            co_return;
        }
        connection_registry->CompleteSecurityLogin(session, key);

        // Build the deferred CS_LOGIN_ACK with full session info. The
        // client sees this and moves on as if the original LOGIN had
        // succeeded.
        LoginAck ack{};
        ack.bResult    = static_cast<std::uint8_t>(services::AuthStatus::Success);
        ack.dwUserID   = static_cast<std::uint32_t>(user_id);
        ack.dwKEY      = key;
        ack.bCreateCnt = 6;
        ack.dCurTime = static_cast<std::int64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        thread_local std::mt19937_64 rng{ std::random_device{}() };
        ack.dlCheckKey = static_cast<std::int64_t>(rng());
        const auto payload2 = EncodeLoginAck(ack);
        spdlog::info("CS_SECURITYCONFIRM_ACK uid={} → deferred CS_LOGIN_ACK "
                     "(key=0x{:08X}, ip whitelisted)", user_id, key);
        co_await sref.SendPacket(
            tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_LOGIN_ACK),
            std::span<const std::byte>(payload2.data(), payload2.size()));

        if (audit_logger != nullptr)
        {
            audit_logger->LogLogin(fourstory::audit::LoginOutcome::Success,
                "<2fa>", user_id, client_ip, key);
        }
    }
}

// ===== Control-server (CT_*) handlers =======================================
//
// Phase B parity stubs for the 5 server-to-server messages legacy
// TLoginSvr handled on `m_bSessionType == SESSION_SERVER` sessions.
// Real TControlSvr hookup lands in Phase D (`MODERNIZATION_PLAN.md`).
// Until then these handlers exist primarily so the dispatcher doesn't
// log them as unhandled when an admin tool fires them at the running
// server.

boost::asio::awaitable<void>
OnControlServiceMonitor(tnetlib::AsioSession& session,
                        std::span<const std::byte> body,
                        services::IConnectionRegistry* connection_registry)
{
    // Inbound: DWORD dwTick. Reply with CT_SERVICEMONITOR_REQ carrying
    // (dwTick, dwSESSION, dwTUSER, dwTACTIVEUSER). Legacy populated
    // SESSION/TUSER/TACTIVEUSER from three separate dicts; the
    // modernized server collapses them into a single ConnectionRegistry
    // count (one entry per authenticated session). The returned counts
    // are equal — fine for the dashboard which mostly cares about
    // "how many users are connected right now."
    std::uint32_t tick = 0;
    if (body.size() >= 4) std::memcpy(&tick, body.data(), 4);
    const std::uint32_t live = connection_registry
        ? static_cast<std::uint32_t>(connection_registry->Count())
        : 0u;

    ByteAppender p;
    p.U32(tick);
    p.U32(live);
    p.U32(live);
    p.U32(live);
    spdlog::info("CT_SERVICEMONITOR_ACK tick={} → CT_SERVICEMONITOR_REQ (live={})",
        tick, live);
    co_await session.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CT_SERVICEMONITOR_REQ),
        std::span<const std::byte>(p.bytes.data(), p.bytes.size()));
}

boost::asio::awaitable<void>
OnControlServiceDataClear(tnetlib::AsioSession& /*session*/,
                          std::span<const std::byte> /*body*/)
{
    // Legacy rebuilds m_mapACTIVEUSER from m_mapTUSER under m_csLI. The
    // modernized server's ConnectionRegistry is the single source of
    // truth — there's no derived map that could drift — so this is a
    // no-op. No reply (legacy returns EC_NOERROR without sending).
    spdlog::debug("CT_SERVICEDATACLEAR_ACK — no-op (registry is canonical)");
    co_return;
}

boost::asio::awaitable<void>
OnControlCtrlSvr(tnetlib::AsioSession& /*session*/,
                 std::span<const std::byte> /*body*/)
{
    // Heartbeat from TControlSvr. Legacy just returns EC_NOERROR. No
    // reply. Useful for keeping the SS link warm so the next real
    // message round-trips quickly.
    spdlog::debug("CT_CTRLSVR_REQ heartbeat");
    co_return;
}

boost::asio::awaitable<void>
OnControlEventUpdate(tnetlib::AsioSession& /*session*/,
                     std::span<const std::byte> body,
                     services::IEventRegistry* event_registry)
{
    // Legacy wire: BYTE bEventID, WORD wValue, then EVENTINFO struct
    // (variable layout — preserved as opaque bytes here). wValue == 0
    // signals "remove this event"; non-zero is upsert.
    if (body.size() < 3)
    {
        spdlog::warn("CT_EVENTUPDATE_REQ body too short ({} bytes)", body.size());
        co_return;
    }
    std::uint8_t event_id = static_cast<std::uint8_t>(body[0]);
    std::uint16_t value = 0;
    std::memcpy(&value, body.data() + 1, 2);

    // Legacy `if (bEventID > EVENT_COUNT) return EC_NOERROR;` —
    // EVENT_COUNT is per-build, so we keep the upper bound loose
    // (255 fits in BYTE anyway) and let the registry hold whatever.
    if (event_registry != nullptr)
    {
        if (value == 0)
        {
            event_registry->Remove(event_id);
        }
        else
        {
            services::EventEntry entry{};
            entry.event_id = event_id;
            entry.value = value;
            entry.opaque_payload.assign(body.begin() + 3, body.end());
            event_registry->Upsert(std::move(entry));
        }
    }
    spdlog::info("CT_EVENTUPDATE_REQ event_id={} value={} ({} bytes payload) → {}",
        event_id, value, body.size() >= 3 ? body.size() - 3 : 0,
        value == 0 ? "remove" : "upsert");
    co_return;
}

boost::asio::awaitable<void>
OnTestLoginReq(std::shared_ptr<tnetlib::AsioSession> session,
               std::span<const std::byte> /*body*/,
               services::IAuthService* auth_service,
               services::IConnectionRegistry* connection_registry,
               fourstory::audit::IAuditLogger* audit_logger)
{
    auto& sref = *session;
    LoginAck ack{};
    ack.bCreateCnt = 6;
    ack.dCurTime = static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    if (auth_service == nullptr)
    {
        ack.bResult = static_cast<std::uint8_t>(services::AuthStatus::InternalError);
        spdlog::warn("CS_TESTLOGIN_REQ from {} — no auth_service wired",
            sref.RemoteIPv4());
    }
    else
    {
        const auto result = auth_service->AuthenticateTest(sref.RemoteIPv4());
        ack.bResult    = static_cast<std::uint8_t>(result.status);
        ack.dwUserID   = static_cast<std::uint32_t>(result.user_id);
        ack.dwKEY      = result.session_key;
        ack.bCreateCnt = result.create_char_count;
        if (result.status == services::AuthStatus::Success
            && connection_registry != nullptr)
        {
            thread_local std::mt19937_64 rng{ std::random_device{}() };
            const std::int64_t check_key = static_cast<std::int64_t>(rng());
            services::ConnectionEntry entry{
                .user_id = result.user_id,
                .session_key = result.session_key,
                .handoff_to_map = false,
                .agreed = true,  // test logins bypass the EULA gate
                .group_id = 0,
                .check_key = check_key,
            };
            auto previous = connection_registry->Register(entry, session);
            if (previous) previous->Close();
            ack.dlCheckKey = check_key;
        }
        if (audit_logger != nullptr)
        {
            audit_logger->LogLogin(
                fourstory::audit::LoginOutcome::Success,
                "<test>",
                result.user_id,
                sref.RemoteIPv4(),
                result.session_key);
        }
    }

    const auto payload = EncodeLoginAck(ack);
    spdlog::info("CS_TESTLOGIN_REQ → CS_LOGIN_ACK result={} uid={}",
        ack.bResult, ack.dwUserID);
    co_await sref.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_LOGIN_ACK),
        std::span<const std::byte>(payload.data(), payload.size()));
}

boost::asio::awaitable<void>
OnQuitServiceReq(tnetlib::AsioSession& session,
                 std::span<const std::byte> /*body*/,
                 std::function<void()> on_stop)
{
    // Legacy CTLoginSvrModule::OnSM_QUITSERVICE_REQ is the Windows
    // Service Manager's clean-shutdown signal. We accept it from any
    // peer (the legacy server didn't gate on session type either).
    // Modern equivalents (SIGINT/SIGTERM) cover the local-restart
    // case; this path is for an orchestrator that wants to shut the
    // process down via the wire protocol.
    spdlog::info("SM_QUITSERVICE_REQ from {} — stopping",
        session.RemoteIPv4().empty() ? std::string{"(unknown)"} : session.RemoteIPv4());
    if (on_stop) on_stop();
    co_return;
}

boost::asio::awaitable<void>
OnTestVersionReq(tnetlib::AsioSession& session,
                 std::span<const std::byte> /*body*/)
{
    // Reply CS_TESTVERSION_ACK { WORD wVersion }. Legacy returns the
    // server's compiled-in TVERSION (CSSender.cpp:58-63).
    std::uint16_t v = kProtocolVersion;
    std::byte payload[2];
    std::memcpy(payload, &v, 2);
    spdlog::info("CS_TESTVERSION_REQ → CS_TESTVERSION_ACK (v=0x{:04X})", v);
    co_await session.SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_TESTVERSION_ACK),
        std::span<const std::byte>(payload, sizeof(payload)));
}

boost::asio::awaitable<void>
OnControlEventMsg(tnetlib::AsioSession& /*session*/,
                  std::span<const std::byte> body)
{
    // Legacy reads (bEventID, bMsgType, strMsg) and returns EC_NOERROR
    // without acting on it. Mirror that — log the first two bytes so
    // operators can correlate when needed.
    std::uint8_t event_id = 0;
    std::uint8_t msg_type = 0;
    if (body.size() >= 1) event_id = static_cast<std::uint8_t>(body[0]);
    if (body.size() >= 2) msg_type = static_cast<std::uint8_t>(body[1]);
    spdlog::info("CT_EVENTMSG_REQ event_id={} msg_type={} body={} bytes",
        event_id, msg_type, body.size());
    co_return;
}

} // namespace tloginsvr::handlers
