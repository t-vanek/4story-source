// W3a-4 wire test: MW_GUILDLEAVE_ACK + the surrounding
// TChar.guild_id ↔ TGuild.members invariant.
//
// Scenarios:
//   1. Load a guild via DM_GUILDLOAD_ACK → TChar.guild_id is set
//      on the founder; the guild has one member (the founder).
//   2. Drive MW_GUILDLEAVE_ACK on the founder → guild's members
//      list is empty, TChar.guild_id is cleared, the test reads
//      back the MW_GUILDLEAVE_REQ reply with the expected fields.
//   3. Drive MW_GUILDLEAVE_ACK a second time → handler treats it
//      as a benign no-op (no crash, no extra reply).
//   4. OnEnterCharReq after the leave → ENTERCHAR_ACK carries
//      guild_id=0 (W3a-3 returned stale data before this fix).
//
// Uses two peer sockets so we can verify the leave reply lands
// on the originating socket, not on an arbitrary registered peer.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/fake_guild_repository.h"
#include "../services/guild_constants.h"
#include "../services/guild_registry.h"
#include "../services/guild_wanted_registry.h"
#include "../services/guild_tactics_wanted_registry.h"
#include "../services/peer_registry.h"
#include "../wire_codec.h"
#include "../world_server.h"
#include "../world_session.h"

#include "MessageId.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

void SendFramed(boost::asio::ip::tcp::socket& sock,
                std::uint16_t wId,
                const std::vector<std::byte>& body)
{
    tworldsvr::PacketHeader hdr{};
    hdr.wSize    = static_cast<std::uint16_t>(
        tworldsvr::kPacketHeaderSize + body.size());
    hdr.wID      = wId;
    hdr.dwChkSum = tworldsvr::ComputeChecksum(body.data(), body.size());
    std::vector<std::byte> buf(hdr.wSize);
    std::memcpy(buf.data(), &hdr, sizeof(hdr));
    if (!body.empty())
        std::memcpy(buf.data() + sizeof(hdr), body.data(), body.size());
    boost::asio::write(sock, boost::asio::buffer(buf.data(), buf.size()));
}

std::pair<std::uint16_t, std::vector<std::byte>>
ReadFramed(boost::asio::ip::tcp::socket& sock)
{
    tworldsvr::PacketHeader hdr{};
    boost::asio::read(sock, boost::asio::buffer(&hdr, sizeof(hdr)));
    const std::size_t body_size = hdr.wSize - tworldsvr::kPacketHeaderSize;
    std::vector<std::byte> body(body_size);
    if (body_size > 0)
        boost::asio::read(sock, boost::asio::buffer(body.data(), body_size));
    return {hdr.wID, std::move(body)};
}

std::vector<std::byte> RelaysvrBody(std::uint16_t wid)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint16_t>(b, wid);
    return b;
}

std::vector<std::byte> AddCharBody(std::uint32_t char_id,
                                    std::uint32_t key)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, 0x7f000001);
    tworldsvr::wire::WritePOD<std::uint16_t>(b, 33500);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, /*user_id=*/100);
    return b;
}

std::vector<std::byte> NameBody(std::uint32_t char_id,
                                 std::uint32_t key,
                                 const std::string& name)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, /*IK_NAME=*/ 48);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, 0);
    tworldsvr::wire::WritePOD<std::uint16_t>(b, 0);
    tworldsvr::wire::WriteString(b, name);
    return b;
}

std::vector<std::byte> GuildLoadBody(std::uint32_t char_id,
                                      std::uint32_t key,
                                      std::uint32_t guild_id,
                                      const std::string& name)
{
    std::vector<std::byte> b;
    using namespace tworldsvr::wire;
    WritePOD(b, char_id); WritePOD(b, key); WritePOD(b, guild_id);
    WriteString(b, name);
    WritePOD<std::uint32_t>(b, 1234);             // dwFame
    WritePOD<std::uint32_t>(b, 0xFF8800);         // dwFameColor
    WritePOD<std::uint8_t>(b, 10);                // bMaxCabinet
    WritePOD<std::uint8_t>(b, 5);                 // bGPoint
    WritePOD<std::uint8_t>(b, 3);                 // bLevel
    WritePOD<std::uint32_t>(b, char_id);          // dwChief
    WritePOD<std::uint32_t>(b, 50000);            // dwExp
    WritePOD<std::uint32_t>(b, 100);              // dwGI
    WritePOD<std::uint8_t>(b, 1);                 // bStatus
    WritePOD<std::uint32_t>(b, 9999);             // dwGold
    WritePOD<std::uint32_t>(b, 8888);             // dwSilver
    WritePOD<std::uint32_t>(b, 7777);             // dwCooper
    WritePOD<std::uint8_t>(b, 0);                 // bDisorg
    WritePOD<std::uint32_t>(b, 0);                // dwTime
    WritePOD<std::int64_t>(b, 1700000000);        // timeEstablish
    WritePOD<std::uint32_t>(b, 4242);             // dwPvPTotalPoint
    WritePOD<std::uint32_t>(b, 100);              // dwPvPUseablePoint
    WritePOD<std::uint16_t>(b, 0);                // wCabinetCount
    return b;
}

std::vector<std::byte> LeaveBody(std::uint32_t char_id,
                                  std::uint32_t key)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    return b;
}

std::vector<std::byte> DisorgBody(std::uint32_t char_id,
                                   std::uint32_t key,
                                   std::uint32_t guild_id,
                                   std::uint8_t  disorg)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD(b, guild_id);
    tworldsvr::wire::WritePOD(b, disorg);
    return b;
}

std::vector<std::byte> DutyBody(std::uint32_t char_id,
                                 std::uint32_t key,
                                 const std::string& target_name,
                                 std::uint8_t  duty)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WriteString(b, target_name);
    tworldsvr::wire::WritePOD(b, duty);
    return b;
}

std::vector<std::byte> FameBody(std::uint32_t char_id,
                                 std::uint32_t key,
                                 std::uint32_t fame,
                                 std::uint32_t fame_color)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD(b, fame);
    tworldsvr::wire::WritePOD(b, fame_color);
    return b;
}

std::vector<std::byte> KickoutBody(std::uint32_t char_id,
                                    std::uint32_t key,
                                    const std::string& target_name)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WriteString(b, target_name);
    return b;
}

std::vector<std::byte> ContributionBody(std::uint32_t char_id,
                                         std::uint32_t key,
                                         std::uint32_t exp,
                                         std::uint32_t gold,
                                         std::uint32_t silver,
                                         std::uint32_t cooper,
                                         std::uint32_t pvp_point)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD(b, exp);
    tworldsvr::wire::WritePOD(b, gold);
    tworldsvr::wire::WritePOD(b, silver);
    tworldsvr::wire::WritePOD(b, cooper);
    tworldsvr::wire::WritePOD(b, pvp_point);
    return b;
}

std::vector<std::byte> MemberAddBody(std::uint32_t guild_id,
                                      std::uint32_t char_id,
                                      std::uint8_t  level,
                                      std::uint8_t  duty)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, guild_id);
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, level);
    tworldsvr::wire::WritePOD(b, duty);
    return b;
}

std::vector<std::byte> InviteAnswerBody(std::uint32_t char_id,
                                         std::uint32_t key,
                                         std::uint8_t  answer,
                                         std::uint32_t inviter_id)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD(b, answer);
    tworldsvr::wire::WritePOD(b, inviter_id);
    return b;
}

std::vector<std::byte> EntercharBody(std::uint32_t char_id,
                                      const std::string& name)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WriteString(b, name);
    return b;
}

} // namespace

