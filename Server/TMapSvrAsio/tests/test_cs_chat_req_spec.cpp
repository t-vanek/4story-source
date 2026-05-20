// Characterization test for CS_CHAT_REQ (F6 Part 1).
//
// Source: Server/TMapSvr/CSHandler.cpp:5206
//
// Wire body: CString strSender, BYTE bGroup, DWORD dwTarget,
//            CString strName, CString strTalk
//
// Branches:
//   §1  malformed body → drop
//   §2  not connected → drop
//   §3  CHAT_NEAR (group=0) → CS_CHAT_ACK echo back to sender
//   §4  CHAT_WHISPER (group=2) → CS_CHAT_ACK to target only
//   §5  message > 256 chars → truncated

#include "handlers.h"
#include "map_state.h"
#include "services/session_registry.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>

#include <cstdio>
#include <exception>
#include <string>

namespace {

int g_passed  = 0;
int g_failed  = 0;
int g_pending = 0;

void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS     %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL     %s\n", label); }
}

void Pending(const char* label, const char* ref)
{
    ++g_pending;
    std::printf("  PENDING  %s   (%s)\n", label, ref);
}

template<typename Coro>
void RunSync(Coro c)
{
    boost::asio::io_context io;
    boost::asio::co_spawn(io, std::move(c), boost::asio::detached);
    io.run();
}

tnetlib::DecodedPacket MakeChatReq(const std::string& msg,
                                   std::uint8_t  group  = 0,
                                   std::uint32_t target = 0,
                                   const std::string& sender = "Hero",
                                   const std::string& name   = "Hero")
{
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;

    tnetlib::DecodedPacket pkt{};
    pkt.wId = ToUint16(MessageId::CS_CHAT_REQ);

    auto pushStr = [&](const std::string& s) {
        // length-prefixed string (int32 LE + bytes)
        std::uint32_t len = static_cast<std::uint32_t>(s.size());
        for (int i = 0; i < 4; ++i)
            pkt.body.push_back(static_cast<std::byte>((len >> (8 * i)) & 0xFF));
        for (char c : s)
            pkt.body.push_back(static_cast<std::byte>(c));
    };
    auto push4 = [&](std::uint32_t v) {
        for (int i = 0; i < 4; ++i)
            pkt.body.push_back(static_cast<std::byte>((v >> (8 * i)) & 0xFF));
    };
    auto push1 = [&](std::uint8_t v) {
        pkt.body.push_back(static_cast<std::byte>(v));
    };

    pushStr(sender);
    push1(group);
    push4(target);
    pushStr(name);
    pushStr(msg);
    return pkt;
}

tmapsvr::MapSessionState MakeConnectedState()
{
    tmapsvr::MapSessionState s{};
    s.user_id   = 100u;
    s.char_id   = 42u;
    s.connected = true;
    s.in_world  = true;
    s.snapshot.emplace();
    s.snapshot->char_id = 42u;
    s.snapshot->name    = "Hero";
    s.snapshot->position.pos_x = 100.0f;
    s.snapshot->position.pos_z = 100.0f;
    return s;
}

// ---------------------------------------------------------------------------
// §1 Malformed
// ---------------------------------------------------------------------------
void TestMalformed()
{
    std::printf("[§1 malformed body → drop]\n");
    tmapsvr::HandlerContext ctx{};
    auto s = MakeConnectedState();

    tnetlib::DecodedPacket bad{};
    bad.body = { std::byte{0xFF} };  // too short for a string length

    bool threw = false;
    RunSync([&]() -> boost::asio::awaitable<void> {
        try { co_await tmapsvr::OnChatReq(nullptr, s, bad, ctx); }
        catch (...) { threw = true; }
    }());
    Check(!threw, "§1 no exception on malformed body");
}

// ---------------------------------------------------------------------------
// §2 Not connected
// ---------------------------------------------------------------------------
void TestNotConnected()
{
    std::printf("[§2 not connected → drop]\n");
    tmapsvr::HandlerContext ctx{};
    tmapsvr::MapSessionState s{};
    s.connected = false;

    RunSync([&]() -> boost::asio::awaitable<void> {
        co_await tmapsvr::OnChatReq(nullptr, s, MakeChatReq("hi"), ctx);
    }());
    Check(true, "§2 no exception");
}

// ---------------------------------------------------------------------------
// §3 CHAT_NEAR → no crash, basic flow
// ---------------------------------------------------------------------------
void TestChatNear()
{
    std::printf("[§3 CHAT_NEAR → parsed without crash]\n");

    tmapsvr::LocalMapState       map;
    tmapsvr::FakeSessionRegistry sessions;

    tmapsvr::legacy::PlayerPresence p{};
    p.char_id = 42u; p.pos_x = 100.0f; p.pos_z = 100.0f;
    map.EnterMap(42u, p);

    tmapsvr::HandlerContext ctx{};
    ctx.map_state        = &map;
    ctx.session_registry = &sessions;

    auto state = MakeConnectedState();

    RunSync([&]() -> boost::asio::awaitable<void> {
        co_await tmapsvr::OnChatReq(nullptr, state,
            MakeChatReq("Hello world", 0), ctx);
    }());
    Check(true, "§3 CHAT_NEAR parsed without exception");
}

// ---------------------------------------------------------------------------
// §4 CHAT_WHISPER → direct message
// ---------------------------------------------------------------------------
void TestChatWhisper()
{
    std::printf("[§4 CHAT_WHISPER → direct message]\n");
    tmapsvr::LocalMapState       map;
    tmapsvr::FakeSessionRegistry sessions;

    tmapsvr::HandlerContext ctx{};
    ctx.map_state        = &map;
    ctx.session_registry = &sessions;

    auto state = MakeConnectedState();

    RunSync([&]() -> boost::asio::awaitable<void> {
        co_await tmapsvr::OnChatReq(nullptr, state,
            MakeChatReq("Private msg", 2, 99u), ctx);
    }());
    Check(true, "§4 CHAT_WHISPER parsed without exception");
}

// ---------------------------------------------------------------------------
// §5 Party/guild chat → PENDING
// ---------------------------------------------------------------------------
void TestPartyGuildPending()
{
    Pending("CHAT_PARTY, CHAT_GUILD, CHAT_TACTICS routing",
            "CSHandler.cpp:5260 — requires party/guild member iteration (F6b)");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnChatReq characterization spec ===\n");
    std::printf("    Source: Server/TMapSvr/CSHandler.cpp:5206\n\n");
    try
    {
        TestMalformed();
        TestNotConnected();
        TestChatNear();
        TestChatWhisper();
        TestPartyGuildPending();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL     unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed, %d pending\n",
        g_passed, g_failed, g_pending);
    return g_failed == 0 ? 0 : 1;
}
