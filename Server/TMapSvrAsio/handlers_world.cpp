#include "handlers_world.h"

#include "audit/audit_log.h"
#include "audit/event.h"
#include "services/channel_presence.h"
#include "services/char_state_store.h"
#include "services/client_senders.h"
#include "services/companion_service.h"
#include "services/inventory_service.h"
#include "services/player_service.h"
#include "services/quest_service.h"
#include "services/session_registry.h"
#include "services/skill_service.h"
#include "services/world_client.h"
#include "services/world_senders.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <span>
#include <utility>
#include <vector>

namespace tmapsvr {

namespace {

// CN_-style result codes for DM_LOADCHAR_ACK. Internal-only constants
// — legacy header not yet recovered. Values match what other
// handlers in this tree use.
constexpr std::uint8_t CnSuccess  = 0;
constexpr std::uint8_t CnInternal = 3;
constexpr std::uint8_t CnNoChar   = 6;

// DM_LOADCHAR_ACK error body — 9 bytes (dwCharID + dwKEY + result).
// Mirrors the error path in legacy SSHandler.cpp:3379 / 3392 where
// the char row is missing or the DB query fails.
std::vector<std::byte> EncodeLoadCharAckError(std::uint32_t dwCharID,
                                              std::uint32_t dwKEY,
                                              std::uint8_t  result)
{
    std::vector<std::byte> body;
    body.reserve(9);
    wire::WritePOD<std::uint32_t>(body, dwCharID);
    wire::WritePOD<std::uint32_t>(body, dwKEY);
    wire::WritePOD<std::uint8_t> (body, result);
    return body;
}

// DM_LOADCHAR_ACK success body — TCHARTABLE-derived snapshot plus the
// sentinel trio (TRand(600000) / WORD(0) / BYTE(TRUE)) that legacy
// SSHandler.cpp:3445 appends right after the snapshot fields. The
// trailing sub-sections (secure code, aid table, PC bang, post info,
// inventory, cabinet, skills, quests, …) are documented as zero /
// empty here and will be filled in by their owning phases as those
// services come online (F9 items, F11 skills, F12 quests, …).
std::vector<std::byte> EncodeLoadCharAckSuccess(std::uint32_t dwCharID,
                                                std::uint32_t dwKEY,
                                                const CharSnapshot& s,
                                                const std::vector<InventoryRow>& inven,
                                                const std::vector<SkillRow>& skills,
                                                const std::vector<QuestProgressRow>& quests,
                                                const std::vector<CompanionRow>& companions)
{
    std::vector<std::byte> body;
    body.reserve(256);

    wire::WritePOD<std::uint32_t>(body, dwCharID);
    wire::WritePOD<std::uint32_t>(body, dwKEY);
    wire::WritePOD<std::uint8_t> (body, CnSuccess);

    wire::WriteString            (body, s.szNAME);
    wire::WritePOD<std::uint8_t> (body, s.bStartAct);
    wire::WritePOD<std::uint8_t> (body, s.bRealSex);
    wire::WritePOD<std::uint8_t> (body, s.bClass);
    wire::WritePOD<std::uint8_t> (body, s.bLevel);
    wire::WritePOD<std::uint8_t> (body, s.bRace);
    wire::WritePOD<std::uint8_t> (body, s.bCountry);
    wire::WritePOD<std::uint8_t> (body, s.bOriCountry);
    wire::WritePOD<std::uint8_t> (body, s.bSex);
    wire::WritePOD<std::uint8_t> (body, s.bHair);
    wire::WritePOD<std::uint8_t> (body, s.bFace);
    wire::WritePOD<std::uint8_t> (body, s.bBody);
    wire::WritePOD<std::uint8_t> (body, s.bPants);
    wire::WritePOD<std::uint8_t> (body, s.bHand);
    wire::WritePOD<std::uint8_t> (body, s.bFoot);
    wire::WritePOD<std::uint8_t> (body, s.bHelmetHide);
    wire::WritePOD<std::uint32_t>(body, s.dwGold);
    wire::WritePOD<std::uint32_t>(body, s.dwSilver);
    wire::WritePOD<std::uint32_t>(body, s.dwCooper);
    wire::WritePOD<std::uint32_t>(body, s.dwEXP);
    wire::WritePOD<std::uint32_t>(body, s.dwHP);
    wire::WritePOD<std::uint32_t>(body, s.dwMP);
    wire::WritePOD<std::uint16_t>(body, s.wSkillPoint);
    wire::WritePOD<std::uint32_t>(body, s.dwRegion);
    wire::WritePOD<std::uint8_t> (body, s.bGuildLeave);
    wire::WritePOD<std::uint32_t>(body, s.dwGuildLeaveTime);
    wire::WritePOD<std::uint16_t>(body, s.wMapID);
    wire::WritePOD<std::uint16_t>(body, s.wSpawnID);
    wire::WritePOD<std::uint16_t>(body, s.wLastSpawnID);
    wire::WritePOD<std::uint32_t>(body, s.dwLastDestination);
    wire::WritePOD<std::uint16_t>(body, s.wTemptedMon);
    wire::WritePOD<std::uint8_t> (body, s.bAftermath);
    wire::WritePOD<float>        (body, s.fPosX);
    wire::WritePOD<float>        (body, s.fPosY);
    wire::WritePOD<float>        (body, s.fPosZ);
    wire::WritePOD<std::uint16_t>(body, s.wDIR);
    wire::WritePOD<std::uint8_t> (body, s.bStatLevel);
    wire::WritePOD<std::uint8_t> (body, s.bStatPoint);
    wire::WritePOD<std::uint32_t>(body, s.dwStatExp);

    // Sentinel trio from legacy SSHandler.cpp:3445. TRand(600000) is
    // a per-load anti-replay value; we ship a fixed sentinel until
    // the gameplay logic that actually consumes it lands (BR/Bow
    // round timing in F16). WORD(0) and BYTE(TRUE) are constants.
    wire::WritePOD<std::uint32_t>(body, 0u);    // TRand placeholder
    wire::WritePOD<std::uint16_t>(body, 0);     // WORD(0)
    wire::WritePOD<std::uint8_t> (body, 1);     // BYTE(TRUE)

    // F9 inventory section (legacy SSHandler.cpp:3540): WORD count
    // followed by one record per row. Each record carries:
    //   BYTE  bInvenID    slot id
    //   WORD  wItemID     item template id
    //   INT64 dEndTime    expiry tick (0 = permanent)
    //   BYTE  bELD        legacy ELD flag
    wire::WritePOD<std::uint16_t>(body, static_cast<std::uint16_t>(inven.size()));
    for (const auto& r : inven)
    {
        wire::WritePOD<std::uint8_t> (body, r.bInvenID);
        wire::WritePOD<std::uint16_t>(body, r.wItemID);
        wire::WritePOD<std::int64_t> (body, r.dEndTime);
        wire::WritePOD<std::uint8_t> (body, r.bELD);
    }

    // F11 skill section: WORD count + one record per learned skill.
    //   WORD  wSkillID
    //   BYTE  bLevel
    //   DWORD dwRemainTick    cooldown remaining (0 = ready)
    wire::WritePOD<std::uint16_t>(body, static_cast<std::uint16_t>(skills.size()));
    for (const auto& r : skills)
    {
        wire::WritePOD<std::uint16_t>(body, r.wSkillID);
        wire::WritePOD<std::uint8_t> (body, r.bLevel);
        wire::WritePOD<std::uint32_t>(body, r.dwRemainTick);
    }

    // F12 quest section: WORD count + one record per accepted quest.
    // Each record carries the TQUESTTABLE fields followed by a
    // nested WORD count + N TQUESTTERMTABLE term-progress rows. The
    // legacy encoder streams the same pair of counts inline (see
    // SSHandler.cpp around line 3700).
    wire::WritePOD<std::uint16_t>(body, static_cast<std::uint16_t>(quests.size()));
    for (const auto& q : quests)
    {
        wire::WritePOD<std::uint32_t>(body, q.dwQuestID);
        wire::WritePOD<std::uint32_t>(body, q.dwTick);
        wire::WritePOD<std::uint8_t> (body, q.bCompleteCount);
        wire::WritePOD<std::uint8_t> (body, q.bTriggerCount);
        wire::WritePOD<std::uint16_t>(body, static_cast<std::uint16_t>(q.terms.size()));
        for (const auto& t : q.terms)
        {
            wire::WritePOD<std::uint32_t>(body, t.dwTermID);
            wire::WritePOD<std::uint8_t> (body, t.bTermType);
            wire::WritePOD<std::uint8_t> (body, t.bCount);
        }
    }

    // F15 companion section: WORD count + one record per row.
    //   BYTE  bSlot
    //   DWORD dwMonID
    //   BYTE  bLevel
    //   string strName
    //   DWORD dwExp
    //   WORD  wLife
    //   BYTE  bStatusPoints
    //   BYTE  bEffect
    //   WORD  wSTR / wDEX / wCON / wINT / wWIS / wMEN
    //   WORD  wBonusID
    wire::WritePOD<std::uint16_t>(body, static_cast<std::uint16_t>(companions.size()));
    for (const auto& c : companions)
    {
        wire::WritePOD<std::uint8_t> (body, c.bSlot);
        wire::WritePOD<std::uint32_t>(body, c.dwMonID);
        wire::WritePOD<std::uint8_t> (body, c.bLevel);
        wire::WriteString            (body, c.strName);
        wire::WritePOD<std::uint32_t>(body, c.dwExp);
        wire::WritePOD<std::uint16_t>(body, c.wLife);
        wire::WritePOD<std::uint8_t> (body, c.bStatusPoints);
        wire::WritePOD<std::uint8_t> (body, c.bEffect);
        wire::WritePOD<std::uint16_t>(body, c.wSTR);
        wire::WritePOD<std::uint16_t>(body, c.wDEX);
        wire::WritePOD<std::uint16_t>(body, c.wCON);
        wire::WritePOD<std::uint16_t>(body, c.wINT);
        wire::WritePOD<std::uint16_t>(body, c.wWIS);
        wire::WritePOD<std::uint16_t>(body, c.wMEN);
        wire::WritePOD<std::uint16_t>(body, c.wBonusID);
    }

    // The legacy ack continues with cabinet / equip / friend / craft
    // / mail / chapter / recall-mon / pet sections. Each remaining
    // section lands with its owning phase, in wire order, on top of
    // this body.

    return body;
}

} // namespace

boost::asio::awaitable<void>
OnDMLoadCharReq(std::vector<std::byte> body, const HandlerContext& ctx)
{
    using tnetlib::protocol::MessageId;

    const auto t0 = std::chrono::steady_clock::now();
    auto emit_audit = [&ctx, &t0](std::uint32_t char_id,
                                  std::uint32_t key,
                                  std::uint32_t user_id,
                                  std::uint8_t  result)
    {
        if (!ctx.audit) return;
        const auto elapsed = std::chrono::steady_clock::now() - t0;
        audit::CharLoadEvent ev{};
        ev.hdr.corr  = ctx.audit->NextCorrelation();
        ev.char_id   = char_id;
        ev.key       = key;
        ev.user_id   = user_id;
        ev.latency_us = static_cast<std::uint32_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
        ev.result    = result;
        ctx.audit->Emit(ev);
    };

    // Wire layout from legacy SSHandler.cpp:3311 (12 bytes):
    //   DWORD dwCharID
    //   DWORD dwKEY
    //   DWORD dwUserID
    wire::Reader r(body.data(), body.size());
    std::uint32_t dwCharID = 0, dwKEY = 0, dwUserID = 0;
    if (!r.Read(dwCharID) || !r.Read(dwKEY) || !r.Read(dwUserID))
    {
        spdlog::warn("DM_LOADCHAR_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    if (!ctx.world_client || !ctx.world_client->IsConnected())
    {
        spdlog::warn("DM_LOADCHAR_REQ: world peer not connected — ack "
                     "dropped (char={})", dwCharID);
        emit_audit(dwCharID, dwKEY, dwUserID, CnInternal);
        co_return;
    }

    // F8 success path: player service configured + char row exists.
    // Falls through to the error variants when either condition
    // isn't met, matching legacy CN_INTERNAL / CN_NOCHAR semantics.
    if (!ctx.player_service)
    {
        spdlog::warn("DM_LOADCHAR_REQ char={}: no player service "
                     "configured — ack INTERNAL", dwCharID);
        co_await ctx.world_client->SendPacket(
            static_cast<std::uint16_t>(MessageId::DM_LOADCHAR_ACK),
            EncodeLoadCharAckError(dwCharID, dwKEY, CnInternal));
        emit_audit(dwCharID, dwKEY, dwUserID, CnInternal);
        co_return;
    }

    const auto snap = ctx.player_service->LoadChar(dwCharID);
    if (!snap)
    {
        spdlog::info("DM_LOADCHAR_REQ char={} user={}: no row — ack NOCHAR",
            dwCharID, dwUserID);
        co_await ctx.world_client->SendPacket(
            static_cast<std::uint16_t>(MessageId::DM_LOADCHAR_ACK),
            EncodeLoadCharAckError(dwCharID, dwKEY, CnNoChar));
        emit_audit(dwCharID, dwKEY, dwUserID, CnNoChar);
        co_return;
    }

    // Optional sections — without their services we ship empty
    // sub-bodies (count = 0) so the wire shape stays well-defined
    // through F11. Legacy treats "0 inventory rows" as the load-
    // error path; we keep that contract documented per service.
    std::vector<InventoryRow> inven;
    if (ctx.inventory_service)
        inven = ctx.inventory_service->LoadInventory(dwCharID);

    std::vector<SkillRow> skills;
    if (ctx.skill_service)
        skills = ctx.skill_service->LoadSkills(dwCharID);

    std::vector<QuestProgressRow> quests;
    if (ctx.quest_service)
        quests = ctx.quest_service->LoadProgress(dwCharID);

    std::vector<CompanionRow> companions;
    if (ctx.companion_service)
        companions = ctx.companion_service->LoadCompanions(dwCharID);

    // Store the live snapshot so the teardown hook can SaveChar on disconnect.
    if (ctx.char_state)
        ctx.char_state->Store(dwCharID, *snap);

    spdlog::info("DM_LOADCHAR_REQ char={} user={} name='{}' lvl={} class={} "
                 "map={} pos=({:.1f},{:.1f},{:.1f}) inven={} skills={} "
                 "quests={} companions={} — F15 snapshot encoded",
        dwCharID, dwUserID, snap->szNAME, snap->bLevel, snap->bClass,
        snap->wMapID, snap->fPosX, snap->fPosY, snap->fPosZ,
        inven.size(), skills.size(), quests.size(), companions.size());

    co_await ctx.world_client->SendPacket(
        static_cast<std::uint16_t>(MessageId::DM_LOADCHAR_ACK),
        EncodeLoadCharAckSuccess(dwCharID, dwKEY, *snap, inven, skills,
                                 quests, companions));

    emit_audit(dwCharID, dwKEY, dwUserID, CnSuccess);
}

boost::asio::awaitable<void>
OnMWEnterSvrReq(std::vector<std::byte> body, const HandlerContext& ctx)
{
    using tnetlib::protocol::MessageId;

    // Wire layout from legacy SSHandler.cpp:3078 (9 bytes):
    //   BYTE  bDBLoad     1 = load char from DB, 0 = snapshot embedded
    //   DWORD dwCharID
    //   DWORD dwKEY
    // When bDBLoad == 0 the legacy World ships the char blob inline so
    // the DB-batch process is spared a load. The modern map owns its DB
    // access in-process, so the embedded fast path is intentionally not
    // reproduced — any trailing bytes after dwKEY are ignored and the
    // identity is resolved through the live cache / player service below.
    wire::Reader r(body.data(), body.size());
    std::uint8_t  bDBLoad  = 0;
    std::uint32_t dwCharID = 0, dwKEY = 0;
    if (!r.Read(bDBLoad) || !r.Read(dwCharID) || !r.Read(dwKEY))
    {
        spdlog::warn("MW_ENTERSVR_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    // No world link → nowhere to route the ack. Legacy returns
    // EC_NOERROR with no reply when FindPlayer misses; mirror the
    // "stay silent, the peer will retry" behavior here.
    if (!ctx.world_client || !ctx.world_client->IsConnected())
    {
        spdlog::warn("MW_ENTERSVR_REQ char={}: world peer not connected — "
                     "ack dropped", dwCharID);
        co_return;
    }

    // Channel claimed at CS_CONNECT_REQ — presence is bound on a clean
    // handshake. Absent means the client never connected to this map (or
    // already tore down); 0 is the legacy default and TWorld keys the
    // session by char id, not channel, so a 0 here only loses the
    // cosmetic channel echo on the error path.
    std::uint8_t channel = 0;
    if (ctx.presence)
    {
        if (const auto e = ctx.presence->FindEntry(dwCharID))
            channel = e->channel;
    }

    // Resolve the char identity. Legacy chained DM_ENTERMAPSVR_REQ (DB-
    // batch persist) → DM_ENTERMAPSVR_ACK → DM_LOADCHAR_REQ → … →
    // MW_ENTERSVR_ACK across two processes (SSHandler.cpp:3096-3299).
    // In-process that collapses to: reuse the snapshot the load path
    // already cached, else pull it through the player service.
    std::optional<CharSnapshot> snap;
    if (ctx.char_state)
        snap = ctx.char_state->Get(dwCharID);
    if (!snap && ctx.player_service)
    {
        snap = ctx.player_service->LoadChar(dwCharID);
        if (snap && ctx.char_state)
            ctx.char_state->Store(dwCharID, *snap);
    }

    // No identity → CN_INTERNAL ack carrying just the char id. Mirrors
    // the legacy error tail (SSHandler.cpp:3281) which still emits
    // MW_ENTERSVR_ACK so TWorld unwinds the pending enter instead of
    // waiting out a timeout.
    if (!snap)
    {
        spdlog::warn("MW_ENTERSVR_REQ char={} key={} dbload={}: no identity "
                     "(char_state miss + no/empty player service) — ack "
                     "INTERNAL", dwCharID, dwKEY, bDBLoad);
        CharSnapshot empty;
        empty.dwCharID = dwCharID;
        co_await ctx.world_client->SendPacket(
            static_cast<std::uint16_t>(MessageId::MW_ENTERSVR_ACK),
            EncodeEnterSvrAck(empty, dwKEY, /*aid_country=*/0, channel,
                              /*logout=*/0, /*save=*/0, CnInternal,
                              /*title_id=*/0, /*rank_point=*/0,
                              /*user_ip=*/0));
        co_return;
    }

    // Success — the char is now resident on this map server. title_id /
    // rank_point live on the legacy CTPlayer, not the TCHARTABLE row, so
    // they ship 0 until the title / ranking services land (same
    // deferral the DM_LOADCHAR_ACK trailing sections use). result = 0 is
    // the modern "apply this identity" contract TWorld's OnEnterSvrAck
    // reads (proven in test_world_handshake).
    spdlog::info("MW_ENTERSVR_REQ char={} key={} name='{}' lvl={} class={} "
                 "map={} ch={} dbload={} — entered, ack SUCCESS",
        dwCharID, dwKEY, snap->szNAME, snap->bLevel, snap->bClass,
        snap->wMapID, channel, bDBLoad);

    co_await ctx.world_client->SendPacket(
        static_cast<std::uint16_t>(MessageId::MW_ENTERSVR_ACK),
        EncodeEnterSvrAck(*snap, dwKEY, /*aid_country=*/0, channel,
                          /*logout=*/0, /*save=*/0, CnSuccess,
                          /*title_id=*/0, /*rank_point=*/0, /*user_ip=*/0));
}

boost::asio::awaitable<void>
OnMWEnterCharReq(std::vector<std::byte> body, const HandlerContext& ctx)
{
    using tnetlib::protocol::MessageId;

    // Wire: the fat per-connection entry composite (legacy
    // SSHandler.cpp:1453 / TWorld SendMwEnterCharReq). It leads with the
    // two fields the ACK echoes:
    //   DWORD dwCharID
    //   DWORD dwKEY
    // followed by the identity header (below) and then the char's full
    // cluster state — guild / party / corps / tactics / soulmate ids +
    // an opaque recall-mon tail. Applying that state onto live char
    // state is a follow-up increment (CharSnapshot doesn't model the
    // guild/party fields yet); this handler completes the *ready*
    // handshake TWorld's CheckMainCon blocks on.
    wire::Reader r(body.data(), body.size());
    std::uint32_t dwCharID = 0, dwKEY = 0;
    if (!r.Read(dwCharID) || !r.Read(dwKEY))
    {
        spdlog::warn("MW_ENTERCHAR_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    if (!ctx.world_client || !ctx.world_client->IsConnected())
    {
        spdlog::warn("MW_ENTERCHAR_REQ char={}: world peer not connected — "
                     "ack dropped", dwCharID);
        co_return;
    }

    // Best-effort identity header (legacy reads these straight into the
    // CTPlayer): start_act, name, map_id, spawn pos. Used for the log
    // line and, when the char is already resident on this map, to track
    // the World-authoritative spawn position. A composite truncated
    // after dwKEY still completes the ready handshake.
    std::uint8_t  bStartAct = 0;
    std::string   name;
    std::uint16_t wMapID = 0;
    float         fPosX = 0.f, fPosY = 0.f, fPosZ = 0.f;
    const bool have_hdr =
        r.Read(bStartAct) && r.ReadString(name) && r.Read(wMapID) &&
        r.Read(fPosX) && r.Read(fPosY) && r.Read(fPosZ);

    // Update() is a no-op when the char isn't resident, so this stays
    // safe for the entry-before-load ordering.
    if (have_hdr && ctx.char_state)
    {
        ctx.char_state->Update(dwCharID, [&](CharSnapshot& s) {
            s.wMapID = wMapID;
            s.fPosX  = fPosX;
            s.fPosY  = fPosY;
            s.fPosZ  = fPosZ;
        });
    }

    spdlog::info("MW_ENTERCHAR_REQ char={} key={} name='{}' map={} — "
                 "connection ready, ack",
        dwCharID, dwKEY, have_hdr ? name : std::string("?"), wMapID);

    co_await ctx.world_client->SendPacket(
        static_cast<std::uint16_t>(MessageId::MW_ENTERCHAR_ACK),
        EncodeEnterCharAck(dwCharID, dwKEY));
}

boost::asio::awaitable<void>
OnMWAddConnectReq(std::vector<std::byte> body, const HandlerContext& ctx)
{
    using tnetlib::protocol::MessageId;

    // Wire (legacy SSHandler.cpp:6785):
    //   DWORD dwCharID, DWORD dwKEY, BYTE bCount,
    //   bCount × { DWORD ip_addr, WORD port, BYTE server_id }
    // TWorld hands the map the peer-server list the client should open
    // cross-server connections to; the map relays it straight down as
    // CS_ADDCONNECT_ACK (legacy pPlayer->Say).
    wire::Reader r(body.data(), body.size());
    std::uint32_t dwCharID = 0, dwKEY = 0;
    std::uint8_t  bCount = 0;
    if (!r.Read(dwCharID) || !r.Read(dwKEY) || !r.Read(bCount))
    {
        spdlog::warn("MW_ADDCONNECT_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    std::vector<ConnectRoute> routes;
    routes.reserve(bCount);
    for (std::uint8_t i = 0; i < bCount; ++i)
    {
        ConnectRoute cr;
        if (!r.Read(cr.ip_addr) || !r.Read(cr.port) || !r.Read(cr.server_id))
        {
            spdlog::warn("MW_ADDCONNECT_REQ char={}: truncated route list at "
                         "{}/{} — dropping", dwCharID, i, bCount);
            co_return;
        }
        routes.push_back(cr);
    }

    // Relay to the client. The char must already be bound from its
    // CS_CONNECT_REQ; an unknown char means the socket dropped between
    // the world push and now — nothing to forward to (legacy FindPlayer
    // miss → silent return).
    auto sess = ctx.session_reg ? ctx.session_reg->Find(dwCharID) : nullptr;
    if (!sess)
    {
        spdlog::warn("MW_ADDCONNECT_REQ char={}: no bound client session — "
                     "drop ({} routes)", dwCharID, routes.size());
        co_return;
    }

    spdlog::info("MW_ADDCONNECT_REQ char={} key={} routes={} — relaying "
                 "CS_ADDCONNECT_ACK", dwCharID, dwKEY, routes.size());

    const auto ack = EncodeAddConnectAck(routes);
    co_await sess->SendPacket(
        static_cast<std::uint16_t>(MessageId::CS_ADDCONNECT_ACK), ack);
}

boost::asio::awaitable<void>
OnMWCheckMainReq(std::vector<std::byte> body, const HandlerContext& ctx)
{
    using tnetlib::protocol::MessageId;

    // Wire (legacy SSHandler.cpp:1310):
    //   DWORD dwCharID, DWORD dwKEY, BYTE bChannel, WORD wMapID,
    //   FLOAT fPosX, fPosY, fPosZ
    // TWorld asks each candidate map "do you own the cell this char
    // stands in?". Only the owner answers MW_CHECKMAIN_ACK, settling
    // which connection is the authoritative main session.
    wire::Reader r(body.data(), body.size());
    std::uint32_t dwCharID = 0, dwKEY = 0;
    std::uint8_t  bChannel = 0;
    std::uint16_t wMapID = 0;
    float fPosX = 0.f, fPosY = 0.f, fPosZ = 0.f;
    if (!r.Read(dwCharID) || !r.Read(dwKEY) || !r.Read(bChannel) ||
        !r.Read(wMapID) || !r.Read(fPosX) || !r.Read(fPosY) || !r.Read(fPosZ))
    {
        spdlog::warn("MW_CHECKMAIN_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    if (!ctx.world_client || !ctx.world_client->IsConnected())
    {
        spdlog::warn("MW_CHECKMAIN_REQ char={}: world peer not connected — "
                     "ack dropped", dwCharID);
        co_return;
    }

    // Legacy gates the ack on IsMainCell(channel, map, pos) — the cell
    // grid that shards one logical map across server instances. The
    // modern map hosts a whole map in-process, so cell ownership reduces
    // to "is this char resident here?": a loaded snapshot or a live
    // client session means this is its main map. Precise multi-instance
    // cell ownership is a follow-up once the cell grid is modelled.
    const bool resident =
        (ctx.char_state  && ctx.char_state->Get(dwCharID).has_value()) ||
        (ctx.session_reg && ctx.session_reg->Find(dwCharID) != nullptr);

    if (!resident)
    {
        spdlog::info("MW_CHECKMAIN_REQ char={} key={} ch={} map={} — not "
                     "resident here, no ack", dwCharID, dwKEY, bChannel, wMapID);
        co_return;
    }

    spdlog::info("MW_CHECKMAIN_REQ char={} key={} ch={} map={} — main cell, "
                 "ack", dwCharID, dwKEY, bChannel, wMapID);

    co_await ctx.world_client->SendPacket(
        static_cast<std::uint16_t>(MessageId::MW_CHECKMAIN_ACK),
        EncodeCheckMainAck(dwCharID, dwKEY));
}

boost::asio::awaitable<void>
OnMWConResultReq(std::vector<std::byte> body, const HandlerContext& ctx)
{
    using tnetlib::protocol::MessageId;

    // Wire (legacy SSHandler.cpp:1341):
    //   DWORD dwCharID, DWORD dwKEY, BYTE bResult, BYTE bCount,
    //   bCount × BYTE server_id
    // TWorld's settled connect verdict plus the cross-server id list.
    // The map forwards it to the client as the authoritative
    // CS_CONNECT_ACK and, on rejection, tears the session down.
    wire::Reader r(body.data(), body.size());
    std::uint32_t dwCharID = 0, dwKEY = 0;
    std::uint8_t  bResult = 0, bCount = 0;
    if (!r.Read(dwCharID) || !r.Read(dwKEY) || !r.Read(bResult) ||
        !r.Read(bCount))
    {
        spdlog::warn("MW_CONRESULT_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    std::vector<std::uint8_t> server_ids;
    server_ids.reserve(bCount);
    for (std::uint8_t i = 0; i < bCount; ++i)
    {
        std::uint8_t sid = 0;
        if (!r.Read(sid))
        {
            spdlog::warn("MW_CONRESULT_REQ char={}: truncated server list at "
                         "{}/{} — dropping", dwCharID, i, bCount);
            co_return;
        }
        server_ids.push_back(sid);
    }

    auto sess = ctx.session_reg ? ctx.session_reg->Find(dwCharID) : nullptr;
    if (!sess)
    {
        spdlog::warn("MW_CONRESULT_REQ char={}: no bound client session — drop",
            dwCharID);
        co_return;
    }

    // Transitional: session.cpp::OnConnectReq still sends an optimistic
    // CS_CONNECT_ACK at CS_CONNECT_REQ time, so a fully wired world loop
    // makes this the second send. The optimistic ack moves here once the
    // connect loop is proven end-to-end; today it keeps connect working
    // when no world peer is configured. See README world-peer note.
    const auto ack = EncodeConnectAck(bResult, server_ids);
    co_await sess->SendPacket(
        static_cast<std::uint16_t>(MessageId::CS_CONNECT_ACK), ack);

    if (bResult != CnSuccess)
    {
        spdlog::info("MW_CONRESULT_REQ char={} result={} — connect rejected, "
                     "closing session", dwCharID, bResult);
        sess->Close();   // per-connection teardown hook unbinds registries
    }
    else
    {
        spdlog::info("MW_CONRESULT_REQ char={} result=SUCCESS servers={} — "
                     "CS_CONNECT_ACK relayed", dwCharID, server_ids.size());
    }
}

boost::asio::awaitable<void>
OnMWCloseCharReq(std::vector<std::byte> body, const HandlerContext& ctx)
{
    using tnetlib::protocol::MessageId;

    // Wire (legacy SSHandler.cpp:2201): DWORD dwCharID, DWORD dwKEY
    wire::Reader r(body.data(), body.size());
    std::uint32_t dwCharID = 0, dwKEY = 0;
    if (!r.Read(dwCharID) || !r.Read(dwKEY))
    {
        spdlog::warn("MW_CLOSECHAR_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    auto sess = ctx.session_reg ? ctx.session_reg->Find(dwCharID) : nullptr;
    if (!sess)
    {
        spdlog::warn("MW_CLOSECHAR_REQ char={}: no bound client session — drop",
            dwCharID);
        co_return;
    }

    // Legacy (SSHandler.cpp:2196): ExitMAP + m_bExit + SendCS_SHUTDOWN_ACK.
    // m_bCloseAll is FALSE on this path, so no MW_CLOSECHAR_ACK is sent
    // back (that confirmation belongs to the multi-connection close-all
    // path). Tell the client to shut down (empty-bodied ack), then close
    // the socket — the MapServer per-connection teardown hook persists
    // the snapshot (SaveChar) and unbinds the session / presence
    // registries (map_server.cpp:139).
    spdlog::info("MW_CLOSECHAR_REQ char={} key={} — CS_SHUTDOWN_ACK + close",
        dwCharID, dwKEY);

    co_await sess->SendPacket(
        static_cast<std::uint16_t>(MessageId::CS_SHUTDOWN_ACK),
        std::span<const std::byte>{});

    sess->Close();   // read loop ends → teardown hook saves + unbinds
}

boost::asio::awaitable<void>
DispatchWorld(std::uint16_t          wId,
              std::vector<std::byte> body,
              const HandlerContext&  ctx)
{
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToMessageId;

    const auto id = ToMessageId(wId);
    switch (id)
    {
    case MessageId::DM_LOADCHAR_REQ:
        co_await OnDMLoadCharReq(std::move(body), ctx);
        break;

    case MessageId::MW_ENTERSVR_REQ:
        co_await OnMWEnterSvrReq(std::move(body), ctx);
        break;

    case MessageId::MW_ENTERCHAR_REQ:
        co_await OnMWEnterCharReq(std::move(body), ctx);
        break;

    case MessageId::MW_ADDCONNECT_REQ:
        co_await OnMWAddConnectReq(std::move(body), ctx);
        break;

    case MessageId::MW_CHECKMAIN_REQ:
        co_await OnMWCheckMainReq(std::move(body), ctx);
        break;

    case MessageId::MW_CONRESULT_REQ:
        co_await OnMWConResultReq(std::move(body), ctx);
        break;

    case MessageId::MW_CLOSECHAR_REQ:
        co_await OnMWCloseCharReq(std::move(body), ctx);
        break;

    default:
        spdlog::debug("world_client: unhandled wId=0x{:04X} body={} bytes",
            wId, body.size());
        break;
    }
}

} // namespace tmapsvr
