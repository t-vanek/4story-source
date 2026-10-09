#include "handlers_world.h"
#include "domain/connect.h"
#include "fourstory/db/co_offload.h"

#include "audit/audit_log.h"
#include "audit/event.h"
#include "services/channel_presence.h"
#include "services/char_state_store.h"
#include "services/client_senders.h"
#include "services/companion_service.h"
#include "services/inventory_service.h"
#include "services/player_service.h"
#include "services/quest_service.h"
#include "services/server_route_resolver.h"
#include "services/session_validator.h"
#include "services/session_registry.h"
#include "services/skill_service.h"
#include "services/skill_cooldown.h"
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

constexpr auto CnSuccess = static_cast<std::uint8_t>(ConnectResult::Ok);
constexpr auto CnInternal = static_cast<std::uint8_t>(ConnectResult::Internal);
constexpr auto CnNoChar = static_cast<std::uint8_t>(ConnectResult::InvalidChar);

// Retirement may arrive while a disconnected socket is awaiting its final DB
// write. Match the key without excluding Closing, so that late World instruction
// still suppresses the subsequent close-all notification.
std::shared_ptr<tnetlib::AsioSession>
FindRetiringClient(const HandlerContext& ctx, std::uint32_t character, std::uint32_t key)
{
    auto client = ctx.session_reg ? ctx.session_reg->Find(character) : nullptr;
    const auto identity = client ? ctx.session_reg->Identity(client.get()) : std::nullopt;
    return identity && identity->key == key ? client : nullptr;
}


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
        ev.key       = 0; // reserved wire slot; never export session credentials
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

    const auto bound = ctx.session_reg ? ctx.session_reg->Find(dwCharID, dwKEY) : nullptr;
    if (!bound) co_return;
    const auto identity = ctx.session_reg->Identity(bound.get());
    if (!identity || identity->user_id != dwUserID || !r.Eof()) co_return;

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

    wire::Reader r(body.data(), body.size());
    std::uint8_t dbload{};
    std::uint32_t character{}, key{};
    if (!r.Read(dbload) || !r.Read(character) || !r.Read(key)) co_return;
    const auto session = ctx.session_reg ? ctx.session_reg->Find(character, key) : nullptr;
    if (!session || !ctx.world_client || !ctx.world_client->IsConnected()) co_return;
    const auto identity = ctx.session_reg->Identity(session.get());
    if(dbload==0 && identity && identity->role==MapSessionRole::Replica) {
        co_await OnMWTransferEnterReq(std::move(body),ctx);co_return;
    }
    if (!identity || !ctx.session_reg->Transition(character, key,
        SessionPhase::Pending, SessionPhase::Loading)) co_return;

    std::optional<CharSnapshot> snap;
    std::uint8_t result = CnInternal;
    // Embedded handoff blobs need a complete, separately verified parser. They
    // must not silently reload stale persisted data and discard unsaved state.
    if (dbload == 1 && r.Eof() && ctx.player_service && ctx.char_state) {
        try {
            auto* players = ctx.player_service;
            snap = co_await fourstory::db::CoOffloadIf(ctx.db_pool,
                [players, claim=identity->Claim(ctx.expected_group)] { return players->LoadAuthorized(claim); });
            result = snap && snap->dwCharID == character ? CnSuccess : CnNoChar;
        } catch (...) { result = CnInternal; }
    }
    if (!session->IsOpen() || !ctx.session_reg->Find(character, key)) co_return;
    if(result==CnSuccess && snap->payload && ctx.skill_cooldown) {
        try {ctx.skill_cooldown->Restore(character,snap->payload->skills,SkillClockMs());}
        catch (...) {result=CnInternal;}
    }
    if (result == CnSuccess) {
        ctx.char_state->Store(character, *snap);
        if (!ctx.session_reg->Transition(character, key, SessionPhase::Loading, SessionPhase::Loaded)) co_return;
    }
    CharSnapshot empty{}; empty.dwCharID = character;
    const bool sent = co_await ctx.world_client->SendPacket(
        static_cast<std::uint16_t>(MessageId::MW_ENTERSVR_ACK),
        EncodeEnterSvrAck(result == CnSuccess ? *snap : empty, key,
            snap&&snap->payload?snap->payload->aid_country:3, identity->channel, /*logout=*/0, /*save=*/0,
            result, snap&&snap->payload?snap->payload->selected_title:0,
            snap&&snap->payload?snap->payload->rank_point:0, /*user_ip=*/0));
    if (!sent) session->Close();
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

    const auto bound = ctx.session_reg ? ctx.session_reg->Find(dwCharID, dwKEY) : nullptr;
    if (!bound) co_return;

    if (!ctx.world_client || !ctx.world_client->IsConnected())
    {
        spdlog::warn("MW_ENTERCHAR_REQ char={}: world peer not connected — "
                     "ack dropped", dwCharID);
        co_return;
    }

    const auto identity=ctx.session_reg->Identity(bound.get());
    if(identity&&identity->role==MapSessionRole::Replica){
        co_await OnMWNativeEnterCharReq(std::move(body),ctx);co_return;
    }
    if(ctx.char_state){auto native=ctx.char_state->Get(dwCharID);if(native&&native->payload){
        co_await OnMWNativeEnterCharReq(std::move(body),ctx);co_return;
    }}
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
    if (!have_hdr || !ctx.char_state || !ctx.char_state->Get(dwCharID)) co_return;
    if (ctx.char_state)
    {
        ctx.char_state->Update(dwCharID, [&](CharSnapshot& s) {
            s.wMapID = wMapID;
            s.fPosX  = fPosX;
            s.fPosY  = fPosY;
            s.fPosZ  = fPosZ;
        });
    }

    spdlog::info("MW_ENTERCHAR_REQ char={} name='{}' map={} — "
                 "connection ready, ack",
        dwCharID, have_hdr ? name : std::string("?"), wMapID);

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
    if(!r.Eof())co_return;

    // Relay to the client. The char must already be bound from its
    // CS_CONNECT_REQ; an unknown char means the socket dropped between
    // the world push and now — nothing to forward to (legacy FindPlayer
    // miss → silent return).
    auto sess = ctx.session_reg ? ctx.session_reg->Find(dwCharID, dwKEY) : nullptr;
    if (!sess)
    {
        spdlog::warn("MW_ADDCONNECT_REQ char={}: no bound client session — "
                     "drop ({} routes)", dwCharID, routes.size());
        co_return;
    }

    const auto identity=ctx.session_reg->Identity(sess.get());
    const auto native=ctx.char_state?ctx.char_state->Get(dwCharID):std::nullopt;
    if(identity&&native&&native->payload&&!routes.empty()) {
        std::vector<ServerRoute> endpoints;
        for(const auto& r:routes)endpoints.push_back({r.ip_addr,r.port,r.server_id});
        bool authorized=false;
        try{auto* validator=ctx.validator;
            if(validator)authorized=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
                [validator,claim=identity->Claim(ctx.expected_group),map=native->wMapID,x=native->fPosX,z=native->fPosZ,endpoints]{
                    return validator->AuthorizeReplicas(claim,map,x,z,endpoints);});
        }catch(...){sess->Close();co_return;}
        if(!authorized){sess->Close();co_return;}
        if(!sess->IsOpen())co_return;
    }

    spdlog::info("MW_ADDCONNECT_REQ char={} routes={} — relaying "
                 "CS_ADDCONNECT_ACK", dwCharID, routes.size());

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

    const auto bound = ctx.session_reg ? ctx.session_reg->Find(dwCharID, dwKEY) : nullptr;
    if (!bound) co_return;

    if (!ctx.world_client || !ctx.world_client->IsConnected())
    {
        spdlog::warn("MW_CHECKMAIN_REQ char={}: world peer not connected — "
                     "ack dropped", dwCharID);
        co_return;
    }

    bool resident=ctx.char_state&&ctx.char_state->Get(dwCharID).has_value();
    const auto identity=ctx.session_reg->Identity(bound.get());
    if(!r.Eof()||!identity||identity->channel!=bChannel)co_return;
    if(resident&&ctx.validator){
        try{auto* validator=ctx.validator;
            resident=co_await fourstory::db::CoOffloadIf(ctx.db_pool,[validator,claim=identity->Claim(ctx.expected_group),wMapID,fPosX,fPosZ]{
                return validator->OwnsCell(claim,wMapID,fPosX,fPosZ);});
        }catch(...){bound->Close();co_return;}
        if(!bound->IsOpen())co_return;
    }

    if (!resident)
    {
        spdlog::info("MW_CHECKMAIN_REQ char={} ch={} map={} — not "
                     "resident here, no ack", dwCharID, bChannel, wMapID);
        co_return;
    }

    spdlog::info("MW_CHECKMAIN_REQ char={} ch={} map={} — main cell, "
                 "ack", dwCharID, bChannel, wMapID);

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

    auto sess = ctx.session_reg ? ctx.session_reg->Find(dwCharID, dwKEY) : nullptr;
    if (!sess)
    {
        spdlog::warn("MW_CONRESULT_REQ char={}: no bound client session — drop",
            dwCharID);
        co_return;
    }

    if (!r.Eof() || bResult > CnInternal) co_return;
    const auto identity = ctx.session_reg->Identity(sess.get());
    if (!identity || identity->phase == SessionPhase::Admitted || identity->phase == SessionPhase::Ready) co_return;
    const bool world_rejected = bResult != CnSuccess;
    if (bResult == CnSuccess) {
        if (!ctx.char_state || !ctx.char_state->Get(dwCharID) ||
            identity->phase != SessionPhase::Loaded) bResult = CnInternal;
    }
    if (!ctx.session_reg->Transition(dwCharID, dwKEY, identity->phase,
        bResult == CnSuccess ? SessionPhase::Admitted : SessionPhase::Rejected)) co_return;
    if (world_rejected)
        ctx.session_reg->SetWorldPresence(sess.get(), WorldPresence::Retired);
    const auto ack = EncodeConnectAck(bResult, server_ids);
    co_await sess->SendPacket(
        static_cast<std::uint16_t>(MessageId::CS_CONNECT_ACK), ack, bResult != CnSuccess);

    if (bResult != CnSuccess)
    {
        spdlog::info("MW_CONRESULT_REQ char={} result={} — connect rejected, "
                     "closing session", dwCharID, bResult);
        // Send queue closes after the rejection frame reaches the socket.
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
    if (!r.Read(dwCharID) || !r.Read(dwKEY) || !r.Eof())
    {
        spdlog::warn("MW_CLOSECHAR_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    auto sess = FindRetiringClient(ctx, dwCharID, dwKEY);
    if (!sess)
    {
        spdlog::warn("MW_CLOSECHAR_REQ char={}: no bound client session — drop",
            dwCharID);
        co_return;
    }

    ctx.session_reg->SetWorldPresence(sess.get(), WorldPresence::Retired);

    // Legacy (SSHandler.cpp:2196): ExitMAP + m_bExit + SendCS_SHUTDOWN_ACK.
    // m_bCloseAll is FALSE on this path, so no MW_CLOSECHAR_ACK is sent
    // back (that confirmation belongs to the multi-connection close-all
    // path). Tell the client to shut down (empty-bodied ack), then close
    // the socket — the MapServer per-connection teardown hook persists
    // the snapshot (SaveChar) and unbinds the session / presence
    // registries (map_server.cpp:139).
    spdlog::info("MW_CLOSECHAR_REQ char={} — CS_SHUTDOWN_ACK + close",
        dwCharID);

    co_await sess->SendPacket(
        static_cast<std::uint16_t>(MessageId::CS_SHUTDOWN_ACK),
        std::span<const std::byte>{}, true);
}

boost::asio::awaitable<void>
OnMWRouteListReq(std::vector<std::byte> body, const HandlerContext& ctx)
{
    using tnetlib::protocol::MessageId;

    // Wire (legacy SSHandler.cpp:6485):
    //   DWORD dwCharID, DWORD dwKEY, BYTE bCount, bCount × BYTE server_id
    // TWorld asks the map to resolve a set of cluster server ids to their
    // live endpoints. The map answers MW_ROUTE_ACK with the (ip, port,
    // server_id) tuples — legacy fanned this through the DB-batch
    // (DM_ROUTE_REQ → CSPRoute → DM_ROUTE_ACK → MW_ROUTE_ACK); the modern
    // map resolves in-process via IServerRouteResolver.
    wire::Reader r(body.data(), body.size());
    std::uint32_t dwCharID = 0, dwKEY = 0;
    std::uint8_t  bCount = 0;
    if (!r.Read(dwCharID) || !r.Read(dwKEY) || !r.Read(bCount))
    {
        spdlog::warn("MW_ROUTELIST_REQ: short body ({} bytes) — dropping",
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
            spdlog::warn("MW_ROUTELIST_REQ char={}: truncated id list at {}/{} "
                         "— dropping", dwCharID, i, bCount);
            co_return;
        }
        server_ids.push_back(sid);
    }

    const auto bound = ctx.session_reg ? ctx.session_reg->Find(dwCharID, dwKEY) : nullptr;
    if (!bound) co_return;

    if (!ctx.world_client || !ctx.world_client->IsConnected())
    {
        spdlog::warn("MW_ROUTELIST_REQ char={}: world peer not connected — "
                     "ack dropped", dwCharID);
        co_return;
    }

    // Resolve via the injected resolver. Without one configured (no SOCI
    // route resolver wired yet) the map answers an empty route list so
    // TWorld's OnRouteAck still unwinds — the production resolver against
    // the cluster server-registry table is the documented follow-up.
    std::vector<ServerRoute> routes;
    if (ctx.route_resolver)
    {
        const auto identity=ctx.session_reg->Identity(bound.get());if(!identity||!r.Eof())co_return;
        try{auto* resolver=ctx.route_resolver;
            routes=co_await fourstory::db::CoOffloadIf(ctx.db_pool,[resolver,claim=identity->Claim(ctx.expected_group),server_ids]{return resolver->ResolveAuthorized(claim,server_ids);});
        }catch(...){bound->Close();co_return;}
        if(!bound->IsOpen())co_return;
    }
    else
        spdlog::warn("MW_ROUTELIST_REQ char={}: no route resolver — replying "
                     "empty MW_ROUTE_ACK ({} ids unresolved)",
            dwCharID, server_ids.size());

    spdlog::info("MW_ROUTELIST_REQ char={} ids={} resolved={} — "
                 "MW_ROUTE_ACK", dwCharID, server_ids.size(),
                 routes.size());

    co_await ctx.world_client->SendPacket(
        static_cast<std::uint16_t>(MessageId::MW_ROUTE_ACK),
        EncodeRouteAck(dwCharID, dwKEY, routes));
}

namespace {
boost::asio::awaitable<void>
RetireFromWorld(std::vector<std::byte> body, const HandlerContext& ctx, bool invalid)
{
    using tnetlib::protocol::MessageId;
    wire::Reader r(body.data(), body.size());
    std::uint32_t character{}, key{};
    std::uint8_t first{}, save{};
    // INVALIDCHAR: char, key, release-main. DELCHAR: char, key, logout, save.
    if (!r.Read(character) || !r.Read(key) || !r.Read(first) || first > 1 ||
        (!invalid && (!r.Read(save) || save > 1)) || !r.Eof()) co_return;
    auto client = FindRetiringClient(ctx, character, key);
    if (!client) co_return;
    ctx.session_reg->SetWorldPresence(client.get(), WorldPresence::Retired);
    // Durable primary ownership, not a peer's save byte, decides whether native
    // teardown must checkpoint/release. Never discard a dirty ready primary.
    // Replicas need their own ownership/release path before native admission.
    if (invalid)
        co_await client->SendPacket(static_cast<std::uint16_t>(MessageId::CS_INVALIDCHAR_ACK),
            std::span<const std::byte>{}, true);
    else client->Close();
}
} // namespace

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
        // The original DM_* load request belongs to the local DB queue, not
        // the World socket. Do not expose the incomplete legacy load encoder.
        spdlog::warn("world link: rejected local-only DM_LOADCHAR_REQ");
        break;

    case MessageId::MW_CHARINFO_REQ:
        co_await OnMWCharInfoReq(std::move(body),ctx);break;
    case MessageId::MW_ROUTE_REQ:
        co_await OnMWNativeRouteReq(std::move(body),ctx,false);break;
    case MessageId::MW_MAPSVRLIST_REQ:
        co_await OnMWNativeRouteReq(std::move(body),ctx,true);break;
    case MessageId::MW_CHARDATA_REQ:
        co_await OnMWCharDataReq(std::move(body),ctx);break;
    case MessageId::MW_RELEASEMAIN_REQ:
        co_await OnMWReleaseMainReq(std::move(body),ctx);break;
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

    case MessageId::MW_INVALIDCHAR_REQ:
        co_await RetireFromWorld(std::move(body), ctx, true);
        break;
    case MessageId::MW_DELCHAR_REQ:
        co_await RetireFromWorld(std::move(body), ctx, false);
        break;
    case MessageId::MW_CLOSECHAR_REQ:
        co_await OnMWCloseCharReq(std::move(body), ctx);
        break;

    case MessageId::MW_ROUTELIST_REQ:
        co_await OnMWRouteListReq(std::move(body), ctx);
        break;

    default:
        spdlog::debug("world_client: unhandled wId=0x{:04X} body={} bytes",
            wId, body.size());
        break;
    }
}

} // namespace tmapsvr
