// Map-state handlers — AOI registration, movement, entry/leave
// broadcasts. Implements the F3 slice of the legacy CTMap /
// CTCell system.
//
// Legacy references:
//   CSHandler.cpp:402  — OnCS_CONREADY_REQ  (InitMap path)
//   CSHandler.cpp:439  — OnCS_MOVE_REQ
//   CSSender.cpp:395   — SendCS_ENTER_ACK
//   CSSender.cpp:599   — SendCS_MOVE_ACK
//   TMap.cpp           — EnterMAP, LeaveMAP, OnMove
//   TCell.cpp          — EnterPlayer (AOI broadcast loop)

#include "handlers.h"
#include "handlers_map.h"
#include "handlers_combat.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <cstring>

namespace tmapsvr {

using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

// Build a PlayerPresence from the session's snapshot + movement state.
// Called on EnterMap to populate map_state's presence store.
legacy::PlayerPresence MakePresenceFromState(const MapSessionState& state)
{
    legacy::PlayerPresence p{};
    p.char_id = state.char_id;
    if (!state.snapshot) return p;

    const auto& snap = *state.snapshot;
    p.name        = snap.name;
    p.level       = snap.level;
    p.hp          = snap.hp;
    p.max_hp      = 0;   // TLEVELCHART — F4
    p.mp          = snap.mp;
    p.max_mp      = 0;   // F4

    p.race        = snap.appearance.race;
    p.sex         = snap.appearance.sex;
    p.char_class  = snap.appearance.char_class;
    p.hair        = snap.appearance.hair;
    p.face        = snap.appearance.face;
    p.body        = snap.appearance.body;
    p.pants       = snap.appearance.pants;
    p.hand        = snap.appearance.hand;
    p.foot        = snap.appearance.foot;
    p.helmet_hide = snap.appearance.helmet_hide;
    p.country     = snap.appearance.country;

    p.pos_x = snap.position.pos_x;
    p.pos_y = snap.position.pos_y;
    p.pos_z = snap.position.pos_z;
    p.dir   = snap.position.dir;
    return p;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// CS_ENTER_ACK
// ---------------------------------------------------------------------------
//
// Wire order: CSSender.cpp:395 — SendCS_ENTER_ACK
// F3 stubs (safe zero defaults):
//   * guild_id, guild_name, tactics → 0 / "" (F3b guild system)
//   * party_chief, party_id, commander → 0 (F3b party)
//   * maintained skills → count 0 (F4)
//   * equipment → count 0 (F5)
//   * max_hp, max_mp → 0 (TLEVELCHART, F4)

boost::asio::awaitable<void>
SendEnterAck(std::shared_ptr<tnetlib::AsioSession>  sess,
             const legacy::PlayerPresence&           p,
             bool                                    new_member)
{
    std::vector<std::byte> body;
    body.reserve(128);

    wire::WritePOD<std::uint32_t>(body, p.char_id);     // m_dwID
    wire::WriteString(body, p.name);                     // m_strNAME
    wire::WritePOD<std::uint16_t>(body, 0);              // m_wTitleID

    // Guild comment — send empty (NAME_NULL equivalent)
    wire::WriteString(body, std::string{});

    wire::WritePOD<std::uint32_t>(body, 0);  // m_dwGuildID
    wire::WritePOD<std::uint32_t>(body, 0);  // m_dwFame
    wire::WritePOD<std::uint32_t>(body, 0);  // m_dwFameColor
    wire::WriteString(body, std::string{});  // m_strGuildName
    wire::WritePOD<std::uint8_t> (body, 0);  // m_bGuildPeer
    wire::WritePOD<std::uint32_t>(body, 0);  // m_dwTacticsID
    wire::WriteString(body, std::string{});  // m_strTacticsName
    wire::WritePOD<std::uint8_t> (body, 0);  // m_bStore (no store open)
    wire::WritePOD<std::uint32_t>(body, 0);  // m_dwRiding (no mount)

    // Appearance
    wire::WritePOD<std::uint8_t>(body, p.char_class);
    wire::WritePOD<std::uint8_t>(body, p.race);
    wire::WritePOD<std::uint8_t>(body, p.country);
    wire::WritePOD<std::uint8_t>(body, 0);           // m_bAidCountry
    wire::WritePOD<std::uint8_t>(body, p.sex);
    wire::WritePOD<std::uint8_t>(body, p.hair);
    wire::WritePOD<std::uint8_t>(body, p.face);
    wire::WritePOD<std::uint8_t>(body, p.body);
    wire::WritePOD<std::uint8_t>(body, p.pants);
    wire::WritePOD<std::uint8_t>(body, p.hand);
    wire::WritePOD<std::uint8_t>(body, p.foot);
    wire::WritePOD<std::uint8_t>(body, p.level);
    wire::WritePOD<std::uint8_t>(body, p.helmet_hide);

    // Vitals (max values from TLEVELCHART — F4; send 0 for now)
    wire::WritePOD<std::uint32_t>(body, p.max_hp);
    wire::WritePOD<std::uint32_t>(body, p.hp);
    wire::WritePOD<std::uint32_t>(body, p.max_mp);
    wire::WritePOD<std::uint32_t>(body, p.mp);

    // Party
    wire::WritePOD<std::uint32_t>(body, 0);  // GetPartyChiefID()
    wire::WritePOD<std::uint32_t>(body, 0);  // GetPartyID()
    wire::WritePOD<std::uint32_t>(body, 0);  // GetCommanderID()

    // Position + orientation
    wire::WritePOD<float>        (body, p.pos_x);
    wire::WritePOD<float>        (body, p.pos_y);
    wire::WritePOD<float>        (body, p.pos_z);
    wire::WritePOD<std::uint8_t> (body, p.action);
    wire::WritePOD<std::uint8_t> (body, 0);         // m_bBlock
    wire::WritePOD<std::uint8_t> (body, p.mode);
    wire::WritePOD<std::uint16_t>(body, p.pitch);
    wire::WritePOD<std::uint16_t>(body, p.dir);
    wire::WritePOD<std::uint8_t> (body, p.mouse_dir);
    wire::WritePOD<std::uint8_t> (body, p.key_dir);

    // Faction colour (0 = TNCOLOR_ALLI / same faction; 1 = enemy)
    wire::WritePOD<std::uint8_t>(body, 0);

    // Misc
    wire::WritePOD<std::uint32_t>(body, 0);  // m_dwRegion (F3b)
    wire::WritePOD<std::uint8_t> (body, 0);  // m_bInPcBang
    wire::WritePOD<std::uint8_t> (body, 0);  // m_bAftermath.m_bStep
    wire::WritePOD<std::uint32_t>(body, 0);  // m_dwRankPoint
    wire::WritePOD<std::uint16_t>(body, 0);  // castle_id
    wire::WritePOD<std::uint8_t> (body, 0);  // camp
    wire::WritePOD<std::uint16_t>(body, 0);  // godball_id

    // Maintained skills — F4 (count = 0)
    wire::WritePOD<std::uint8_t>(body, 0);

    // Equipment — F5 (count = 0)
    wire::WritePOD<std::uint8_t>(body, 0);

    // bNewMember flag
    wire::WritePOD<std::uint8_t>(body, new_member ? 1u : 0u);

    co_await sess->SendPacket(
        ToUint16(MessageId::CS_ENTER_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// ---------------------------------------------------------------------------
// CS_MOVE_ACK
// ---------------------------------------------------------------------------

boost::asio::awaitable<void>
SendMoveAck(std::shared_ptr<tnetlib::AsioSession> sess,
            std::uint32_t char_id,
            float pos_x, float pos_y, float pos_z,
            std::uint16_t pitch, std::uint16_t dir,
            std::uint8_t mouse_dir, std::uint8_t key_dir,
            std::uint8_t action, float speed)
{
    std::vector<std::byte> body;
    body.reserve(36);
    wire::WritePOD<std::uint32_t>(body, char_id);
    wire::WritePOD<float>        (body, pos_x);
    wire::WritePOD<float>        (body, pos_y);
    wire::WritePOD<float>        (body, pos_z);
    wire::WritePOD<std::uint16_t>(body, pitch);
    wire::WritePOD<std::uint16_t>(body, dir);
    wire::WritePOD<std::uint8_t> (body, mouse_dir);
    wire::WritePOD<std::uint8_t> (body, key_dir);
    wire::WritePOD<std::uint8_t> (body, action);
    wire::WritePOD<float>        (body, speed);
    co_await sess->SendPacket(
        ToUint16(MessageId::CS_MOVE_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// ---------------------------------------------------------------------------
// CS_LEAVE_ACK
// ---------------------------------------------------------------------------

boost::asio::awaitable<void>
SendLeaveAck(std::shared_ptr<tnetlib::AsioSession> sess,
             std::uint32_t char_id)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint32_t>(body, char_id);
    co_await sess->SendPacket(
        ToUint16(MessageId::CS_LEAVE_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// ---------------------------------------------------------------------------
// CS_CHARINFO_ACK
// ---------------------------------------------------------------------------
//
// Moved from handlers.cpp so both OnConReadyReq paths share one TU.
// Wire order: CSSender.cpp:121 — SendCS_CHARINFO_ACK

boost::asio::awaitable<void>
SendCharInfoAck(std::shared_ptr<tnetlib::AsioSession>  sess,
                const legacy::CharSnapshot&             snap)
{
    std::vector<std::byte> body;
    body.reserve(256);

    wire::WritePOD<std::uint32_t>(body, snap.char_id);  // m_dwID
    wire::WritePOD<std::uint8_t> (body, 0);              // bSecureCreated
    wire::WritePOD<std::uint8_t> (body, 0);              // m_bSecureCurUnlocked
    wire::WritePOD<std::uint8_t> (body, 1);              // bSecureDisabled
    wire::WritePOD<std::uint16_t>(body, 0);              // m_wTitleID

    wire::WriteString(body, snap.name);
    wire::WritePOD<std::uint8_t>(body, snap.appearance.start_act);
    wire::WritePOD<std::uint8_t>(body, snap.appearance.char_class);
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
    wire::WritePOD<std::uint8_t>(body, snap.level);

    wire::WritePOD<std::uint16_t>(body, 0);  // PartyID
    wire::WritePOD<std::uint32_t>(body, 0);  // m_dwGuildID
    wire::WritePOD<std::uint32_t>(body, 0);  // m_dwFame
    wire::WritePOD<std::uint32_t>(body, 0);  // m_dwFameColor
    wire::WritePOD<std::uint8_t> (body, 0);  // m_bGuildDuty
    wire::WritePOD<std::uint8_t> (body, 0);  // m_bGuildPeer
    wire::WriteString(body, std::string{});   // m_strGuildName
    wire::WritePOD<std::uint32_t>(body, 0);  // m_dwTacticsID
    wire::WriteString(body, std::string{});   // m_strTacticsName

    wire::WritePOD<std::uint32_t>(body, snap.gold);
    wire::WritePOD<std::uint32_t>(body, snap.silver);
    wire::WritePOD<std::uint32_t>(body, snap.copper);

    wire::WritePOD<std::uint32_t>(body, 0);          // dwPrevExp (F4 level chart)
    wire::WritePOD<std::uint32_t>(body, 0);          // dwNextExp
    wire::WritePOD<std::uint32_t>(body, snap.exp);
    wire::WritePOD<std::uint32_t>(body, 0);          // GetMaxHP() (F4)
    wire::WritePOD<std::uint32_t>(body, snap.hp);
    wire::WritePOD<std::uint32_t>(body, 0);          // GetMaxMP() (F4)
    wire::WritePOD<std::uint32_t>(body, snap.mp);

    wire::WritePOD<std::uint32_t>(body, 0);  // GetPartyChiefID()
    wire::WritePOD<std::uint32_t>(body, 0);  // GetCommanderID()

    wire::WritePOD<std::uint32_t>(body, snap.position.region);
    wire::WritePOD<std::uint16_t>(body, snap.position.map_id);
    wire::WritePOD<float>        (body, snap.position.pos_x);
    wire::WritePOD<float>        (body, snap.position.pos_y);
    wire::WritePOD<float>        (body, snap.position.pos_z);
    wire::WritePOD<std::uint16_t>(body, snap.position.dir);
    wire::WritePOD<std::uint16_t>(body, snap.skill_point);
    wire::WritePOD<std::uint8_t> (body, 0);  // m_bLuckyNumber
    wire::WritePOD<std::int64_t> (body, 0);  // GetAidLeftTime()

    for (int i = 0; i < 4; ++i)
        wire::WritePOD<std::uint16_t>(body, 0);  // arPoint[0..3]

    wire::WritePOD<std::uint32_t>(body, 0);  // m_dwRankPoint
    wire::WritePOD<std::uint8_t> (body, 0);  // BOW compile mode flag

    // Inventory snapshot
    const auto inven_count =
        static_cast<std::uint8_t>(snap.inventory.size());
    wire::WritePOD<std::uint8_t>(body, inven_count);
    for (const auto& slot : snap.inventory)
    {
        wire::WritePOD<std::uint8_t> (body, slot.inven_id);
        wire::WritePOD<std::uint16_t>(body, slot.item_id);
        wire::WritePOD<std::int64_t> (body, slot.end_time);
    }

    wire::WritePOD<std::uint8_t>(body, 0);  // item count (F5)
    wire::WritePOD<std::uint8_t>(body, 0);  // skill count (F4)
    wire::WritePOD<std::uint8_t>(body, 0);  // maintain skill count (F4)

    co_await sess->SendPacket(
        ToUint16(MessageId::CS_CHARINFO_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// ---------------------------------------------------------------------------
// OnConReadyReq — F3 §2 full InitMap
// ---------------------------------------------------------------------------
//
// §1 CSHandler.cpp:406 — pre-connect stray → silent drop
// §2 CSHandler.cpp:408 — first-time map entry → InitMap:
//     a) standalone/cluster with snapshot: register in map_state,
//        exchange CS_ENTER_ACK with all AOI neighbours, send CS_CHARINFO_ACK
// §3/§4 CSHandler.cpp:410-413 — re-enter / sub-map → PENDING F3b

boost::asio::awaitable<void>
OnConReadyReq(std::shared_ptr<tnetlib::AsioSession> sess,
              MapSessionState&                     state,
              const tnetlib::DecodedPacket&        /*packet*/,
              const HandlerContext&                ctx)
{
    // §1: stray CONREADY or snapshot not ready
    if (!state.connected || !state.snapshot.has_value())
    {
        spdlog::debug("CS_CONREADY_REQ uid={} — no snapshot (§1 drop)",
            state.user_id);
        co_return;
    }

    // §2 F3: register in session registry + AOI map
    if (ctx.session_registry)
        ctx.session_registry->Register(state.char_id, sess);

    // F4 Part 3: register player vitals for server-side monster damage
    if (ctx.player_hp && state.snapshot)
    {
        const auto& snap = *state.snapshot;
        ctx.player_hp->Register(state.char_id,
            snap.hp, snap.hp,  // max_hp placeholder until TLEVELCHART (F4b)
            snap.mp, snap.mp);
    }

    if (ctx.map_state)
    {
        const auto presence = MakePresenceFromState(state);

        // EnterMap returns existing neighbours
        auto aoi = ctx.map_state->EnterMap(state.char_id, presence);
        ctx.map_state->UpdatePresence(state.char_id, presence);

        state.in_world = true;
        spdlog::info("CS_CONREADY_REQ uid={} char='{}' → map, "
                     "{} AOI neighbours",
            state.user_id, state.snapshot->name, aoi.size());

        // Send each existing neighbour's appearance to the entering player
        for (std::uint32_t nid : aoi)
        {
            if (const auto* np = ctx.map_state->GetPresence(nid))
                co_await SendEnterAck(sess, *np, false);
        }

        // Send the entering player's appearance to each existing neighbour
        for (std::uint32_t nid : aoi)
        {
            auto nbr = ctx.session_registry
                       ? ctx.session_registry->Get(nid)
                       : std::shared_ptr<tnetlib::AsioSession>{};
            if (nbr)
                co_await SendEnterAck(nbr, presence, true);
        }
    }
    else
    {
        spdlog::info("CS_CONREADY_REQ uid={} char='{}' → CS_CHARINFO_ACK "
                     "(no IMapState wired)",
            state.user_id, state.snapshot->name);
    }

    // F4: send CS_ADDMON_ACK for each monster in AOI
    if (ctx.monster_registry)
    {
        const auto mon_ids = ctx.monster_registry->GetNeighborIds(
            state.snapshot->position.pos_x,
            state.snapshot->position.pos_z);
        for (std::uint32_t mid : mon_ids)
        {
            if (const auto* mon = ctx.monster_registry->Get(mid))
                co_await SendAddMonAck(sess, *mon, false);
        }
    }

    co_await SendCharInfoAck(sess, *state.snapshot);
}

// ---------------------------------------------------------------------------
// OnMoveReq — CS_MOVE_REQ
// ---------------------------------------------------------------------------
//
// Wire body: DWORD wMapID, FLOAT fPosX/Y/Z, WORD wPitch, WORD wDIR,
//            BYTE bMouseDIR, BYTE bKeyDIR, BYTE bAction, BYTE bGhost,
//            FLOAT fSpeed
// Source: CSHandler.cpp:439-485

boost::asio::awaitable<void>
OnMoveReq(std::shared_ptr<tnetlib::AsioSession> sess,
          MapSessionState&                     state,
          const tnetlib::DecodedPacket&        packet,
          const HandlerContext&                ctx)
{
    if (!state.connected || !state.snapshot) co_return;

    wire::Reader r(packet.body.data(), packet.body.size());
    std::uint32_t map_id  = 0;
    float         px = 0, py = 0, pz = 0;
    std::uint16_t pitch = 0, dir = 0;
    std::uint8_t  mouse_dir = 0, key_dir = 0, action = 0, ghost = 0;
    float         speed = 0.0f;

    if (!r.Read(map_id) || !r.Read(px) || !r.Read(py) || !r.Read(pz) ||
        !r.Read(pitch)  || !r.Read(dir) ||
        !r.Read(mouse_dir) || !r.Read(key_dir) ||
        !r.Read(action) || !r.Read(ghost) || !r.Read(speed))
    {
        spdlog::warn("CS_MOVE_REQ malformed uid={}", state.user_id);
        co_return;
    }

    // Speed-hack detection — legacy cap 3.40f
    constexpr float MAX_SPEED = 3.40f;
    if (speed > MAX_SPEED) speed = MAX_SPEED;

    // Update position in snapshot (source of truth on session)
    state.snapshot->position.pos_x = px;
    state.snapshot->position.pos_y = py;
    state.snapshot->position.pos_z = pz;
    state.snapshot->position.dir   = dir;

    if (!ctx.map_state || !state.in_world) co_return;

    const auto delta = ctx.map_state->OnMove(state.char_id, px, pz);

    // Broadcast CS_MOVE_ACK to all chars that remain in AOI
    for (std::uint32_t nid : delta.common_aoi)
    {
        auto nbr = ctx.session_registry
                   ? ctx.session_registry->Get(nid)
                   : std::shared_ptr<tnetlib::AsioSession>{};
        if (nbr)
            co_await SendMoveAck(nbr, state.char_id,
                px, py, pz, pitch, dir, mouse_dir, key_dir, action, speed);
    }

    // New AOI entrants — mutual CS_ENTER_ACK exchange
    for (std::uint32_t nid : delta.entered_aoi)
    {
        auto nbr = ctx.session_registry
                   ? ctx.session_registry->Get(nid)
                   : std::shared_ptr<tnetlib::AsioSession>{};
        if (nbr)
        {
            if (const auto* our_p = ctx.map_state->GetPresence(state.char_id))
                co_await SendEnterAck(nbr, *our_p, false);
            if (const auto* their_p = ctx.map_state->GetPresence(nid))
                co_await SendEnterAck(sess, *their_p, false);
        }
    }

    // AOI departures — mutual CS_LEAVE_ACK
    for (std::uint32_t nid : delta.left_aoi)
    {
        auto nbr = ctx.session_registry
                   ? ctx.session_registry->Get(nid)
                   : std::shared_ptr<tnetlib::AsioSession>{};
        if (nbr)
            co_await SendLeaveAck(nbr, state.char_id);
        co_await SendLeaveAck(sess, nid);
    }

    // Update stored presence with new position + movement state
    if (const auto* cur = ctx.map_state->GetPresence(state.char_id))
    {
        auto updated      = *cur;
        updated.pos_x     = px;
        updated.pos_y     = py;
        updated.pos_z     = pz;
        updated.dir       = dir;
        updated.pitch     = pitch;
        updated.action    = action;
        updated.mouse_dir = mouse_dir;
        updated.key_dir   = key_dir;
        updated.speed     = speed;
        ctx.map_state->UpdatePresence(state.char_id, std::move(updated));
    }
}

} // namespace tmapsvr
