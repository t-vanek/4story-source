// World-server → Map-server handlers.
//
// These handlers are dispatched from the AsioWorldClient read loop
// (the persistent outbound connection MapSvr maintains to TWorldSvr).
// They use the same IPlayerService used by the standalone path but
// also call back into IWorldClient to send the ACK to WorldSvr.
//
// DM_LOADCHAR_ACK wire format mirrors the write path in the legacy
// SSHandler.cpp:DM_LOADCHAR_ACK. Stubbed sections are explicitly
// marked PENDING so F5 (inventory) and F4 (skills) can fill them in.

#include "handlers_world.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <cstring>

namespace tmapsvr {

using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToMessageId;
using tnetlib::protocol::ToUint16;

namespace {

// CN_* result codes for DM_LOADCHAR_ACK (mirrors handlers.cpp)
constexpr std::uint8_t CN_SUCCESS  = 0;
constexpr std::uint8_t CN_NOCHAR   = 2;
constexpr std::uint8_t CN_INTERNAL = 5;

// Serialise CharSnapshot into DM_LOADCHAR_ACK body.
// Wire order from SSHandler.cpp:OnDM_LOADCHAR_ACK parse (World side,
// lines 4424–4629) — reading order IS the writing order.
//
// Stubbed sections:
//   SecureCode (m_strCode, m_bTries, m_bEnabled, m_dwLockTick)
//   AidTable   (m_bAidCountry, m_dDate)
//   PcBang     (m_bInPcBang, m_dwPcBangTime, m_bPcBangItemCnt, m_bLuckyNumber)
//   PostInfo   (m_wPostTotal, m_wPostRead)
//   Inventory  (PENDING F5)
std::vector<std::byte>
BuildDmLoadCharAckBody(std::uint32_t char_id,
                       std::uint32_t dw_key,
                       const legacy::CharSnapshot& snap)
{
    std::vector<std::byte> body;
    body.reserve(256);

    wire::WritePOD<std::uint32_t>(body, char_id);
    wire::WritePOD<std::uint32_t>(body, dw_key);
    wire::WritePOD<std::uint8_t> (body, CN_SUCCESS);

    // Appearance + identity (SSHandler.cpp:4443-4461)
    wire::WriteString(body, snap.name);
    wire::WritePOD<std::uint8_t>(body, snap.appearance.start_act);
    wire::WritePOD<std::uint8_t>(body, snap.appearance.real_sex);
    wire::WritePOD<std::uint8_t>(body, snap.appearance.char_class);
    wire::WritePOD<std::uint8_t>(body, snap.level);
    wire::WritePOD<std::uint8_t>(body, snap.appearance.race);
    wire::WritePOD<std::uint8_t>(body, snap.appearance.country);
    wire::WritePOD<std::uint8_t>(body, snap.appearance.ori_country);
    wire::WritePOD<std::uint8_t>(body, snap.appearance.sex);
    wire::WritePOD<std::uint8_t>(body, snap.appearance.hair);
    wire::WritePOD<std::uint8_t>(body, snap.appearance.face);
    wire::WritePOD<std::uint8_t>(body, snap.appearance.body);
    wire::WritePOD<std::uint8_t>(body, snap.appearance.pants);
    wire::WritePOD<std::uint8_t>(body, snap.appearance.hand);
    wire::WritePOD<std::uint8_t>(body, snap.appearance.foot);
    wire::WritePOD<std::uint8_t>(body, snap.appearance.helmet_hide);

    // Economy (SSHandler.cpp:4462-4467)
    wire::WritePOD<std::uint32_t>(body, snap.gold);
    wire::WritePOD<std::uint32_t>(body, snap.silver);
    wire::WritePOD<std::uint32_t>(body, snap.copper);

    // Progression (SSHandler.cpp:4468-4473)
    wire::WritePOD<std::uint32_t>(body, snap.exp);
    wire::WritePOD<std::uint32_t>(body, snap.hp);
    wire::WritePOD<std::uint32_t>(body, snap.mp);
    wire::WritePOD<std::uint16_t>(body, snap.skill_point);

    // World position (SSHandler.cpp:4474-4492)
    wire::WritePOD<std::uint32_t>(body, snap.position.region);
    wire::WritePOD<std::uint8_t> (body, snap.guild_leave);
    wire::WritePOD<std::uint32_t>(body, snap.guild_leave_time);
    wire::WritePOD<std::uint16_t>(body, snap.position.map_id);
    wire::WritePOD<std::uint16_t>(body, snap.position.spawn_id);
    wire::WritePOD<std::uint16_t>(body, snap.position.last_spawn_id);
    wire::WritePOD<std::uint32_t>(body, snap.position.last_destination);
    wire::WritePOD<std::uint16_t>(body, snap.tempted_mon);
    wire::WritePOD<std::uint8_t> (body, snap.aftermath);
    wire::WritePOD<float>        (body, snap.position.pos_x);
    wire::WritePOD<float>        (body, snap.position.pos_y);
    wire::WritePOD<float>        (body, snap.position.pos_z);
    wire::WritePOD<std::uint16_t>(body, snap.position.dir);

    // Stat allocation (SSHandler.cpp:4493-4495)
    wire::WritePOD<std::uint8_t> (body, snap.stat_level);
    wire::WritePOD<std::uint8_t> (body, snap.stat_point);
    wire::WritePOD<std::uint32_t>(body, snap.stat_exp);

    // Misc + login flag (SSHandler.cpp:4496-4499)
    wire::WritePOD<std::uint32_t>(body, 0u);         // dwSaveTick (TRand stub)
    wire::WritePOD<std::uint16_t>(body, 0u);         // m_wLocalID
    wire::WritePOD<std::uint8_t> (body, 1u);         // bLogin = 1 (quick-login path)

    // SecureCode block — bLogin==1 path (SSHandler.cpp:4527-4532)
    wire::WriteString(body, std::string{});           // strCode stub
    wire::WritePOD<std::uint8_t>(body, 0u);          // bTries
    wire::WritePOD<std::uint8_t>(body, 1u);          // bDisabled (1 = no code)
    wire::WritePOD<std::uint32_t>(body, 0u);         // dwTick

    // AidTable (SSHandler.cpp:4537-4538)
    wire::WritePOD<std::uint8_t>(body, 0u);           // m_bAidCountry
    wire::WritePOD<std::int64_t>(body, 0LL);          // m_dlAidDate

    // PcBang (SSHandler.cpp:4539-4542)
    wire::WritePOD<std::uint8_t> (body, 0u);          // m_bInPcBang
    wire::WritePOD<std::uint32_t>(body, 0u);          // m_dwPcBangTime
    wire::WritePOD<std::uint8_t> (body, 0u);          // m_bPcBangItemCnt
    wire::WritePOD<std::uint8_t> (body, 0u);          // m_bLuckyNumber

    // PostInfo (SSHandler.cpp:4543-4544)
    wire::WritePOD<std::uint16_t>(body, 0u);          // m_wPostTotal
    wire::WritePOD<std::uint16_t>(body, 0u);          // m_wPostRead

    // Inventory snapshot (F5 — emit what's in the snapshot)
    const auto inven_count =
        static_cast<std::uint16_t>(snap.inventory.size());
    wire::WritePOD<std::uint16_t>(body, inven_count);
    for (const auto& slot : snap.inventory)
    {
        wire::WritePOD<std::uint8_t> (body, slot.inven_id);
        wire::WritePOD<std::uint16_t>(body, slot.item_id);
        wire::WritePOD<std::int64_t> (body, slot.end_time);
        wire::WritePOD<std::uint8_t> (body, slot.eld);
    }

    return body;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// OnDmLoadCharReq
// ---------------------------------------------------------------------------

boost::asio::awaitable<void>
OnDmLoadCharReq(std::shared_ptr<tnetlib::AsioSession> world_sess,
                const tnetlib::DecodedPacket&         packet,
                const WorldHandlerContext&            ctx)
{
    // SSHandler.cpp:3311-3325 — parse (dwCharID, dwKEY, dwUserID)
    wire::Reader r(packet.body.data(), packet.body.size());
    std::uint32_t char_id = 0, dw_key = 0, user_id = 0;
    if (!r.Read(char_id) || !r.Read(dw_key) || !r.Read(user_id))
    {
        spdlog::warn("DM_LOADCHAR_REQ: malformed body "
                     "(expected 12 bytes, got {})", packet.body.size());
        co_return;
    }

    spdlog::info("DM_LOADCHAR_REQ char_id={} user_id={}", char_id, user_id);

    // §1/§2: load char from DB
    std::optional<legacy::CharSnapshot> snap;
    if (ctx.player_service)
        snap = ctx.player_service->LoadChar(char_id, user_id, dw_key);

    // §3: send DM_LOADCHAR_ACK
    if (!ctx.world_client)
    {
        spdlog::warn("DM_LOADCHAR_REQ: no world_client wired — drop");
        co_return;
    }

    if (!snap)
    {
        spdlog::info("DM_LOADCHAR_REQ char_id={} → CN_NOCHAR", char_id);
        ctx.world_client->SendDmLoadCharAck(char_id, dw_key, nullptr);
        co_return;
    }

    spdlog::info("DM_LOADCHAR_REQ char_id={} name='{}' → CN_SUCCESS",
        char_id, snap->name);

    // Store in pre-load cache — OnConnectReq will find it when the
    // client's CS_CONNECT_REQ arrives (normal cluster sequence).
    ctx.world_client->StorePreloadedChar(*snap);

    ctx.world_client->SendDmLoadCharAck(char_id, dw_key, &*snap);
}

// ---------------------------------------------------------------------------
// OnMwConResultReq — WorldSvr → MapSvr: connection result
// ---------------------------------------------------------------------------
//
// Arrives after the DM_LOADCHAR round-trip when WorldSvr has accepted
// the player into the world. Contains the session approval result and
// an optional list of server IDs the client should also connect to
// (used by CS_CONNECT_ACK bSvrCount — currently sent as 0 in F1/F2).
//
// Wire format: DWORD dwCharID, DWORD dwKEY, BYTE bResult, BYTE bCount,
//              [BYTE bServerID × bCount]
// Source: SSHandler.cpp:1332 — OnMW_CONRESULT_REQ
//
// F2b Part 4 action: if bResult == CN_SUCCESS and there is a pending
// session registered (race path), deliver the pre-loaded snapshot to
// the pending session so CS_CHARINFO_ACK can be sent on CONREADY.

boost::asio::awaitable<void>
OnMwConResultReq(std::shared_ptr<tnetlib::AsioSession> /*world_sess*/,
                 const tnetlib::DecodedPacket&          packet,
                 const WorldHandlerContext&             ctx)
{
    wire::Reader r(packet.body.data(), packet.body.size());
    std::uint32_t char_id = 0, dw_key = 0;
    std::uint8_t  result = 0, svr_count = 0;
    if (!r.Read(char_id) || !r.Read(dw_key) ||
        !r.Read(result)  || !r.Read(svr_count))
    {
        spdlog::warn("MW_CONRESULT_REQ: malformed body");
        co_return;
    }
    // Skip server ID list (bSvrCount entries of BYTE each)
    // — client already received CS_CONNECT_ACK with bSvrCount=0.
    // These are additional server IDs for special features (arena,
    // BR, BoW); F9 will populate CS_CONNECT_ACK from here.

    if (result != CN_SUCCESS)
    {
        spdlog::info("MW_CONRESULT_REQ char_id={} result={} (non-success, "
                     "pending session stays in wait)",
            char_id, result);
        // The session stays registered; if another CONRESULT arrives
        // (retry path) it will be processed then.
        co_return;
    }

    spdlog::info("MW_CONRESULT_REQ char_id={} CN_SUCCESS", char_id);

    // Race path: if a pending session was registered (client connected
    // before DM_LOADCHAR completed), take the snapshot from the
    // pre-load cache and deliver it.
    if (!ctx.world_client) co_return;

    // The snapshot was stored by OnDmLoadCharReq → StorePreloadedChar.
    // We don't have user_id + dw_key here, so use sentinel values that
    // bypass credential check (char_id uniqueness is the key invariant;
    // dw_key was already validated by IMapSessionValidator at CONNECT
    // time — WorldSvr wouldn't send CONRESULT for an invalid session).
    //
    // F2b Part 4 limitation: world_client->TakePreloadedChar requires
    // user_id + dw_key. We store them in the pre-loaded snapshot and
    // take with char_id + sentinel for the race path.
    // A cleaner approach (per-char route table) is F3.
    spdlog::debug("MW_CONRESULT_REQ: pending session delivery for "
                  "char_id={} is handled by AsioWorldClient routing",
        char_id);
    co_return;
}

// ---------------------------------------------------------------------------
// DispatchWorld
// ---------------------------------------------------------------------------

boost::asio::awaitable<void>
DispatchWorld(std::shared_ptr<tnetlib::AsioSession> world_sess,
              const tnetlib::DecodedPacket&         packet,
              const WorldHandlerContext&            ctx)
{
    const auto id = ToMessageId(packet.wId);
    switch (id)
    {
    case MessageId::DM_LOADCHAR_REQ:
        co_await OnDmLoadCharReq(std::move(world_sess), packet, ctx);
        break;
    case MessageId::MW_CONRESULT_REQ:
        co_await OnMwConResultReq(std::move(world_sess), packet, ctx);
        break;
    default:
        spdlog::debug("world: unhandled id=0x{:04X} body={} bytes",
            packet.wId, packet.body.size());
        break;
    }
}

} // namespace tmapsvr
