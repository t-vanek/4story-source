// Party handlers — F6 Part 2.
//
// Standalone mode: LocalPartyService manages party state in-memory.
// Cluster mode: party requests would be forwarded to TWorldSvr via
//   MW_PARTYADD_ACK etc. — PENDING F8 (server-server handlers).
//
// Wire references:
//   CS_PARTYADD_REQ      — CSHandler.cpp:3419
//   CS_PARTYJOIN_REQ     — CSHandler.cpp:3451
//   CS_PARTYJOINASK_ACK  — CSSender.cpp:1718
//   CS_PARTYJOIN_ACK     — CSSender.cpp:1729

#include "handlers.h"
#include "party_service.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>
#include <algorithm>

namespace tmapsvr {

using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

// CS_PARTYJOINASK_ACK — ask target player to join a party.
// Wire: CString strRequestID (inviter name), BYTE bObtainType
// Source: CSSender.cpp:1718
boost::asio::awaitable<void>
SendPartyJoinAskAck(std::shared_ptr<tnetlib::AsioSession> sess,
                    const std::string& inviter_name,
                    std::uint8_t       obtain_type)
{
    std::vector<std::byte> body;
    wire::WriteString(body, inviter_name);
    wire::WritePOD<std::uint8_t>(body, obtain_type);
    co_await sess->SendPacket(
        ToUint16(MessageId::CS_PARTYJOINASK_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// CS_PARTYJOIN_ACK — broadcast new member to all party members.
// Wire: WORD wPartyID, CString member_name, DWORD member_id,
//       DWORD chief_id, WORD commander_id, CString guild_name,
//       BYTE level, DWORD max_hp, DWORD hp, DWORD max_mp, DWORD mp,
//       BYTE race, BYTE sex, BYTE face, BYTE hair, BYTE obtain_type, BYTE class
// Source: CSSender.cpp:1729
boost::asio::awaitable<void>
SendPartyJoinAck(std::shared_ptr<tnetlib::AsioSession> sess,
                 std::uint16_t               party_id,
                 const PartyMemberInfo&       new_member,
                 std::uint32_t               chief_id,
                 std::uint8_t                obtain_type)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint16_t>(body, party_id);
    wire::WriteString(body, new_member.name);
    wire::WritePOD<std::uint32_t>(body, new_member.char_id);
    wire::WritePOD<std::uint32_t>(body, chief_id);
    wire::WritePOD<std::uint16_t>(body, 0u);         // commander_id (F6b)
    wire::WriteString(body, std::string{});           // guild_name (F6b)
    wire::WritePOD<std::uint8_t> (body, new_member.level);
    wire::WritePOD<std::uint32_t>(body, new_member.max_hp);
    wire::WritePOD<std::uint32_t>(body, new_member.hp);
    wire::WritePOD<std::uint32_t>(body, new_member.max_mp);
    wire::WritePOD<std::uint32_t>(body, new_member.mp);
    wire::WritePOD<std::uint8_t> (body, new_member.race);
    wire::WritePOD<std::uint8_t> (body, new_member.sex);
    wire::WritePOD<std::uint8_t> (body, new_member.face);
    wire::WritePOD<std::uint8_t> (body, new_member.hair);
    wire::WritePOD<std::uint8_t> (body, obtain_type);
    wire::WritePOD<std::uint8_t> (body, new_member.char_class);
    co_await sess->SendPacket(
        ToUint16(MessageId::CS_PARTYJOIN_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// Build PartyMemberInfo from session state
PartyMemberInfo MakeMemberInfo(const MapSessionState& state)
{
    PartyMemberInfo m{};
    m.char_id = state.char_id;
    if (state.snapshot)
    {
        m.name       = state.snapshot->name;
        m.level      = state.snapshot->level;
        m.hp         = state.snapshot->hp;
        m.max_hp     = state.snapshot->hp;  // placeholder until F4b max_hp
        m.mp         = state.snapshot->mp;
        m.max_mp     = state.snapshot->mp;
        m.race       = state.snapshot->appearance.race;
        m.sex        = state.snapshot->appearance.sex;
        m.face       = state.snapshot->appearance.face;
        m.hair       = state.snapshot->appearance.hair;
        m.char_class = state.snapshot->appearance.char_class;
        m.country    = state.snapshot->appearance.country;
    }
    return m;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// CS_PARTYADD_REQ — invite a player to party
// ---------------------------------------------------------------------------
//
// Wire body: CString strTarget, BYTE bObtainType
// Source: CSHandler.cpp:3419

boost::asio::awaitable<void>
OnPartyAddReq(std::shared_ptr<tnetlib::AsioSession> sess,
              MapSessionState&                     state,
              const tnetlib::DecodedPacket&        packet,
              const HandlerContext&                ctx)
{
    if (!state.connected || !state.snapshot) co_return;

    wire::Reader r(packet.body.data(), packet.body.size());
    std::string  target_name;
    std::uint8_t obtain_type = 0;
    if (!r.ReadString(target_name) || !r.Read(obtain_type))
    {
        spdlog::warn("CS_PARTYADD_REQ malformed uid={}", state.user_id);
        co_return;
    }

    spdlog::info("CS_PARTYADD_REQ uid={} '{}' → target='{}' type={}",
        state.user_id, state.snapshot->name, target_name, obtain_type);

    if (!ctx.party_svc || !ctx.session_registry) co_return;

    // Store pending invite keyed by target name
    PartyInvite invite{};
    invite.inviter_id  = state.char_id;
    invite.obtain_type = obtain_type;
    // party_id=0 means "create when they accept"
    ctx.party_svc->StorePendingInvite(target_name, invite);

    // Find target's session by scanning all AOI neighbours
    // (simplified — full by-name lookup requires global player registry)
    if (ctx.map_state && state.snapshot)
    {
        const auto aoi = ctx.map_state->GetNeighborIds(
            state.snapshot->position.pos_x,
            state.snapshot->position.pos_z);
        for (std::uint32_t pid : aoi)
        {
            // We don't have char_id→name mapping here.
            // Check if session registry can find the target by checking
            // each AOI session's name (would need IPlayerNameRegistry F6b).
            // For now: send ask to all AOI players named target_name.
            // This is a stub — real lookup uses m_mapPLAYER by name.
            auto nbr = ctx.session_registry->Get(pid);
            if (nbr)
                co_await SendPartyJoinAskAck(nbr,
                    state.snapshot->name, obtain_type);
        }
    }
}

// ---------------------------------------------------------------------------
// CS_PARTYJOIN_REQ — accept party invite
// ---------------------------------------------------------------------------
//
// Wire body: CString strOrigin (inviter name), BYTE bObtainType
// Source: CSHandler.cpp:3451

boost::asio::awaitable<void>
OnPartyJoinReq(std::shared_ptr<tnetlib::AsioSession> sess,
               MapSessionState&                     state,
               const tnetlib::DecodedPacket&        packet,
               const HandlerContext&                ctx)
{
    if (!state.connected || !state.snapshot) co_return;

    wire::Reader r(packet.body.data(), packet.body.size());
    std::string  origin_name;
    std::uint8_t obtain_type = 0;
    if (!r.ReadString(origin_name) || !r.Read(obtain_type))
    {
        spdlog::warn("CS_PARTYJOIN_REQ malformed uid={}", state.user_id);
        co_return;
    }

    if (!ctx.party_svc) co_return;

    // Take the pending invite for this player's name
    auto invite = ctx.party_svc->TakePendingInvite(state.snapshot->name);
    if (!invite)
    {
        spdlog::debug("CS_PARTYJOIN_REQ: no pending invite for '{}'",
            state.snapshot->name);
        co_return;
    }

    const auto new_member = MakeMemberInfo(state);

    // Create or join party
    std::uint16_t party_id = invite->party_id;
    std::uint32_t chief_id = invite->inviter_id;

    if (party_id == 0)
    {
        // First acceptance — create party with inviter as leader
        // Inviter's PartyMemberInfo not available here (F6b: needs player registry)
        PartyMemberInfo leader{};
        leader.char_id = invite->inviter_id;
        party_id = ctx.party_svc->CreateParty(leader, obtain_type);
        chief_id = invite->inviter_id;
    }
    ctx.party_svc->AddMember(party_id, new_member);

    spdlog::info("CS_PARTYJOIN_REQ: uid={} '{}' joined party_id={} "
                 "(inviter uid={})",
        state.user_id, state.snapshot->name,
        party_id, invite->inviter_id);

    // Broadcast CS_PARTYJOIN_ACK to all AOI (simplified — real broadcast
    // goes to all party members regardless of location, F6b)
    if (ctx.session_registry && ctx.map_state && state.snapshot)
    {
        const auto aoi = ctx.map_state->GetNeighborIds(
            state.snapshot->position.pos_x,
            state.snapshot->position.pos_z);
        for (std::uint32_t pid : aoi)
        {
            auto nbr = ctx.session_registry->Get(pid);
            if (nbr)
                co_await SendPartyJoinAck(nbr, party_id,
                    new_member, chief_id, obtain_type);
        }
    }
    // Also send to the joiner themselves
    co_await SendPartyJoinAck(sess, party_id,
        new_member, chief_id, obtain_type);
}

} // namespace tmapsvr