int main()
{
    using boost::asio::ip::tcp;
    using namespace std::chrono_literals;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;

    boost::asio::io_context io;
    tworldsvr::CharRegistry  chars;
    tworldsvr::GuildRegistry guilds;
    tworldsvr::PeerRegistry  peers;
    tworldsvr::GuildWantedRegistry guild_wanted;
    tworldsvr::GuildTacticsWantedRegistry guild_tactics_wanted;
    tworldsvr::FakeGuildRepository fake_repo;
    tworldsvr::HandlerContext ctx{};
    ctx.io           = &io;
    ctx.chars        = &chars;
    ctx.guilds       = &guilds;
    ctx.peers        = &peers;
    ctx.guild_wanted = &guild_wanted;
    ctx.guild_tactics_wanted = &guild_tactics_wanted;
    ctx.guild_repo   = &fake_repo;
    ctx.nation       = 0;

    tworldsvr::WorldServerConfig svr_cfg{};
    svr_cfg.port = 0;
    svr_cfg.max_connections = 4;
    svr_cfg.ctx = ctx;
    tworldsvr::WorldServer server(io, svr_cfg);
    const std::uint16_t port = server.Port();
    boost::asio::co_spawn(io, server.Run(), boost::asio::detached);
    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(20ms);

    boost::asio::io_context client_io;
    tcp::socket peer1(client_io);
    peer1.connect(tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port));
    std::this_thread::sleep_for(20ms);

    SendFramed(peer1, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0042));
    // Drain RELAYSVR_ACK.
    { auto [w, _] = ReadFramed(peer1); EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    EXPECT(peers.Size() == 1);

    // --- Scenario 1: load guild → TChar.guild_id set -----------------
    SendFramed(peer1, ToUint16(MessageId::MW_ADDCHAR_ACK),
        AddCharBody(42, 0xCAFEBABE));
    SendFramed(peer1, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        NameBody(42, 0xCAFEBABE, "Alice"));
    SendFramed(peer1, ToUint16(MessageId::DM_GUILDLOAD_ACK),
        GuildLoadBody(42, 0xCAFEBABE, 7, "Alpha"));
    // Drain the GUILDESTABLISH_REQ reply.
    { auto [w, _] = ReadFramed(peer1); EXPECT(w == ToUint16(MessageId::MW_GUILDESTABLISH_REQ)); }

    // Poll until the back-pointer is set (handler is async).
    for (int i = 0; i < 50; ++i)
    {
        if (auto c = chars.Find(42))
        {
            std::lock_guard g(c->lock);
            if (c->guild_id == 7) break;
        }
        std::this_thread::sleep_for(10ms);
    }
    {
        auto c = chars.Find(42);
        EXPECT(c != nullptr);
        if (c) { std::lock_guard g(c->lock); EXPECT(c->guild_id == 7); }
        auto g = guilds.Find(7);
        EXPECT(g != nullptr);
        if (g) { std::lock_guard gl(g->lock); EXPECT(g->members.size() == 1); }
    }

    // --- Scenario 2: GUILDLEAVE_ACK removes member + replies --------
    SendFramed(peer1, ToUint16(MessageId::MW_GUILDLEAVE_ACK),
        LeaveBody(42, 0xCAFEBABE));
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDLEAVE_REQ));
        tworldsvr::wire::Reader r(body);
        std::uint32_t cid = 0, key = 0; std::string name;
        std::uint8_t  reason = 0; std::uint32_t time_unix = 0;
        EXPECT(r.Read(cid));    EXPECT(cid == 42);
        EXPECT(r.Read(key));    EXPECT(key == 0xCAFEBABE);
        EXPECT(r.ReadString(name)); EXPECT(name == "Alice");
        EXPECT(r.Read(reason)); EXPECT(reason == 12); // GUILD_LEAVE_SELF (NetCode.h:448)
        EXPECT(r.Read(time_unix));
        EXPECT(time_unix > 0);                       // any non-zero epoch
    }
    {
        auto c = chars.Find(42);
        if (c) { std::lock_guard g(c->lock); EXPECT(c->guild_id == 0); }
        auto g = guilds.Find(7);
        if (g) { std::lock_guard gl(g->lock); EXPECT(g->members.empty()); }
    }

    // --- Scenario 3: second leave is a benign no-op -----------------
    SendFramed(peer1, ToUint16(MessageId::MW_GUILDLEAVE_ACK),
        LeaveBody(42, 0xCAFEBABE));
    std::this_thread::sleep_for(60ms);
    // No reply expected (handler short-circuits on guild_id=0).
    // Confirm the socket is still alive by sending another packet.
    SendFramed(peer1, ToUint16(MessageId::RW_ENTERCHAR_REQ),
        EntercharBody(42, "Alice"));
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::RW_ENTERCHAR_ACK));
        // --- Scenario 4: ENTERCHAR_ACK after leave carries guild=0 ---
        tworldsvr::wire::Reader r(body);
        std::uint32_t cid = 0; std::string name;
        std::uint8_t  result = 0, country = 0, aid = 0;
        std::uint32_t gid = 0, gchief = 0; std::uint8_t duty = 0;
        EXPECT(r.Read(cid));         EXPECT(cid == 42);
        EXPECT(r.ReadString(name));
        EXPECT(r.Read(result));      EXPECT(result == 1);
        EXPECT(r.Read(country));
        EXPECT(r.Read(aid));
        EXPECT(r.Read(gid));         EXPECT(gid == 0);
        EXPECT(r.Read(gchief));      EXPECT(gchief == 0);
        EXPECT(r.Read(duty));        EXPECT(duty == 0);
    }

    // --- Scenario 5: load a second guild with two members for the
    //                 W3a-4b duty/fame/disorg cases ----------------
    SendFramed(peer1, ToUint16(MessageId::MW_ADDCHAR_ACK),
        AddCharBody(200, 0xBEEF1111));
    SendFramed(peer1, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        NameBody(200, 0xBEEF1111, "Bob"));
    SendFramed(peer1, ToUint16(MessageId::DM_GUILDLOAD_ACK),
        GuildLoadBody(200, 0xBEEF1111, 8, "Bravos"));
    // Drain GUILDESTABLISH_REQ.
    { auto [w, _] = ReadFramed(peer1); EXPECT(w == ToUint16(MessageId::MW_GUILDESTABLISH_REQ)); }

    // Manually add a second member via the registry (real
    // OnGuildMemberAdd is W3a-4c). Mirrors what
    // OnDM_GUILDMEMBERADD_REQ + OnMW_GUILDINVITEANSWER_ACK will
    // do once those land.
    SendFramed(peer1, ToUint16(MessageId::MW_ADDCHAR_ACK),
        AddCharBody(201, 0xBEEF2222));
    SendFramed(peer1, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        NameBody(201, 0xBEEF2222, "Bravo2"));
    for (int i = 0; i < 50; ++i)
    {
        if (chars.FindByName("Bravo2")) break;
        std::this_thread::sleep_for(10ms);
    }
    {
        auto g = guilds.Find(8);
        EXPECT(g != nullptr);
        if (g)
        {
            std::lock_guard gl(g->lock);
            tworldsvr::TGuildMember m2;
            m2.char_id  = 201;
            m2.guild_id = 8;
            m2.duty     = tworldsvr::guild::kDutyNone;
            m2.name     = "Bravo2";
            g->members.push_back(m2);
            g->pvp_useable_point = 50000;  // enough for fame change
        }
        auto m2_char = chars.Find(201);
        if (m2_char)
        {
            std::lock_guard cg(m2_char->lock);
            m2_char->guild_id = 8;
        }
    }

    // --- Scenario 6: DUTY change promotes Bravo2 to vice-chief ------
    SendFramed(peer1, ToUint16(MessageId::MW_GUILDDUTY_ACK),
        DutyBody(200, 0xBEEF1111, "Bravo2", tworldsvr::guild::kDutyViceChief));
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDDUTY_REQ));
        tworldsvr::wire::Reader r(body);
        std::uint32_t cid = 0, key = 0; std::string name;
        std::uint8_t duty = 0;
        EXPECT(r.Read(cid));      EXPECT(cid == 200);
        EXPECT(r.Read(key));      EXPECT(key == 0xBEEF1111);
        EXPECT(r.ReadString(name)); EXPECT(name == "Bravo2");
        EXPECT(r.Read(duty));     EXPECT(duty == tworldsvr::guild::kDutyViceChief);
    }
    {
        auto g = guilds.Find(8);
        EXPECT(g != nullptr);
        if (g)
        {
            std::lock_guard gl(g->lock);
            const auto* m = g->FindMember(201);
            EXPECT(m && m->duty == tworldsvr::guild::kDutyViceChief);
        }
        // Repo recorded the persistence call.
        const auto calls = fake_repo.Calls();
        bool saw_duty_call = false;
        for (const auto& c : calls)
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kUpdateMemberDuty
                && c.char_id == 201 && c.guild_id == 8
                && c.a == tworldsvr::guild::kDutyViceChief)
            { saw_duty_call = true; break; }
        }
        EXPECT(saw_duty_call);
    }

    // --- Scenario 7: FAME change broadcasts to all members ----------
    SendFramed(peer1, ToUint16(MessageId::MW_GUILDFAME_ACK),
        FameBody(200, 0xBEEF1111, /*fame=*/ 9999, /*color=*/ 0xAABBCC));
    // Two FAME_REQ replies expected — one per member (chief +
    // Bravo2) since both have main_server_id=0x42 (which equals
    // peer1's wID LOBYTE).
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDFAME_REQ));
        tworldsvr::wire::Reader r(body);
        std::uint32_t cid = 0, key = 0, dwID = 0, fame = 0, color = 0;
        std::uint8_t result = 0;
        EXPECT(r.Read(cid));
        EXPECT(r.Read(key));
        EXPECT(r.Read(result));   EXPECT(result == tworldsvr::guild::kSuccess);
        EXPECT(r.Read(dwID));     EXPECT(dwID == 200);
        EXPECT(r.Read(fame));     EXPECT(fame == 9999);
        EXPECT(r.Read(color));    EXPECT(color == 0xAABBCC);
    }
    {
        auto [w, _b] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDFAME_REQ));  // second member
    }
    {
        auto g = guilds.Find(8);
        EXPECT(g != nullptr);
        if (g)
        {
            std::lock_guard gl(g->lock);
            EXPECT(g->fame == 9999);
            EXPECT(g->fame_color == 0xAABBCC);
            EXPECT(g->pvp_useable_point ==
                   50000 - tworldsvr::guild::kPvPointCostFameChange);
        }
    }

    // --- Scenario 8: DISORGANIZATION flips flag + persists ----------
    SendFramed(peer1, ToUint16(MessageId::DM_GUILDDISORGANIZATION_REQ),
        DisorgBody(200, 0xBEEF1111, 8, /*disorg=*/ 1));
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDDISORGANIZATION_REQ));
        tworldsvr::wire::Reader r(body);
        std::uint32_t cid = 0, key = 0; std::uint8_t d = 0;
        EXPECT(r.Read(cid)); EXPECT(cid == 200);
        EXPECT(r.Read(key)); EXPECT(key == 0xBEEF1111);
        EXPECT(r.Read(d));   EXPECT(d == 1);
    }
    {
        auto g = guilds.Find(8);
        EXPECT(g != nullptr);
        if (g)
        {
            std::lock_guard gl(g->lock);
            EXPECT(g->disorg == 1);
            EXPECT(g->disorg_time > 0);
        }
        bool saw_disorg_call = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kSetDisorg
                && c.guild_id == 8 && c.a == 1 && c.b > 0)
            { saw_disorg_call = true; break; }
        }
        EXPECT(saw_disorg_call);
    }

    // --- Scenario 9: DUTY change on disorg guild is rejected ----------
    // Legacy gate at SSHandler.cpp:3430 — disorg guild ignores
    // duty changes. We confirm no reply lands by sending a known
    // packet right after and asserting it's the next response.
    SendFramed(peer1, ToUint16(MessageId::MW_GUILDDUTY_ACK),
        DutyBody(200, 0xBEEF1111, "Bravo2", tworldsvr::guild::kDutyChief));
    SendFramed(peer1, ToUint16(MessageId::RW_ENTERCHAR_REQ),
        EntercharBody(200, "Bob"));
    {
        auto [w, _] = ReadFramed(peer1);
        // Skips the disorg-rejected DUTY_REQ, lands on ENTERCHAR_ACK.
        EXPECT(w == ToUint16(MessageId::RW_ENTERCHAR_ACK));
    }

    // --- Scenario 10: Cancel disorg + KICKOUT ------------------------
    //
    // Re-enable the guild (legacy lets the chief cancel disorg by
    // sending bDisorg=0), then exercise OnGuildKickoutAck. After
    // the kick, Bravo2's guild_id should be 0 and the guild's
    // members vector should contain only the chief.
    SendFramed(peer1, ToUint16(MessageId::DM_GUILDDISORGANIZATION_REQ),
        DisorgBody(200, 0xBEEF1111, 8, /*disorg=*/ 0));
    { auto [w, _] = ReadFramed(peer1); EXPECT(w == ToUint16(MessageId::MW_GUILDDISORGANIZATION_REQ)); }

    SendFramed(peer1, ToUint16(MessageId::MW_GUILDKICKOUT_ACK),
        KickoutBody(200, 0xBEEF1111, "Bravo2"));
    // Two GUILDLEAVE_REQ replies expected: one to the chief
    // (peer1) about the kick, one to Bravo2's main peer
    // (also peer1 since both share wID 0x42).
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDLEAVE_REQ));
        tworldsvr::wire::Reader r(body);
        std::uint32_t cid = 0, key = 0; std::string name;
        std::uint8_t reason = 0;
        EXPECT(r.Read(cid));        EXPECT(cid == 200);
        EXPECT(r.Read(key));
        EXPECT(r.ReadString(name)); EXPECT(name == "Bravo2");
        EXPECT(r.Read(reason));     EXPECT(reason == tworldsvr::guild::kLeaveKick);
    }
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDLEAVE_REQ));
        tworldsvr::wire::Reader r(body);
        std::uint32_t cid = 0, key = 0; std::string name;
        std::uint8_t reason = 0;
        EXPECT(r.Read(cid));        EXPECT(cid == 201);   // Bravo2 himself
        EXPECT(r.Read(key));
        EXPECT(r.ReadString(name));
        EXPECT(r.Read(reason));     EXPECT(reason == tworldsvr::guild::kLeaveKick);
    }
    {
        auto g = guilds.Find(8);
        if (g) { std::lock_guard gl(g->lock); EXPECT(g->members.size() == 1); }
        auto bravo = chars.Find(201);
        if (bravo) { std::lock_guard cg(bravo->lock); EXPECT(bravo->guild_id == 0); }
    }

    // --- Scenario 11: CONTRIBUTION applies deltas --------------------
    //
    // Chief contributes 100 exp + 500 gold + 2000 pvp_point. Guild
    // gold goes 9999 → 10499, member service score 0 → 100.
    SendFramed(peer1, ToUint16(MessageId::MW_GUILDCONTRIBUTION_ACK),
        ContributionBody(200, 0xBEEF1111, /*exp*/100, /*gold*/500,
            /*silver*/0, /*cooper*/0, /*pvp*/2000));
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDCONTRIBUTION_REQ));
        tworldsvr::wire::Reader r(body);
        std::uint32_t cid = 0, key = 0, exp = 0, gold = 0, silver = 0,
                       cooper = 0, pvp = 0;
        std::uint8_t result = 0;
        EXPECT(r.Read(cid));    EXPECT(cid == 200);
        EXPECT(r.Read(key));
        EXPECT(r.Read(result)); EXPECT(result == tworldsvr::guild::kSuccess);
        EXPECT(r.Read(exp));    EXPECT(exp == 100);
        EXPECT(r.Read(gold));   EXPECT(gold == 500);
        EXPECT(r.Read(silver));
        EXPECT(r.Read(cooper));
        EXPECT(r.Read(pvp));    EXPECT(pvp == 2000);
    }
    {
        auto g = guilds.Find(8);
        EXPECT(g != nullptr);
        if (g)
        {
            std::lock_guard gl(g->lock);
            EXPECT(g->gold == 9999 + 500);
            EXPECT(g->exp  == 50000 + 100);
            const auto* chief = g->FindMember(200);
            EXPECT(chief && chief->service == 100);
        }
    }

    // --- Scenario 12: DM_GUILDMEMBERADD_REQ persists, no reply -------
    //
    // Pure DB write — confirm the fake repo recorded the call,
    // then send a follow-up RW_ENTERCHAR_REQ to prove the framer
    // stayed alive (no reply between sends).
    SendFramed(peer1, ToUint16(MessageId::DM_GUILDMEMBERADD_REQ),
        MemberAddBody(8, /*char*/ 333, /*level*/ 5,
            tworldsvr::guild::kDutyNone));
    SendFramed(peer1, ToUint16(MessageId::RW_ENTERCHAR_REQ),
        EntercharBody(200, "Bob"));
    { auto [w, _] = ReadFramed(peer1); EXPECT(w == ToUint16(MessageId::RW_ENTERCHAR_ACK)); }
    {
        bool saw_add = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind::kAddMember
                && c.guild_id == 8 && c.char_id == 333 && c.a == 5
                && c.b == tworldsvr::guild::kDutyNone)
            { saw_add = true; break; }
        }
        EXPECT(saw_add);
    }

    // --- Scenario 13: INVITEANSWER ASK_NO declines (chief gets JOIN) ---
    //
    // Set up a fresh char 400 who's not in any guild, then have
    // them answer NO to an invite from char 200 (chief of
    // guild_id=8). The chief receives MW_GUILDJOIN_REQ with
    // result=ASK_NO carrying empty guild meta.
    SendFramed(peer1, ToUint16(MessageId::MW_ADDCHAR_ACK),
        AddCharBody(400, 0xFEED0001));
    SendFramed(peer1, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        NameBody(400, 0xFEED0001, "Carol"));
    for (int i = 0; i < 50; ++i)
    {
        if (chars.FindByName("Carol")) break;
        std::this_thread::sleep_for(10ms);
    }

    // Re-add char 200's guild (we kicked Bravo2 + canceled disorg
    // in earlier scenarios; the guild itself is still loaded but
    // we need to make sure 200's back-pointer is still set).
    {
        auto c200 = chars.Find(200);
        if (c200) { std::lock_guard cg(c200->lock); c200->guild_id = 8; }
    }

    SendFramed(peer1, ToUint16(MessageId::MW_GUILDINVITEANSWER_ACK),
        InviteAnswerBody(400, 0xFEED0001, /*answer=ASK_NO=*/1, 200));
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDJOIN_REQ));
        tworldsvr::wire::Reader r(body);
        std::uint32_t cid = 0, key = 0;
        std::uint8_t  result = 0;
        EXPECT(r.Read(cid));    EXPECT(cid == 200);     // chief gets it
        EXPECT(r.Read(key));
        EXPECT(r.Read(result)); EXPECT(result == 1);    // ASK_NO
    }

    // --- Scenario 14: INVITEANSWER ASK_YES joins the guild --------
    //
    // Carol (char 400) accepts. Both chief + Carol get
    // MW_GUILDJOIN_REQ with kSuccess. Carol's TChar.guild_id flips
    // to 8 and the guild's member list gains an entry. Repo
    // records AddMember(400, 8, …, kDutyNone).
    SendFramed(peer1, ToUint16(MessageId::MW_GUILDINVITEANSWER_ACK),
        InviteAnswerBody(400, 0xFEED0001, /*answer=ASK_YES=*/0, 200));
    // Carol (peer1) gets a reply…
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDJOIN_REQ));
        tworldsvr::wire::Reader r(body);
        std::uint32_t cid = 0, key = 0, gid = 0;
        std::uint8_t  result = 0;
        EXPECT(r.Read(cid));    EXPECT(cid == 400);
        EXPECT(r.Read(key));    EXPECT(key == 0xFEED0001);
        // W3a-7 fix: kJoinSuccess (15) on both invite reply
        // branches — legacy NotifyAddGuildMember sends
        // GUILD_JOIN_SUCCESS per NetCode.h:451 to both invited
        // and inviter.
        EXPECT(r.Read(result)); EXPECT(result == tworldsvr::guild::kJoinSuccess);
        EXPECT(r.Read(gid));    EXPECT(gid == 8);
    }
    // …and the chief (also peer1 since both share the peer wID) too.
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDJOIN_REQ));
        tworldsvr::wire::Reader r(body);
        std::uint32_t cid = 0, key2 = 0;
        std::uint8_t  result = 0;
        EXPECT(r.Read(cid));    EXPECT(cid == 200);
        EXPECT(r.Read(key2));
        // W3a-7 fix: must be kJoinSuccess (15), not the W3a-6
        // kSuccess (0) — legacy NotifyAddGuildMember sends
        // GUILD_JOIN_SUCCESS per NetCode.h:451.
        EXPECT(r.Read(result));
        EXPECT(result == tworldsvr::guild::kJoinSuccess);
    }
    {
        auto carol = chars.Find(400);
        EXPECT(carol != nullptr);
        if (carol) { std::lock_guard g(carol->lock); EXPECT(carol->guild_id == 8); }
        auto g = guilds.Find(8);
        EXPECT(g != nullptr);
        if (g)
        {
            std::lock_guard gl(g->lock);
            EXPECT(g->FindMember(400) != nullptr);
        }
        bool saw_add = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind::kAddMember
                && c.guild_id == 8 && c.char_id == 400)
            { saw_add = true; break; }
        }
        EXPECT(saw_add);
    }

    // --- Scenario 15: GUILDMEMBERLIST returns roster ---------------
    //
    // Chief (char 200) refreshes the guild window. Server replies
    // MW_GUILDMEMBERLIST_REQ kSuccess + guild meta + variable-
    // length member tail. Should include chief (200) + Carol (400).
    SendFramed(peer1, ToUint16(MessageId::MW_GUILDMEMBERLIST_ACK),
        LeaveBody(200, 0xBEEF1111)); // same shape: { dwCharID, dwKEY }
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDMEMBERLIST_REQ));
        tworldsvr::wire::Reader r(body);
        std::uint32_t cid = 0, key = 0, gid = 0;
        std::uint8_t  result = 0;
        std::string   gname;
        std::uint16_t cnt = 0;
        EXPECT(r.Read(cid));    EXPECT(cid == 200);
        EXPECT(r.Read(key));
        EXPECT(r.Read(result)); EXPECT(result == tworldsvr::guild::kSuccess);
        EXPECT(r.Read(gid));    EXPECT(gid == 8);
        EXPECT(r.ReadString(gname));
        EXPECT(r.Read(cnt));    EXPECT(cnt == 2);   // chief + Carol

        bool saw_chief = false, saw_carol = false;
        for (std::uint16_t i = 0; i < cnt; ++i)
        {
            std::uint32_t mid = 0; std::string mname;
            std::uint8_t lvl = 0, klass = 0, duty = 0, peer_r = 0;
            std::uint8_t online = 0; std::uint32_t region = 0;
            std::uint16_t castle = 0; std::uint8_t camp = 0;
            std::uint32_t tactics = 0; std::uint8_t war = 0;
            std::int64_t connected = 0;
            EXPECT(r.Read(mid));         EXPECT(r.ReadString(mname));
            EXPECT(r.Read(lvl));         EXPECT(r.Read(klass));
            EXPECT(r.Read(duty));        EXPECT(r.Read(peer_r));
            EXPECT(r.Read(online));      EXPECT(r.Read(region));
            EXPECT(r.Read(castle));      EXPECT(r.Read(camp));
            EXPECT(r.Read(tactics));     EXPECT(r.Read(war));
            EXPECT(r.Read(connected));
            if (mid == 200) { saw_chief = true; EXPECT(duty == tworldsvr::guild::kDutyChief); }
            if (mid == 400) { saw_carol = true; }
        }
        EXPECT(saw_chief);
        EXPECT(saw_carol);
    }

    // --- Scenario 16: GUILDMEMBERLIST for char without guild --------
    //
    // Char 999 (which doesn't exist) → no reply. Use char 400
    // with wrong key → also no reply (key mismatch gate).
    // To actually exercise the kNotFound branch, char would need
    // guild_id=0; we test by sending for char 100 (Alice, who
    // left her guild in scenario 4).
    SendFramed(peer1, ToUint16(MessageId::MW_GUILDMEMBERLIST_ACK),
        LeaveBody(100, 0xCAFEBABE));
    {
        // Wait — char 100 was disconnected in scenario 4 socket close.
        // Use char 200 with wrong key → no reply.
        // Send a known-good packet to confirm framer survives.
    }

    // --- Scenario 17: DM_GUILDPVPOINT_REQ (W3a-13) ------------------
    //
    // DB pushes new PvP-point counters for guild 8. Handler
    // updates in-memory + persists via UpdatePvPoints. No reply.
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);     // guild_id
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 12345); // total
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 6789);  // useable
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 1000);  // month
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDPVPOINT_REQ), body);
    }
    for (int i = 0; i < 50; ++i)
    {
        bool saw = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kUpdatePvPoints
                && c.guild_id == 8 && c.a == 12345 && c.b == 6789
                && c.c == 1000)
            { saw = true; break; }
        }
        if (saw) break;
        std::this_thread::sleep_for(10ms);
    }
    {
        bool saw_pvp_call = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kUpdatePvPoints
                && c.guild_id == 8 && c.a == 12345 && c.b == 6789
                && c.c == 1000)
            { saw_pvp_call = true; break; }
        }
        EXPECT(saw_pvp_call);
        if (auto g = guilds.Find(8))
        {
            std::lock_guard gl(g->lock);
            EXPECT(g->pvp_total_point   == 12345);
            EXPECT(g->pvp_useable_point == 6789);
            EXPECT(g->pvp_month_point   == 1000);
        }
    }

    // --- Scenario 18: DM_GUILDDUTY_REQ (W3a-14 fan-in) -------------
    //
    // DB-side duty update (e.g. admin tool). Handler calls
    // repo->UpdateMemberDuty — no in-memory change here
    // because the DB is authoritative for this fan-in path.
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 201);  // char_id
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);    // guild_id
        tworldsvr::wire::WritePOD<std::uint8_t>(body,
            tworldsvr::guild::kDutyChief);
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDDUTY_REQ), body);
    }
    for (int i = 0; i < 50; ++i)
    {
        bool saw = false;
        std::size_t hits = 0;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kUpdateMemberDuty
                && c.char_id == 201 && c.guild_id == 8
                && c.a == tworldsvr::guild::kDutyChief)
            { ++hits; }
        }
        if (hits > 0) { saw = true; }
        if (saw) break;
        std::this_thread::sleep_for(10ms);
    }
    {
        bool saw_duty_fanin = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kUpdateMemberDuty
                && c.char_id == 201 && c.guild_id == 8
                && c.a == tworldsvr::guild::kDutyChief)
            { saw_duty_fanin = true; break; }
        }
        EXPECT(saw_duty_fanin);
    }

    // --- Scenario 19: DM_GUILDPEER_REQ (W3a-14 fan-in) -------------
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 201);  // char_id
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);    // guild_id
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 3);     // peer
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDPEER_REQ), body);
    }
    for (int i = 0; i < 50; ++i)
    {
        bool saw = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kUpdateMemberPeer
                && c.char_id == 201 && c.guild_id == 8 && c.a == 3)
            { saw = true; break; }
        }
        if (saw) break;
        std::this_thread::sleep_for(10ms);
    }
    {
        bool saw_peer_fanin = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kUpdateMemberPeer
                && c.char_id == 201 && c.guild_id == 8 && c.a == 3)
            { saw_peer_fanin = true; break; }
        }
        EXPECT(saw_peer_fanin);
    }

    // --- Scenario 20: DM_GUILDCONTRIBUTION_REQ (W3a-14 fan-in) -----
    //
    // 6-field wire (no pvp_point). Repo gets called with
    // pvp_point=0 since the legacy CSP signature ignores it.
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);    // guild_id
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 201);  // char_id
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 500);  // exp
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 100);  // gold
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 50);   // silver
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 25);   // cooper
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDCONTRIBUTION_REQ),
            body);
    }
    for (int i = 0; i < 50; ++i)
    {
        bool saw = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kIncrementContribution
                && c.char_id == 201 && c.guild_id == 8
                && c.a == 500 && c.b == 100 && c.c == 50 && c.d == 25
                && c.e == 0)
            { saw = true; break; }
        }
        if (saw) break;
        std::this_thread::sleep_for(10ms);
    }
    {
        bool saw_contrib_fanin = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kIncrementContribution
                && c.char_id == 201 && c.guild_id == 8
                && c.a == 500 && c.b == 100 && c.c == 50 && c.d == 25
                && c.e == 0)
            { saw_contrib_fanin = true; break; }
        }
        EXPECT(saw_contrib_fanin);
    }

    // --- Scenario 21: DM_GUILDLEVEL_REQ (W3a-14 fan-in) ------------
    //
    // Level fan-in defensively updates in-memory TGuild.level
    // because the peerage gate's member-cap derives from it.
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);   // guild_id
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 5);    // level
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDLEVEL_REQ), body);
    }
    for (int i = 0; i < 50; ++i)
    {
        if (auto g = guilds.Find(8))
        {
            std::lock_guard gl(g->lock);
            if (g->level == 5) break;
        }
        std::this_thread::sleep_for(10ms);
    }
    {
        if (auto g = guilds.Find(8))
        {
            std::lock_guard gl(g->lock);
            EXPECT(g->level == 5);
        }
        bool saw_level_fanin = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kUpdateLevel
                && c.guild_id == 8 && c.a == 5)
            { saw_level_fanin = true; break; }
        }
        EXPECT(saw_level_fanin);
    }

    // --- Scenario 22: DM_GUILDPOINTREWARD_REQ (W3a-14 fan-in) ------
    //
    // Logs a single PvP-point grant + updates running totals.
    // String "Bravo2" carried in the wire body — fake repo drops
    // it from the Call record (numeric fields verified below).
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);     // guild_id
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 250);   // point
        tworldsvr::wire::WriteString(body, "Bravo2");          // recipient
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 99999); // total
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8888);  // useable
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDPOINTREWARD_REQ),
            body);
    }
    for (int i = 0; i < 50; ++i)
    {
        bool saw = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kLogPointReward
                && c.guild_id == 8 && c.a == 250 && c.b == 99999
                && c.c == 8888)
            { saw = true; break; }
        }
        if (saw) break;
        std::this_thread::sleep_for(10ms);
    }
    {
        bool saw_reward_log = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kLogPointReward
                && c.guild_id == 8 && c.a == 250 && c.b == 99999
                && c.c == 8888)
            { saw_reward_log = true; break; }
        }
        EXPECT(saw_reward_log);
        if (auto g = guilds.Find(8))
        {
            std::lock_guard gl(g->lock);
            EXPECT(g->pvp_total_point   == 99999);
            EXPECT(g->pvp_useable_point == 8888);
        }
    }

    // --- Scenario 23: DM_GUILDFAME_REQ (W3a-15 fan-in) -------------
    //
    // FAME fan-in mirrors fame/fame_color into the registry
    // because they're broadcast in GuildInfo / Establish.
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);      // guild_id
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 77777);  // fame
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 0x123456);// color
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDFAME_REQ), body);
    }
    for (int i = 0; i < 50; ++i)
    {
        if (auto g = guilds.Find(8))
        {
            std::lock_guard gl(g->lock);
            if (g->fame == 77777) break;
        }
        std::this_thread::sleep_for(10ms);
    }
    {
        if (auto g = guilds.Find(8))
        {
            std::lock_guard gl(g->lock);
            EXPECT(g->fame       == 77777);
            EXPECT(g->fame_color == 0x123456);
        }
        bool saw_fame_fanin = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kUpdateFame
                && c.guild_id == 8 && c.a == 77777 && c.b == 0x123456)
            { saw_fame_fanin = true; break; }
        }
        EXPECT(saw_fame_fanin);
    }

    // --- Scenario 24: DM_GUILDARTICLEADD_REQ (W3a-15 fan-in) -------
    //
    // Article fan-in is repo-only: in-memory TGuild.articles
    // is owned by the article_index counter on the MW_*_ACK path
    // and the DB-pushed id might collide. Defer to the next
    // OnGuildArticleListAck refresh — same as legacy parity.
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);     // guild_id
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 999);   // article_id
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 1);      // duty
        tworldsvr::wire::WriteString(body, "Bravo2");          // writer
        tworldsvr::wire::WriteString(body, "DB-pushed title"); // title
        tworldsvr::wire::WriteString(body, "Body text");       // body
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 1700001000); // time
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDARTICLEADD_REQ),
            body);
    }
    for (int i = 0; i < 50; ++i)
    {
        bool saw = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kAddArticle
                && c.guild_id == 8 && c.char_id == 999)
            { saw = true; break; }
        }
        if (saw) break;
        std::this_thread::sleep_for(10ms);
    }
    {
        bool saw_add = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kAddArticle
                && c.guild_id == 8 && c.char_id == 999)
            { saw_add = true; break; }
        }
        EXPECT(saw_add);
    }

    // --- Scenario 25: DM_GUILDARTICLEDEL_REQ (W3a-15 fan-in) -------
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);     // guild_id
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 999);   // article_id
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDARTICLEDEL_REQ),
            body);
    }
    for (int i = 0; i < 50; ++i)
    {
        bool saw = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kDelArticle
                && c.guild_id == 8 && c.char_id == 999)
            { saw = true; break; }
        }
        if (saw) break;
        std::this_thread::sleep_for(10ms);
    }
    {
        bool saw_del = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kDelArticle
                && c.guild_id == 8 && c.char_id == 999)
            { saw_del = true; break; }
        }
        EXPECT(saw_del);
    }

    // --- Scenario 26: DM_GUILDARTICLEUPDATE_REQ (W3a-15 fan-in) ----
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);   // guild_id
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 1000);// article_id
        tworldsvr::wire::WriteString(body, "Edited title");  // title
        tworldsvr::wire::WriteString(body, "Edited body");   // body
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDARTICLEUPDATE_REQ),
            body);
    }
    for (int i = 0; i < 50; ++i)
    {
        bool saw = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kUpdateArticle
                && c.guild_id == 8 && c.char_id == 1000)
            { saw = true; break; }
        }
        if (saw) break;
        std::this_thread::sleep_for(10ms);
    }
    {
        bool saw_upd = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kUpdateArticle
                && c.guild_id == 8 && c.char_id == 1000)
            { saw_upd = true; break; }
        }
        EXPECT(saw_upd);
    }

    // --- Scenario 27: DM_GUILDWANTEDADD_REQ (W3a-16 fan-in) -------
    //
    // WANTED ADD fan-in: looks up guild_id=8 in the registry to
    // pick country + name, computes end_time = now + 14 days,
    // mirrors into GuildWantedRegistry + persists via repo.
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);     // guild_id
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 10);     // min_level
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 50);     // max_level
        tworldsvr::wire::WriteString(body, "DB-pushed posting");
        tworldsvr::wire::WriteString(body, "Looking for raiders");
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDWANTEDADD_REQ),
            body);
    }
    for (int i = 0; i < 50; ++i)
    {
        bool saw = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kAddWanted
                && c.guild_id == 8)
            { saw = true; break; }
        }
        if (saw) break;
        std::this_thread::sleep_for(10ms);
    }
    {
        bool saw_add = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kAddWanted
                && c.guild_id == 8)
            { saw_add = true; break; }
        }
        EXPECT(saw_add);
        // Registry should have the entry.
        EXPECT(guild_wanted.Find(8).has_value());
    }

    // --- Scenario 28: DM_GUILDWANTEDDEL_REQ (W3a-16 fan-in) -------
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);     // guild_id
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDWANTEDDEL_REQ),
            body);
    }
    for (int i = 0; i < 50; ++i)
    {
        if (!guild_wanted.Find(8).has_value()) break;
        std::this_thread::sleep_for(10ms);
    }
    {
        EXPECT(!guild_wanted.Find(8).has_value());
        bool saw_del = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kDeleteWanted
                && c.guild_id == 8)
            { saw_del = true; break; }
        }
        EXPECT(saw_del);
    }

    // --- Scenario 29: DM_GUILDVOLUNTEERING_REQ (W3a-16 fan-in) ----
    //
    // VOLUNTEERING ADD fan-in needs a wanted entry to attach
    // the applicant to. Re-add guild 8's wanted, then push the
    // applicant via DM. Then verify the app landed in the
    // registry + persistence call ran. bType=kMember (=0) here;
    // bType=kTactics path is exercised by scenario 30.
    {
        // Recreate wanted entry first.
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 1);
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 99);
        tworldsvr::wire::WriteString(body, "Recruiting again");
        tworldsvr::wire::WriteString(body, "All welcome");
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDWANTEDADD_REQ),
            body);
    }
    for (int i = 0; i < 50; ++i)
    {
        if (guild_wanted.Find(8).has_value()) break;
        std::this_thread::sleep_for(10ms);
    }
    EXPECT(guild_wanted.Find(8).has_value());
    // Push volunteering for char 400 (Carol — added in scenario 14).
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint8_t>(body,
            tworldsvr::guild::kVolunteerKindMember);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 400); // char_id
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);   // wanted_id
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDVOLUNTEERING_REQ),
            body);
    }
    for (int i = 0; i < 50; ++i)
    {
        bool saw = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kAddVolunteerApp
                && c.char_id == 400 && c.guild_id == 8)
            { saw = true; break; }
        }
        if (saw) break;
        std::this_thread::sleep_for(10ms);
    }
    {
        bool saw_add_app = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kAddVolunteerApp
                && c.char_id == 400 && c.guild_id == 8)
            { saw_add_app = true; break; }
        }
        EXPECT(saw_add_app);
        // Note: Carol (char 400) joined guild 8 in scenario 14, so
        // the registry's AddApp gate may reject (already in a guild
        // → kHaveGuild equivalent). The handler logs that
        // divergence but still persists — that's the DB-authoritative
        // contract. Either way, the persistence call ran.
    }

    // --- Scenario 30: DM_GUILDVOLUNTEERING_REQ kTactics dropped ---
    //
    // Tactics applicants get logged + dropped (subsystem ports
    // later). No persistence call should land for this type.
    const auto pre_calls = fake_repo.Calls().size();
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint8_t>(body,
            tworldsvr::guild::kVolunteerKindTactics);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 401);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDVOLUNTEERING_REQ),
            body);
    }
    std::this_thread::sleep_for(80ms);
    EXPECT(fake_repo.Calls().size() == pre_calls);

    // --- Scenario 31: DM_GUILDVOLUNTEERINGDEL_REQ (W3a-16 fan-in) -
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint8_t>(body,
            tworldsvr::guild::kVolunteerKindMember);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 400); // char_id
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDVOLUNTEERINGDEL_REQ),
            body);
    }
    for (int i = 0; i < 50; ++i)
    {
        bool saw = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kDelVolunteerApp
                && c.char_id == 400)
            { saw = true; break; }
        }
        if (saw) break;
        std::this_thread::sleep_for(10ms);
    }
    {
        bool saw_del_app = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kDelVolunteerApp
                && c.char_id == 400)
            { saw_del_app = true; break; }
        }
        EXPECT(saw_del_app);
    }

    // --- Scenario 32: DM_GUILDLEAVE_REQ (W3a-17 fan-in) ------------
    //
    // Carol (char 400) is in guild 8 from scenario 14. DB tells
    // us she's leaving; we should drop her from guild->members,
    // clear her TChar.guild_id back-pointer, and persist via
    // repo->RemoveMember. No wire reply.
    {
        auto g = guilds.Find(8);
        EXPECT(g != nullptr);
        if (g)
        {
            std::lock_guard gl(g->lock);
            EXPECT(g->FindMember(400) != nullptr);  // pre-state
        }
        auto c = chars.Find(400);
        EXPECT(c != nullptr);
        if (c) { std::lock_guard cg(c->lock); EXPECT(c->guild_id == 8); }
    }
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);          // guild_id
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 400);        // char_id
        tworldsvr::wire::WritePOD<std::uint8_t>(body,
            tworldsvr::guild::kLeaveSelf);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 1700002000); // leave_time
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDLEAVE_REQ), body);
    }
    for (int i = 0; i < 50; ++i)
    {
        bool gone = false;
        if (auto g = guilds.Find(8))
        {
            std::lock_guard gl(g->lock);
            gone = (g->FindMember(400) == nullptr);
        }
        if (gone) break;
        std::this_thread::sleep_for(10ms);
    }
    {
        if (auto g = guilds.Find(8))
        {
            std::lock_guard gl(g->lock);
            EXPECT(g->FindMember(400) == nullptr);
        }
        if (auto c = chars.Find(400))
        {
            std::lock_guard cg(c->lock);
            EXPECT(c->guild_id == 0);
        }
        bool saw_remove = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kRemoveMember
                && c.char_id == 400 && c.guild_id == 8)
            { saw_remove = true; break; }
        }
        EXPECT(saw_remove);
    }

    // --- Scenario 33: DM_GUILDKICKOUT_REQ (W3a-17 fan-in) ----------
    //
    // Seed a fresh char (500 "Echo") into guild 8 manually, then
    // exercise the kickout fan-in. Same cleanup as LEAVE but no
    // leave_kind/leave_time on the wire. Earlier scenarios kicked
    // Bravo2 (10) and just left Carol (32), so we need a fresh
    // member for this scenario.
    SendFramed(peer1, ToUint16(MessageId::MW_ADDCHAR_ACK),
        AddCharBody(500, 0xECC00500));
    SendFramed(peer1, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        NameBody(500, 0xECC00500, "Echo"));
    for (int i = 0; i < 50; ++i)
    {
        if (chars.FindByName("Echo")) break;
        std::this_thread::sleep_for(10ms);
    }
    {
        auto g = guilds.Find(8);
        EXPECT(g != nullptr);
        if (g)
        {
            std::lock_guard gl(g->lock);
            tworldsvr::TGuildMember m;
            m.char_id  = 500;
            m.guild_id = 8;
            m.duty     = tworldsvr::guild::kDutyNone;
            m.name     = "Echo";
            g->members.push_back(std::move(m));
        }
        auto echo = chars.Find(500);
        if (echo) { std::lock_guard cg(echo->lock); echo->guild_id = 8; }
    }
    {
        auto g = guilds.Find(8);
        if (g)
        {
            std::lock_guard gl(g->lock);
            EXPECT(g->FindMember(500) != nullptr);  // pre-state
        }
    }
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);    // guild_id
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 500);  // char_id
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDKICKOUT_REQ), body);
    }
    for (int i = 0; i < 50; ++i)
    {
        bool gone = false;
        if (auto g = guilds.Find(8))
        {
            std::lock_guard gl(g->lock);
            gone = (g->FindMember(500) == nullptr);
        }
        if (gone) break;
        std::this_thread::sleep_for(10ms);
    }
    {
        if (auto g = guilds.Find(8))
        {
            std::lock_guard gl(g->lock);
            EXPECT(g->FindMember(500) == nullptr);
        }
        if (auto c = chars.Find(500))
        {
            std::lock_guard cg(c->lock);
            EXPECT(c->guild_id == 0);
        }
        bool saw_kickout = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kRemoveMember
                && c.char_id == 500 && c.guild_id == 8)
            { saw_kickout = true; break; }
        }
        EXPECT(saw_kickout);
    }

    // --- Scenario 34: DM_GUILDLEAVE_REQ idempotent on stale row ----
    //
    // Re-send the LEAVE for Carol after she's already gone.
    // Handler should treat it as a benign no-op (no crash, repo
    // still gets called because DB-authoritative).
    const auto pre_remove_count = [&]{
        std::size_t n = 0;
        for (const auto& c : fake_repo.Calls())
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kRemoveMember
                && c.char_id == 400 && c.guild_id == 8) ++n;
        return n;
    }();
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 400);
        tworldsvr::wire::WritePOD<std::uint8_t>(body,
            tworldsvr::guild::kLeaveSelf);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 1700003000);
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDLEAVE_REQ), body);
    }
    std::this_thread::sleep_for(80ms);
    {
        std::size_t post = 0;
        for (const auto& c : fake_repo.Calls())
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kRemoveMember
                && c.char_id == 400 && c.guild_id == 8) ++post;
        EXPECT(post == pre_remove_count + 1);  // repo got called again
        // Char 400 should still have no guild (was already cleared).
        if (auto c = chars.Find(400))
        {
            std::lock_guard cg(c->lock);
            EXPECT(c->guild_id == 0);
        }
    }

    // --- Scenario 35: MW_GUILDESTABLISH_ACK creates a new guild ----
    //
    // Add a fresh char (char 600 "Foxtrot") not in any guild, then
    // send MW_GUILDESTABLISH_ACK with a unique name. Expect:
    //   - repo->CreateGuild call (Call::kCreateGuild)
    //   - new guild lands in registry with chief = char 600
    //   - char 600's guild_id back-pointer set
    //   - MW_GUILDESTABLISH_REQ reply with kSuccess + bEstablish=1
    SendFramed(peer1, ToUint16(MessageId::MW_ADDCHAR_ACK),
        AddCharBody(600, 0xF0F0F0F0));
    SendFramed(peer1, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        NameBody(600, 0xF0F0F0F0, "Foxtrot"));
    for (int i = 0; i < 50; ++i)
    {
        if (chars.FindByName("Foxtrot")) break;
        std::this_thread::sleep_for(10ms);
    }
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 600);   // char_id
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 0xF0F0F0F0); // key
        tworldsvr::wire::WriteString(body, "FoxtrotGuild");
        SendFramed(peer1, ToUint16(MessageId::MW_GUILDESTABLISH_ACK),
            body);
    }
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDESTABLISH_REQ));
        tworldsvr::wire::Reader rr(body);
        std::uint32_t cid = 0, key = 0, gid = 0;
        std::uint8_t  result = 0, establish = 0;
        std::string   name;
        EXPECT(rr.Read(cid));          EXPECT(cid == 600);
        EXPECT(rr.Read(key));          EXPECT(key == 0xF0F0F0F0);
        EXPECT(rr.Read(result));       EXPECT(result == tworldsvr::guild::kSuccess);
        EXPECT(rr.Read(gid));          EXPECT(gid != 0);
        EXPECT(rr.ReadString(name));   EXPECT(name == "FoxtrotGuild");
        EXPECT(rr.Read(establish));    EXPECT(establish == 1);
    }
    {
        // Find the new guild by name (id is auto-assigned).
        std::uint32_t new_gid = 0;
        for (std::uint32_t probe = 1; probe < 1000 && new_gid == 0; ++probe)
        {
            if (auto g = guilds.Find(probe))
            {
                std::lock_guard gl(g->lock);
                if (g->name == "FoxtrotGuild") new_gid = probe;
            }
        }
        EXPECT(new_gid != 0);
        if (auto g = guilds.Find(new_gid))
        {
            std::lock_guard gl(g->lock);
            EXPECT(g->chief_char_id == 600);
            EXPECT(g->level         == 1);
            EXPECT(g->members.size() == 1);
            EXPECT(g->members[0].char_id == 600);
            EXPECT(g->members[0].duty    == tworldsvr::guild::kDutyChief);
        }
        if (auto c = chars.Find(600))
        {
            std::lock_guard cg(c->lock);
            EXPECT(c->guild_id == new_gid);
        }
        bool saw_create = false, saw_add_member = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kCreateGuild
                && c.guild_id == new_gid && c.a == 600)
            { saw_create = true; }
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kAddMember
                && c.guild_id == new_gid && c.char_id == 600
                && c.b == tworldsvr::guild::kDutyChief)
            { saw_add_member = true; }
        }
        EXPECT(saw_create);
        EXPECT(saw_add_member);
    }

    // --- Scenario 36: ESTABLISH rejected when char already in guild ---
    //
    // char 200 (Bob) is the chief of guild 8 from scenario 5+. Try
    // to create another guild → kHaveGuild reply, no new guild.
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 200);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 0xBEEF1111);
        tworldsvr::wire::WriteString(body, "SecondGuildBob");
        SendFramed(peer1, ToUint16(MessageId::MW_GUILDESTABLISH_ACK),
            body);
    }
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDESTABLISH_REQ));
        tworldsvr::wire::Reader rr(body);
        std::uint32_t cid = 0, key = 0, gid = 0;
        std::uint8_t  result = 0, establish = 0;
        std::string   name;
        EXPECT(rr.Read(cid));        EXPECT(cid == 200);
        EXPECT(rr.Read(key));
        EXPECT(rr.Read(result));     EXPECT(result == tworldsvr::guild::kHaveGuild);
        EXPECT(rr.Read(gid));        EXPECT(gid == 0);   // empty meta on failure
        EXPECT(rr.ReadString(name));
        EXPECT(rr.Read(establish));  EXPECT(establish == 1);
    }

    // --- Scenario 37: ESTABLISH rejected on duplicate name --------
    //
    // Add char 700 not in any guild, then try to create with
    // "FoxtrotGuild" which already exists from scenario 35. Fake
    // repo's CreateGuild scans by name and returns nullopt.
    SendFramed(peer1, ToUint16(MessageId::MW_ADDCHAR_ACK),
        AddCharBody(700, 0xA1B2C3D4));
    SendFramed(peer1, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        NameBody(700, 0xA1B2C3D4, "Golf"));
    for (int i = 0; i < 50; ++i)
    {
        if (chars.FindByName("Golf")) break;
        std::this_thread::sleep_for(10ms);
    }
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 700);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 0xA1B2C3D4);
        tworldsvr::wire::WriteString(body, "FoxtrotGuild");   // dup
        SendFramed(peer1, ToUint16(MessageId::MW_GUILDESTABLISH_ACK),
            body);
    }
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDESTABLISH_REQ));
        tworldsvr::wire::Reader rr(body);
        std::uint32_t cid = 0, key = 0, gid = 0;
        std::uint8_t  result = 0, establish = 0;
        std::string   name;
        EXPECT(rr.Read(cid));        EXPECT(cid == 700);
        EXPECT(rr.Read(key));
        EXPECT(rr.Read(result));
        EXPECT(result == tworldsvr::guild::kAlreadyGuildName);
        EXPECT(rr.Read(gid));        EXPECT(gid == 0);
        EXPECT(rr.ReadString(name));
        EXPECT(rr.Read(establish));  EXPECT(establish == 1);
    }
    {
        // Char 700 should remain guild-less after the rejection.
        if (auto c = chars.Find(700))
        {
            std::lock_guard cg(c->lock);
            EXPECT(c->guild_id == 0);
        }
    }

    // --- Scenario 38: DM_GUILDESTABLISH_ACK echo (W3a-20 vestigial) ---
    //
    // Wire-compat stub for hybrid legacy-DB deployments. Handler
    // accepts the packet, logs at info-level, doesn't mutate
    // state (W3a-18 already did the synchronous create). We just
    // verify the handler doesn't crash + the dispatch isn't
    // dropped (framer survives — follow-up packet processes).
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 0);          // bRet
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 100);       // guild_id
        tworldsvr::wire::WriteString(body, "EchoGuild");           // name
        tworldsvr::wire::WritePOD<std::int64_t>(body, 1700000000); // time_es
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 999);       // char_id
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 0xDEADBEEF);// key
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDESTABLISH_ACK),
            body);
    }
    // Sleep + framer-survival check via a known-good packet.
    std::this_thread::sleep_for(50ms);
    // Use a no-mutation query to confirm the connection still works.
    SendFramed(peer1, ToUint16(MessageId::RW_ENTERCHAR_REQ),
        EntercharBody(200, "Bob"));
    { auto [w, _] = ReadFramed(peer1); EXPECT(w == ToUint16(MessageId::RW_ENTERCHAR_ACK)); }

    // --- Scenario 39: DM_GUILDDISORGANIZATION_ACK echo (W3a-20) ---
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 999);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 0xDEADBEEF);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 100);
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 1);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 1700000000);
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDDISORGANIZATION_ACK),
            body);
    }
    std::this_thread::sleep_for(50ms);
    SendFramed(peer1, ToUint16(MessageId::RW_ENTERCHAR_REQ),
        EntercharBody(200, "Bob"));
    { auto [w, _] = ReadFramed(peer1); EXPECT(w == ToUint16(MessageId::RW_ENTERCHAR_ACK)); }

    // --- Scenario 40: DM_GUILDEXTINCTION_ACK echo (W3a-20) --------
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 100);
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 0);
        SendFramed(peer1, ToUint16(MessageId::DM_GUILDEXTINCTION_ACK),
            body);
    }
    std::this_thread::sleep_for(50ms);
    SendFramed(peer1, ToUint16(MessageId::RW_ENTERCHAR_REQ),
        EntercharBody(200, "Bob"));
    { auto [w, _] = ReadFramed(peer1); EXPECT(w == ToUint16(MessageId::RW_ENTERCHAR_ACK)); }

    // --- Scenario 41: DM_PVPRECORD_REQ persists N rows (W3a-21) ---
    //
    // Batched PvP record write — guild 8, char 200, 2 rows with
    // distinct dates + kill/die counts. Repo should receive one
    // LogPvPRecord call per row. Audit-log only — no in-memory
    // mirror to verify.
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);     // guild_id
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 200);   // member_id
        tworldsvr::wire::WritePOD<std::uint16_t>(body, 2);     // row count

        // Row 0
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 1700000000); // date
        tworldsvr::wire::WritePOD<std::uint16_t>(body, 5);          // kills
        tworldsvr::wire::WritePOD<std::uint16_t>(body, 2);          // dies
        for (std::size_t i = 0; i < tworldsvr::guild::kPvPEventCount; ++i)
            tworldsvr::wire::WritePOD<std::uint32_t>(body,
                static_cast<std::uint32_t>(100 + i));

        // Row 1
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 1700086400); // date
        tworldsvr::wire::WritePOD<std::uint16_t>(body, 3);          // kills
        tworldsvr::wire::WritePOD<std::uint16_t>(body, 1);          // dies
        for (std::size_t i = 0; i < tworldsvr::guild::kPvPEventCount; ++i)
            tworldsvr::wire::WritePOD<std::uint32_t>(body,
                static_cast<std::uint32_t>(200 + i));

        SendFramed(peer1, ToUint16(MessageId::DM_PVPRECORD_REQ), body);
    }
    for (int i = 0; i < 50; ++i)
    {
        std::size_t hits = 0;
        for (const auto& c : fake_repo.Calls())
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kLogPvPRecord
                && c.guild_id == 8 && c.char_id == 200) ++hits;
        if (hits >= 2) break;
        std::this_thread::sleep_for(10ms);
    }
    {
        // Verify both rows landed with the right per-row fields.
        // Call layout (set in fake repo): a=date, b=kill_count,
        // c=die_count, d=points[0], e=points[1].
        bool saw_row0 = false, saw_row1 = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind != tworldsvr::FakeGuildRepository::Call::Kind
                            ::kLogPvPRecord) continue;
            if (c.guild_id != 8 || c.char_id != 200) continue;
            if (c.a == 1700000000 && c.b == 5 && c.c == 2 &&
                c.d == 100 && c.e == 101) saw_row0 = true;
            if (c.a == 1700086400 && c.b == 3 && c.c == 1 &&
                c.d == 200 && c.e == 201) saw_row1 = true;
        }
        EXPECT(saw_row0);
        EXPECT(saw_row1);
    }

    // --- Scenario 42: DM_GUILDUPDATE_REQ overwrites scalars + lists ---
    //
    // Admin / bulk-load path. Overwrite guild 8's scalar columns
    // + the alliance / enemy ID lists. Verify the in-memory mirror
    // landed (W3a-25 added the list fields to TGuild) AND the
    // repo recorded the call with the lists.
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);     // guild_id
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 42);     // fame
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 7);      // gpoint
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 6);      // level
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 2);      // status
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 200);   // chief
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 555);   // exp
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 999);   // gi
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 1700009999); // time

        // 2 allies, 1 enemy — W3a-25 populates these into
        // TGuild.alliance_ids / .enemy_ids.
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 2);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 11);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 22);
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 1);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 33);

        SendFramed(peer1, ToUint16(MessageId::DM_GUILDUPDATE_REQ), body);
    }
    for (int i = 0; i < 50; ++i)
    {
        if (auto g = guilds.Find(8))
        {
            std::lock_guard gl(g->lock);
            if (g->fame == 42 && g->level == 6 &&
                g->alliance_ids.size() == 2)
                break;
        }
        std::this_thread::sleep_for(10ms);
    }
    {
        if (auto g = guilds.Find(8))
        {
            std::lock_guard gl(g->lock);
            EXPECT(g->fame          == 42);
            EXPECT(g->guild_points  == 7);
            EXPECT(g->level         == 6);
            EXPECT(g->status        == 2);
            EXPECT(g->chief_char_id == 200);
            EXPECT(g->exp           == 555);
            EXPECT(g->gi            == 999);
            EXPECT(g->disorg_time   == 1700009999);
            // W3a-25: alliance + enemy lists land in TGuild.
            EXPECT(g->alliance_ids.size() == 2);
            if (g->alliance_ids.size() == 2)
            {
                EXPECT(g->alliance_ids[0] == 11);
                EXPECT(g->alliance_ids[1] == 22);
            }
            EXPECT(g->enemy_ids.size() == 1);
            if (g->enemy_ids.size() == 1)
                EXPECT(g->enemy_ids[0] == 33);
        }
        // Repo got the call. Call layout (set in fake repo):
        // char_id=chief, a=fame, b=gpoint, c=level, d=status, e=time.
        bool saw_update = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kUpdateGuildFull
                && c.guild_id == 8 && c.char_id == 200
                && c.a == 42 && c.b == 7 && c.c == 6 && c.d == 2
                && c.e == 1700009999) { saw_update = true; break; }
        }
        EXPECT(saw_update);
        // W3a-25: Fake repo records the last-seen lists separately.
        const auto last_a = fake_repo.LastAllianceIds();
        const auto last_e = fake_repo.LastEnemyIds();
        EXPECT(last_a.size() == 2);
        if (last_a.size() == 2)
        {
            EXPECT(last_a[0] == 11);
            EXPECT(last_a[1] == 22);
        }
        EXPECT(last_e.size() == 1);
        if (last_e.size() == 1) EXPECT(last_e[0] == 33);
    }

    // --- Scenario 43: MW_GUILDPVPRECORD_ACK returns weekrecord ---
    //
    // Chief (char 200) opens the PvP-statistics panel. We pre-
    // populate one member's vRecord with today's row (W3a-28
    // semantics: weekrecord is derived from vRecord; setting
    // weekrecord directly would get wiped by the next
    // CalcWeekRecord call from scenario 44's fan-in). Mirror
    // the weekrecord too so the reader returns immediately
    // without needing a fan-in first.
    {
        auto g = guilds.Find(8);
        EXPECT(g != nullptr);
        if (g)
        {
            std::lock_guard gl(g->lock);
            for (auto& m : g->members)
            {
                if (m.char_id == 200)
                {
                    const std::int64_t today =
                        static_cast<std::int64_t>(std::time(nullptr)) /
                        tworldsvr::guild::kDaySec;
                    tworldsvr::TPvPDayRecord day;
                    day.day_index  = today;
                    day.kill_count = 7;
                    day.die_count  = 3;
                    day.points[0]  = 500;  // PVPE_KILL_H
                    day.points[1]  = 300;  // PVPE_KILL_E
                    day.points[2]  = 100;  // PVPE_KILL_L
                    m.vRecord.push_back(std::move(day));
                    m.weekrecord.kill_count = 7;
                    m.weekrecord.die_count  = 3;
                    m.weekrecord.points[0]  = 500;
                    m.weekrecord.points[1]  = 300;
                    m.weekrecord.points[2]  = 100;
                }
            }
        }
    }
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 200);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 0xBEEF1111);
        SendFramed(peer1,
            ToUint16(MessageId::MW_GUILDPVPRECORD_ACK), body);
    }
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDPVPRECORD_REQ));
        tworldsvr::wire::Reader rr(body);
        std::uint32_t cid = 0, key = 0;
        std::uint16_t mcount = 0;
        EXPECT(rr.Read(cid));      EXPECT(cid == 200);
        EXPECT(rr.Read(key));      EXPECT(key == 0xBEEF1111);
        EXPECT(rr.Read(mcount));
        // Guild 8's membership has been mutated across scenarios
        // (added/removed Carol, Bravo2, Echo at various points)
        // — just verify at least the chief shows up correctly.
        bool saw_chief = false;
        for (std::uint16_t i = 0; i < mcount; ++i)
        {
            std::uint32_t mid = 0;
            std::uint16_t kc = 0, dc = 0;
            std::array<std::uint32_t, 6> pts{};
            std::uint16_t last_kc = 0, last_dc = 0;
            std::array<std::uint32_t, 6> last_pts{};
            EXPECT(rr.Read(mid));
            EXPECT(rr.Read(kc));
            EXPECT(rr.Read(dc));
            for (std::size_t p = 0; p < pts.size(); ++p) EXPECT(rr.Read(pts[p]));
            EXPECT(rr.Read(last_kc));
            EXPECT(rr.Read(last_dc));
            for (std::size_t p = 0; p < last_pts.size(); ++p)
                EXPECT(rr.Read(last_pts[p]));
            // "Last record" slot is always zeros until per-day
            // vRecord fan-in ports.
            EXPECT(last_kc == 0);
            EXPECT(last_dc == 0);
            for (std::size_t p = 0; p < last_pts.size(); ++p)
                EXPECT(last_pts[p] == 0);
            if (mid == 200)
            {
                saw_chief = true;
                EXPECT(kc == 7);
                EXPECT(dc == 3);
                EXPECT(pts[0] == 500);
                EXPECT(pts[1] == 300);
                EXPECT(pts[2] == 100);
                EXPECT(pts[3] == 0);
            }
        }
        EXPECT(saw_chief);
    }

    // --- Scenario 44: MW_LOCALRECORD_ACK feeds W3a-23 reader ------
    //
    // Fan-in a war-result batch then read it back via
    // MW_GUILDPVPRECORD_ACK. Chief (char 200) already has
    // weekrecord pre-populated from scenario 43 with
    // kill=7/die=3/points[0..2]={500,300,100}; this scenario
    // sends ADDITIONAL deltas and verifies they accumulate.
    {
        std::vector<std::byte> body;
        // Header: win_guild_id, guild_point, guild_count
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);   // win_guild
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 1000);// guild_pt
        tworldsvr::wire::WritePOD<std::uint16_t>(body, 1);   // 1 guild

        // Guild 8 — 1 record for char 200
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);   // guild_id
        tworldsvr::wire::WritePOD<std::uint16_t>(body, 1);   // 1 record

        // Record: char 200, kill+=2, die+=1, points: +50 to bucket 0
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 200);
        tworldsvr::wire::WritePOD<std::uint16_t>(body, 2);
        tworldsvr::wire::WritePOD<std::uint16_t>(body, 1);
        for (std::size_t p = 0; p < tworldsvr::guild::kPvPEventCount; ++p)
            tworldsvr::wire::WritePOD<std::uint32_t>(body,
                p == 0 ? 50u : 0u);

        SendFramed(peer1, ToUint16(MessageId::MW_LOCALRECORD_ACK),
            body);
    }
    // Wait for accumulation to land (async write under guild lock).
    for (int i = 0; i < 50; ++i)
    {
        if (auto g = guilds.Find(8))
        {
            std::lock_guard gl(g->lock);
            bool found = false;
            for (const auto& m : g->members)
                if (m.char_id == 200 && m.weekrecord.kill_count == 9)
                { found = true; break; }
            if (found) break;
        }
        std::this_thread::sleep_for(10ms);
    }
    {
        // Verify in-memory accumulation: 7+2=9 kills, 3+1=4 dies,
        // points[0]=500+50=550 (others unchanged).
        if (auto g = guilds.Find(8))
        {
            std::lock_guard gl(g->lock);
            bool checked = false;
            for (const auto& m : g->members)
            {
                if (m.char_id == 200)
                {
                    EXPECT(m.weekrecord.kill_count == 9);
                    EXPECT(m.weekrecord.die_count  == 4);
                    EXPECT(m.weekrecord.points[0]  == 550);
                    EXPECT(m.weekrecord.points[1]  == 300);
                    EXPECT(m.weekrecord.points[2]  == 100);
                    checked = true;
                    break;
                }
            }
            EXPECT(checked);
        }
    }

    // Round-trip via W3a-23 reader: verify the accumulated
    // weekrecord lands on the wire.
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 200);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 0xBEEF1111);
        SendFramed(peer1,
            ToUint16(MessageId::MW_GUILDPVPRECORD_ACK), body);
    }
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDPVPRECORD_REQ));
        tworldsvr::wire::Reader rr(body);
        std::uint32_t cid = 0, key = 0;
        std::uint16_t mcount = 0;
        EXPECT(rr.Read(cid));
        EXPECT(rr.Read(key));
        EXPECT(rr.Read(mcount));
        bool saw_chief = false;
        for (std::uint16_t i = 0; i < mcount; ++i)
        {
            std::uint32_t mid = 0;
            std::uint16_t kc = 0, dc = 0;
            std::array<std::uint32_t, 6> pts{};
            std::uint16_t last_kc = 0, last_dc = 0;
            std::array<std::uint32_t, 6> last_pts{};
            EXPECT(rr.Read(mid));
            EXPECT(rr.Read(kc));
            EXPECT(rr.Read(dc));
            for (std::size_t p = 0; p < pts.size(); ++p) EXPECT(rr.Read(pts[p]));
            EXPECT(rr.Read(last_kc));
            EXPECT(rr.Read(last_dc));
            for (std::size_t p = 0; p < last_pts.size(); ++p)
                EXPECT(rr.Read(last_pts[p]));
            if (mid == 200)
            {
                saw_chief = true;
                EXPECT(kc == 9);
                EXPECT(dc == 4);
                EXPECT(pts[0] == 550);
                EXPECT(pts[1] == 300);
                EXPECT(pts[2] == 100);
            }
        }
        EXPECT(saw_chief);
    }

    // --- Scenario 45: MW_GUILDCABINETLIST_ACK returns empty stub --
    //
    // Open the guild storage UI. W3a-26 stub always replies with
    // max_cabinet from the guild + 0 items (item codec deferred).
    // Verify the reply lands with the right header and zero count.
    // Guild 8's max_cabinet was set to 10 by scenario 1's
    // GuildLoadBody; subsequent scenarios haven't changed it.
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 200);  // char_id
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 0xBEEF1111); // key
        SendFramed(peer1,
            ToUint16(MessageId::MW_GUILDCABINETLIST_ACK), body);
    }
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDCABINETLIST_REQ));
        tworldsvr::wire::Reader rr(body);
        std::uint32_t cid = 0, key = 0;
        std::uint8_t  max_cab = 0, count = 0;
        EXPECT(rr.Read(cid));     EXPECT(cid == 200);
        EXPECT(rr.Read(key));     EXPECT(key == 0xBEEF1111);
        EXPECT(rr.Read(max_cab)); EXPECT(max_cab == 10);
        EXPECT(rr.Read(count));   EXPECT(count == 0);
    }

    // --- Scenario 46: MW_GUILDPOINTLOG_ACK round-trip (W3a-27) ----
    //
    // Scenario 22 already fired a DM_GUILDPOINTREWARD_REQ for
    // guild 8 ("Bravo2" recipient, 250 points). W3a-27 added an
    // in-memory mirror onto TGuild.point_log inside that
    // handler, so the log reader should return that entry now.
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 200);  // char_id
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 0xBEEF1111); // key
        SendFramed(peer1,
            ToUint16(MessageId::MW_GUILDPOINTLOG_ACK), body);
    }
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDPOINTLOG_REQ));
        tworldsvr::wire::Reader rr(body);
        std::uint32_t cid = 0, key = 0;
        std::uint16_t entry_count = 0;
        EXPECT(rr.Read(cid));    EXPECT(cid == 200);
        EXPECT(rr.Read(key));    EXPECT(key == 0xBEEF1111);
        EXPECT(rr.Read(entry_count));
        EXPECT(entry_count >= 1);
        bool saw_bravo = false;
        for (std::uint16_t i = 0; i < entry_count; ++i)
        {
            std::int64_t  date = 0;
            std::string   name;
            std::uint32_t point = 0;
            EXPECT(rr.Read(date));
            EXPECT(rr.ReadString(name));
            EXPECT(rr.Read(point));
            if (name == "Bravo2" && point == 250)
            {
                saw_bravo = true;
                EXPECT(date > 0);   // any non-zero epoch second
            }
        }
        EXPECT(saw_bravo);
    }

    // --- Scenario 47: MW_GAINPVPPOINT_ACK guild gain (W3a-29) -----
    //
    // Guild-owned gain with PVP_TOTAL | PVP_USEABLE. TOTAL bumps
    // total + month; USEABLE bumps useable. Verify all three
    // banks + the persistence call. Guild 8's banks were last
    // set by scenario 17's DM_GUILDPVPOINT_REQ
    // (total=12345 useable=6789 month=1000); scenario 42's
    // DM_GUILDUPDATE_REQ then overwrote dwTime but NOT the pvp
    // banks, so the W3a-13 values still stand. We don't assert
    // absolute pre-state — just the delta.
    std::uint32_t pre_total = 0, pre_useable = 0, pre_month = 0;
    {
        if (auto g = guilds.Find(8))
        {
            std::lock_guard gl(g->lock);
            pre_total   = g->pvp_total_point;
            pre_useable = g->pvp_useable_point;
            pre_month   = g->pvp_month_point;
        }
    }
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint8_t>(body,
            tworldsvr::guild::kPvPOwnerGuild);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);    // owner_id
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 500);  // point
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 0);     // event
        tworldsvr::wire::WritePOD<std::uint8_t>(body,
            tworldsvr::guild::kPvPMaskTotal |
            tworldsvr::guild::kPvPMaskUseable);               // type
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 1);     // gain
        tworldsvr::wire::WriteString(body, "");               // name
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 0);     // klass
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 0);     // level
        SendFramed(peer1,
            ToUint16(MessageId::MW_GAINPVPPOINT_ACK), body);
    }
    for (int i = 0; i < 50; ++i)
    {
        if (auto g = guilds.Find(8))
        {
            std::lock_guard gl(g->lock);
            if (g->pvp_total_point == pre_total + 500) break;
        }
        std::this_thread::sleep_for(10ms);
    }
    {
        if (auto g = guilds.Find(8))
        {
            std::lock_guard gl(g->lock);
            EXPECT(g->pvp_total_point   == pre_total + 500);
            EXPECT(g->pvp_useable_point == pre_useable + 500);
            EXPECT(g->pvp_month_point   == pre_month + 500);
        }
        bool saw_persist = false;
        for (const auto& c : fake_repo.Calls())
        {
            if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                            ::kUpdatePvPoints
                && c.guild_id == 8 && c.a == pre_total + 500
                && c.b == pre_useable + 500 && c.c == pre_month + 500)
            { saw_persist = true; break; }
        }
        EXPECT(saw_persist);
    }

    // --- Scenario 48: MW_GAINPVPPOINT_ACK guild use (W3a-29) ------
    //
    // Guild-owned use with PVP_USEABLE only. useable shrinks;
    // total + month untouched. Saturates at 0 (not tested here —
    // we use a small delta well within the bank).
    std::uint32_t pre2_total = 0, pre2_useable = 0, pre2_month = 0;
    {
        if (auto g = guilds.Find(8))
        {
            std::lock_guard gl(g->lock);
            pre2_total   = g->pvp_total_point;
            pre2_useable = g->pvp_useable_point;
            pre2_month   = g->pvp_month_point;
        }
    }
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint8_t>(body,
            tworldsvr::guild::kPvPOwnerGuild);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 8);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 100);  // point
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 0);     // event
        tworldsvr::wire::WritePOD<std::uint8_t>(body,
            tworldsvr::guild::kPvPMaskUseable);               // type
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 0);     // gain=use
        tworldsvr::wire::WriteString(body, "");
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 0);
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 0);
        SendFramed(peer1,
            ToUint16(MessageId::MW_GAINPVPPOINT_ACK), body);
    }
    for (int i = 0; i < 50; ++i)
    {
        if (auto g = guilds.Find(8))
        {
            std::lock_guard gl(g->lock);
            if (g->pvp_useable_point == pre2_useable - 100) break;
        }
        std::this_thread::sleep_for(10ms);
    }
    {
        if (auto g = guilds.Find(8))
        {
            std::lock_guard gl(g->lock);
            EXPECT(g->pvp_useable_point == pre2_useable - 100);
            EXPECT(g->pvp_total_point   == pre2_total);   // unchanged
            EXPECT(g->pvp_month_point   == pre2_month);   // unchanged
        }
    }

    // --- Scenario 49: tactics WANTED add → registry + list (W3a-31) -
    //
    // Chief (char 200, guild 8, country 0) posts a tactics-wanted
    // entry with id=0 (auto-assign). Expect the ADD result reply
    // + a follow-up LIST refresh carrying the new posting.
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 200);  // char_id
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 0xBEEF1111); // key
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 0);    // id (auto)
        tworldsvr::wire::WriteString(body, "Need DPS");       // title
        tworldsvr::wire::WriteString(body, "War prep");       // text
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 7);     // day
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 10);    // min_level
        tworldsvr::wire::WritePOD<std::uint8_t>(body, 50);    // max_level
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 1000); // point
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 500);  // gold
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 0);    // silver
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 0);    // cooper
        SendFramed(peer1,
            ToUint16(MessageId::MW_GUILDTACTICSWANTEDADD_ACK), body);
    }
    {
        // ADD result reply.
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDTACTICSWANTEDADD_REQ));
        tworldsvr::wire::Reader rr(body);
        std::uint32_t cid = 0, key = 0; std::uint8_t result = 0;
        EXPECT(rr.Read(cid));    EXPECT(cid == 200);
        EXPECT(rr.Read(key));
        EXPECT(rr.Read(result)); EXPECT(result == tworldsvr::guild::kSuccess);
    }
    std::uint32_t posted_id = 0;
    {
        // LIST refresh — should carry exactly one entry.
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDTACTICSWANTEDLIST_REQ));
        tworldsvr::wire::Reader rr(body);
        std::uint32_t cid = 0, key = 0, count = 0;
        EXPECT(rr.Read(cid));
        EXPECT(rr.Read(key));
        EXPECT(rr.Read(count));  EXPECT(count == 1);
        if (count == 1)
        {
            std::uint32_t id = 0, gid = 0;
            std::string name, title, text;
            std::uint8_t day = 0, minl = 0, maxl = 0;
            std::uint32_t point = 0, gold = 0, silver = 0, cooper = 0;
            std::int64_t  end_time = 0;
            std::uint8_t  applied = 0;
            EXPECT(rr.Read(id));         posted_id = id;
            EXPECT(rr.Read(gid));        EXPECT(gid == 8);
            EXPECT(rr.ReadString(name));
            EXPECT(rr.ReadString(title)); EXPECT(title == "Need DPS");
            EXPECT(rr.ReadString(text));
            EXPECT(rr.Read(day));        EXPECT(day == 7);
            EXPECT(rr.Read(minl));       EXPECT(minl == 10);
            EXPECT(rr.Read(maxl));       EXPECT(maxl == 50);
            EXPECT(rr.Read(point));      EXPECT(point == 1000);
            EXPECT(rr.Read(gold));       EXPECT(gold == 500);
            EXPECT(rr.Read(silver));
            EXPECT(rr.Read(cooper));
            EXPECT(rr.Read(end_time));   EXPECT(end_time > 0);
            EXPECT(rr.Read(applied));    EXPECT(applied == 0);
        }
        EXPECT(posted_id != 0);
        EXPECT(guild_tactics_wanted.Size() == 1);
    }

    // --- Scenario 50: tactics WANTED list standalone (W3a-31) -----
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 200);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 0xBEEF1111);
        SendFramed(peer1,
            ToUint16(MessageId::MW_GUILDTACTICSWANTEDLIST_ACK), body);
    }
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDTACTICSWANTEDLIST_REQ));
        tworldsvr::wire::Reader rr(body);
        std::uint32_t cid = 0, key = 0, count = 0;
        EXPECT(rr.Read(cid));
        EXPECT(rr.Read(key));
        EXPECT(rr.Read(count)); EXPECT(count == 1);
    }

    // --- Scenario 51: tactics WANTED del → registry empty (W3a-31) -
    {
        std::vector<std::byte> body;
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 200);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, 0xBEEF1111);
        tworldsvr::wire::WritePOD<std::uint32_t>(body, posted_id);
        SendFramed(peer1,
            ToUint16(MessageId::MW_GUILDTACTICSWANTEDDEL_ACK), body);
    }
    {
        // DEL result reply.
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDTACTICSWANTEDDEL_REQ));
        tworldsvr::wire::Reader rr(body);
        std::uint32_t cid = 0, key = 0; std::uint8_t result = 0;
        EXPECT(rr.Read(cid));
        EXPECT(rr.Read(key));
        EXPECT(rr.Read(result)); EXPECT(result == tworldsvr::guild::kSuccess);
    }
    {
        // LIST refresh — now empty.
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_GUILDTACTICSWANTEDLIST_REQ));
        tworldsvr::wire::Reader rr(body);
        std::uint32_t cid = 0, key = 0, count = 0;
        EXPECT(rr.Read(cid));
        EXPECT(rr.Read(key));
        EXPECT(rr.Read(count)); EXPECT(count == 0);
        EXPECT(guild_tactics_wanted.Size() == 0);
    }

    boost::system::error_code ec;
    peer1.shutdown(tcp::socket::shutdown_both, ec);
    peer1.close(ec);

    std::this_thread::sleep_for(60ms);
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_guild_mut_handlers (51 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_guild_mut_handlers (%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
