// Session-lifecycle handlers — CS_CONNECT_REQ and CS_CONREADY_REQ.
//
//   CS_CONNECT_REQ   — 29-byte handshake from the legacy client. The
//                      server validates the TCURRENTUSER row written
//                      by TLoginSvrAsio, binds the session into both
//                      registries, sends CS_CONNECT_ACK back, and on
//                      success fires MW_ADDCHAR_ACK to the World peer
//                      to register the char for inter-map routing.
//   CS_CONREADY_REQ  — empty-body signal from the client that local
//                      scene load finished. F7 keeps this as a stub
//                      log; the enter-map flood (CHARINFO_ACK + AOI
//                      players + monsters) is gameplay-policy work
//                      that lands with the F13 spawn / AI phase.
//
// Legacy parity: CSHandler.cpp:249 (OnCS_CONNECT_REQ),
// CSHandler.cpp:402 (OnCS_CONREADY_REQ).

#include "handlers.h"

#include "audit/audit_log.h"
#include "audit/event.h"
#include "services/channel_presence.h"
#include "services/char_state_store.h"
#include "services/client_senders.h"
#include "services/session_registry.h"
#include "services/session_validator.h"
#include "services/world_client.h"
#include "services/world_senders.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace tmapsvr {

namespace {

// Numeric result codes the legacy CS_CONNECT_ACK sends back. Names
// match the CN_* constants the legacy CSHandler.cpp uses; the values
// are operator-controlled until the legacy header turns up — keeping
// 0 = OK so a default-zero byte means success matches the convention
// used elsewhere on the wire.
enum class ConnectResult : std::uint8_t
{
    Ok          = 0,
    InvalidVer  = 1,
    InvalidChar = 2,
    Internal    = 3,
    NoChannel   = 4,
    Duplicate   = 5,
};

const char* ConnectResultName(ConnectResult r)
{
    switch (r) {
        case ConnectResult::Ok:          return "OK";
        case ConnectResult::InvalidVer:  return "INVALID_VER";
        case ConnectResult::InvalidChar: return "INVALID_CHAR";
        case ConnectResult::Internal:    return "INTERNAL";
        case ConnectResult::NoChannel:   return "NO_CHANNEL";
        case ConnectResult::Duplicate:   return "DUPLICATE";
    }
    return "?";
}

// CS_CONNECT_ACK body — single result byte followed by an empty
// vServerID vector (length 0). Mirrors CTPlayer::SendCS_CONNECT_ACK
// in CSSender.cpp:78. The vServerID list is populated by the World
// peer in later phases; F4/F5 always send an empty list.
std::vector<std::byte> EncodeConnectAck(ConnectResult r)
{
    std::vector<std::byte> body;
    body.reserve(2);
    wire::WritePOD<std::uint8_t>(body, static_cast<std::uint8_t>(r));
    wire::WritePOD<std::uint8_t>(body, 0); // vServerID.size()
    return body;
}

// MW_ADDCHAR_ACK encoding moved to services/world_senders.h so the
// map↔world body encoders live in one place (and stay unit-testable).

// "AM/PM HH:MM" server clock string CS_CHARINFO_ACK carries (legacy
// CSSender.cpp:344 formats the wall-clock the same way). Cosmetic — the
// client displays it; kept here so the pure encoder takes it as data.
std::string FormatServerClock()
{
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    const char* ampm = (tm.tm_hour < 12) ? "AM" : "PM";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%s %02d : %02d", ampm, tm.tm_hour,
        tm.tm_min);
    return std::string(buf);
}

} // namespace

boost::asio::awaitable<void>
OnConnectReq(std::shared_ptr<tnetlib::AsioSession> sess,
             std::vector<std::byte>                body,
             const HandlerContext&                 ctx)
{
    using tnetlib::protocol::MessageId;

    // Wire layout from legacy CSHandler.cpp::OnCS_CONNECT_REQ (decode
    // order matches CPacket::operator>>):
    //   WORD  wVersion
    //   BYTE  bChannel
    //   DWORD dwUserID
    //   DWORD dwID         (char id)
    //   DWORD dwKEY        (session token)
    //   DWORD dwIPAddr
    //   WORD  wPort
    //   INT64 llChecksum1  (validated against a derived value;
    //                       version + checksum checks land in a
    //                       follow-up commit — for F4 we trust the
    //                       framing and only verify the DB lookup)
    wire::Reader r(body.data(), body.size());

    std::uint16_t wVersion = 0;
    std::uint8_t  bChannel = 0;
    std::uint32_t dwUserID = 0;
    std::uint32_t dwID     = 0;
    std::uint32_t dwKEY    = 0;
    std::uint32_t dwIPAddr = 0;
    std::uint16_t wPort    = 0;
    std::int64_t  llChecksum1 = 0;

    if (!r.Read(wVersion) ||
        !r.Read(bChannel) ||
        !r.Read(dwUserID) ||
        !r.Read(dwID)     ||
        !r.Read(dwKEY)    ||
        !r.Read(dwIPAddr) ||
        !r.Read(wPort)    ||
        !r.Read(llChecksum1))
    {
        spdlog::warn("CS_CONNECT_REQ: short body ({} bytes) — dropping",
            body.size());
        co_await sess->SendPacket(
            static_cast<std::uint16_t>(MessageId::CS_CONNECT_ACK),
            EncodeConnectAck(ConnectResult::Internal));
        co_return;
    }

    // Run the validator. No DB pool → no validator → handshake fails
    // (the F3 boot log already warned the operator).
    if (!ctx.validator)
    {
        spdlog::warn("CS_CONNECT_REQ uid={} key={}: validator not configured "
                     "(no [database] in TOML) — refusing",
            dwUserID, dwKEY);
        co_await sess->SendPacket(
            static_cast<std::uint16_t>(MessageId::CS_CONNECT_ACK),
            EncodeConnectAck(ConnectResult::Internal));
        co_return;
    }

    const auto row = ctx.validator->LookupSession(dwUserID, dwKEY);
    ConnectResult result = ConnectResult::Ok;
    if (!row)
        result = ConnectResult::InvalidChar;
    else if (row->bLocked)
        result = ConnectResult::InvalidChar;
    else if (ctx.expected_group != 0 && row->bGroupID != ctx.expected_group)
        result = ConnectResult::NoChannel;
    else if (row->bChannel != bChannel)
        result = ConnectResult::NoChannel;

    spdlog::info("CS_CONNECT_REQ uid={} key={} char={} ch={} ver={} -> {}",
        dwUserID, dwKEY, dwID, bChannel, wVersion,
        ConnectResultName(result));

    // T4 audit: structured login event for the operations dashboard.
    // Always emitted (success and failure) so security tooling can
    // alert on InvalidChar / Internal spikes.
    if (ctx.audit)
    {
        audit::LoginAttemptEvent ev{};
        ev.hdr.corr = ctx.audit->NextCorrelation();
        ev.user_id  = dwUserID;
        ev.key      = dwKEY;
        ev.char_id  = dwID;
        ev.channel  = bChannel;
        ev.result   = static_cast<std::uint8_t>(result);
        ev.version  = wVersion;
        ctx.audit->Emit(ev);
    }

    // Register the session under its char id BEFORE the ack flushes
    // so a fast world reply (DM_LOADCHAR_REQ on this same char) can
    // resolve back to this socket. Bind overwrites any stale entry —
    // the legacy m_mapPLAYER had the same "last-write-wins" behavior
    // when a duplicate connect raced an outstanding session. Also
    // pre-binds the per-channel presence entry so subsequent
    // CS_MOVE_REQ broadcasts know which channel the client is on.
    if (result == ConnectResult::Ok && ctx.session_reg)
        ctx.session_reg->Bind(dwID, sess);
    if (result == ConnectResult::Ok && ctx.presence)
        ctx.presence->Bind(dwID, bChannel, sess);

    co_await sess->SendPacket(
        static_cast<std::uint16_t>(MessageId::CS_CONNECT_ACK),
        EncodeConnectAck(result));

    // On success, announce the new char to the World peer so it can
    // route MW_/DM_ traffic our way. Mirrors the legacy
    // CSHandler.cpp::OnCS_CONNECT_REQ tail-call into
    // SendMW_ADDCHAR_ACK (SSSender.cpp:237). The send is fire-and-
    // forget — IsConnected gate prevents queueing into a dead peer
    // (F5 doesn't buffer; F5+ phases may add a retry queue).
    if (result == ConnectResult::Ok && ctx.world_client &&
        ctx.world_client->IsConnected())
    {
        const bool sent = co_await ctx.world_client->SendPacket(
            static_cast<std::uint16_t>(MessageId::MW_ADDCHAR_ACK),
            EncodeAddCharAck(dwID, dwKEY, dwIPAddr, wPort, dwUserID));
        if (!sent)
            spdlog::warn("MW_ADDCHAR_ACK uid={} char={}: world send returned "
                         "false (peer dropped between IsConnected and send)",
                dwUserID, dwID);
    }
    else if (result == ConnectResult::Ok && !ctx.world_client)
    {
        spdlog::debug("MW_ADDCHAR_ACK uid={} char={}: world peer not "
                      "configured — char registration skipped",
            dwUserID, dwID);
    }
    else if (result == ConnectResult::Ok)
    {
        spdlog::warn("MW_ADDCHAR_ACK uid={} char={}: world peer disconnected "
                     "— char registration deferred",
            dwUserID, dwID);
    }
}

boost::asio::awaitable<void>
OnConReadyReq(std::shared_ptr<tnetlib::AsioSession> sess,
              std::vector<std::byte>                body,
              const HandlerContext&                 ctx)
{
    using tnetlib::protocol::MessageId;

    // CS_CONREADY_REQ has no body fields — it's a one-shot signal from
    // the client that it's done loading the local scene. Legacy
    // CSHandler.cpp:402 calls InitMap() / EnterMAP(), which floods the
    // client with its own CS_CHARINFO_ACK plus every surrounding entity
    // (other players, monsters, NPCs). This bounded step sends the
    // player's own CS_CHARINFO_ACK from the loaded snapshot; the
    // surrounding-entity AOI flood lands with the gameplay / spawn pass.
    (void)body;

    std::uint32_t cid = 0;
    if (ctx.session_reg)
    {
        if (const auto found = ctx.session_reg->FindCharIdBySession(sess.get()))
            cid = *found;
    }

    std::optional<CharSnapshot> snap;
    if (cid != 0 && ctx.char_state)
        snap = ctx.char_state->Get(cid);

    // No snapshot means the World load handshake (DM_LOADCHAR /
    // MW_ENTERSVR) hasn't populated char_state for this char yet — the
    // client readied before its data arrived. Nothing to send; the
    // enter flood will be driven once the snapshot lands.
    if (!snap)
    {
        spdlog::info("CS_CONREADY_REQ char={} — no loaded snapshot yet, "
                     "CHARINFO_ACK skipped", cid);
        co_return;
    }

    spdlog::info("CS_CONREADY_REQ char={} name='{}' map={} — CS_CHARINFO_ACK "
                 "(AOI flood deferred)", cid, snap->szNAME, snap->wMapID);

    const auto ack = EncodeCharInfoAck(*snap, FormatServerClock());
    co_await sess->SendPacket(
        static_cast<std::uint16_t>(MessageId::CS_CHARINFO_ACK), ack);
}

} // namespace tmapsvr
