// Unit test: OnMW_ENTERSVR_REQ — the inbound World→Map enter-server
// handler that brings a char fully resident and replies MW_ENTERSVR_ACK.
//
// Drives the handler directly against fakes (no sockets, no DB): a
// capturing IWorldClient records the outbound packet, a FakePlayerService
// / InMemoryCharStateStore supply the char identity, and the channel
// comes from a bound InMemoryChannelPresence entry. This proves the
// inbound leg — test_world_senders only proves the EncodeEnterSvrAck
// byte layout, and test_world_handshake only drives the ACK manually
// from the map's driver; nothing yet exercises the REQ→ACK reply path.
//
// Scenarios:
//   A. identity in the player service only  → loads it, caches it, ACK SUCCESS
//   B. identity already in the char cache   → cache hit (no player service), ACK SUCCESS
//   C. identity nowhere                      → ACK CN_INTERNAL carrying just the char id

#include "handlers.h"
#include "handlers_world.h"
#include "services/char_state_store.h"
#include "services/channel_presence.h"
#include "services/fake_player_service.h"
#include "services/world_client.h"
#include "domain/character.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>

#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {

int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

// Result codes mirror the file-local CN_* constants in handlers_world.cpp
// (the legacy header hasn't been recovered; 0 = success is the modern
// "apply this identity" contract TWorld's OnEnterSvrAck reads).
constexpr std::uint8_t kCnSuccess  = 0;
constexpr std::uint8_t kCnInternal = 3;

// Capturing world client — records the (wId, body) of the last SendPacket
// so the test can decode the ack. IsConnected() is true so the handler
// takes the real send path rather than the "peer down" early-out.
class CapturingWorldClient final : public tmapsvr::IWorldClient
{
public:
    boost::asio::awaitable<bool>
        SendPacket(std::uint16_t wId, std::vector<std::byte> body) override
    {
        last_id   = wId;
        last_body = std::move(body);
        ++sends;
        co_return true;
    }

    bool IsConnected() const override { return true; }

    std::uint16_t          last_id = 0;
    std::vector<std::byte> last_body;
    int                    sends = 0;
};

// Decoded MW_ENTERSVR_ACK — field order mirrors EncodeEnterSvrAck
// (services/world_senders.cpp). We read every field so the reader stays
// aligned, then assert on the ones that matter.
struct EnterAck
{
    bool          ok = false;
    std::uint32_t char_id = 0, key = 0;
    std::string   name;
    std::uint8_t  level = 0, real_sex = 0, klass = 0, race = 0, sex = 0,
                  face = 0, hair = 0, helmet_hide = 0, country = 0, aid = 0;
    std::uint32_t region = 0;
    std::uint8_t  channel = 0;
    std::uint16_t map_id = 0;
    float         pos_x = 0, pos_y = 0, pos_z = 0;
    std::uint8_t  logout = 0, save = 0, result = 0xFF;
    std::uint16_t title = 0;
    std::uint32_t rank = 0, user_ip = 0;
};

bool Decode(const std::vector<std::byte>& b, EnterAck& a)
{
    tmapsvr::wire::Reader r(b.data(), b.size());
    if (!r.Read(a.char_id) || !r.Read(a.key) || !r.ReadString(a.name))
        return false;
    if (!r.Read(a.level) || !r.Read(a.real_sex) || !r.Read(a.klass) ||
        !r.Read(a.race)  || !r.Read(a.sex)      || !r.Read(a.face)  ||
        !r.Read(a.hair)  || !r.Read(a.helmet_hide) ||
        !r.Read(a.country) || !r.Read(a.aid))
        return false;
    if (!r.Read(a.region) || !r.Read(a.channel) || !r.Read(a.map_id))
        return false;
    if (!r.Read(a.pos_x) || !r.Read(a.pos_y) || !r.Read(a.pos_z))
        return false;
    if (!r.Read(a.logout) || !r.Read(a.save) || !r.Read(a.result))
        return false;
    if (!r.Read(a.title) || !r.Read(a.rank) || !r.Read(a.user_ip))
        return false;
    a.ok = true;
    return true;
}

// Build the 9-byte MW_ENTERSVR_REQ body and drive the handler to
// completion on a private io_context.
void RunEnter(std::uint8_t dbload, std::uint32_t char_id, std::uint32_t key,
              const tmapsvr::HandlerContext& ctx)
{
    std::vector<std::byte> body;
    tmapsvr::wire::WritePOD<std::uint8_t> (body, dbload);
    tmapsvr::wire::WritePOD<std::uint32_t>(body, char_id);
    tmapsvr::wire::WritePOD<std::uint32_t>(body, key);

    boost::asio::io_context io;
    boost::asio::co_spawn(
        io, tmapsvr::OnMWEnterSvrReq(std::move(body), ctx),
        boost::asio::detached);
    io.run();
}

tmapsvr::CharSnapshot MakeHero(std::uint32_t char_id)
{
    tmapsvr::CharSnapshot s;
    s.dwCharID = char_id;
    s.szNAME   = "Hero";
    s.bLevel   = 77;
    s.bClass   = 3;
    s.bRace    = 2;
    s.wMapID   = 60;
    s.fPosX    = 100.5f;
    s.fPosY    = 7.0f;
    s.fPosZ    = -50.25f;
    return s;
}

} // namespace

int main()
{
    using tnetlib::protocol::MessageId;

    constexpr std::uint32_t kCharId = 0xABCDEF01;
    constexpr std::uint32_t kKey    = 0x12345678;
    constexpr std::uint8_t  kChannel = 1;

    // --- Scenario A: load through the player service -------------------
    // char_state starts empty; the handler must pull the snapshot from
    // the player service, cache it, and ACK SUCCESS with the identity.
    {
        CapturingWorldClient        wc;
        tmapsvr::FakePlayerService  players;
        tmapsvr::InMemoryChannelPresence presence;
        tmapsvr::InMemoryCharStateStore  char_state;
        players.Add(MakeHero(kCharId));
        presence.Bind(kCharId, kChannel, nullptr);   // channel echo source

        tmapsvr::HandlerContext ctx;
        ctx.world_client   = &wc;
        ctx.player_service = &players;
        ctx.presence       = &presence;
        ctx.char_state     = &char_state;

        RunEnter(/*dbload=*/1, kCharId, kKey, ctx);

        EXPECT(wc.sends == 1);
        EXPECT(wc.last_id == static_cast<std::uint16_t>(MessageId::MW_ENTERSVR_ACK));
        EnterAck a;
        EXPECT(Decode(wc.last_body, a));
        EXPECT(a.result  == kCnSuccess);
        EXPECT(a.char_id == kCharId);
        EXPECT(a.key     == kKey);
        EXPECT(a.name    == "Hero");
        EXPECT(a.level   == 77);
        EXPECT(a.klass   == 3);
        EXPECT(a.race    == 2);
        EXPECT(a.channel == kChannel);
        EXPECT(a.map_id  == 60);
        EXPECT(a.pos_x   == 100.5f);
        EXPECT(a.pos_z   == -50.25f);
        // The load path must have cached the snapshot for the teardown
        // SaveChar hook (and so a later enter is a pure cache hit).
        EXPECT(char_state.Get(kCharId).has_value());
    }

    // --- Scenario B: cache hit, no player service ----------------------
    // The snapshot is already live in char_state; the handler must reply
    // from cache without any player service configured.
    {
        CapturingWorldClient             wc;
        tmapsvr::InMemoryChannelPresence presence;
        tmapsvr::InMemoryCharStateStore  char_state;
        char_state.Store(kCharId, MakeHero(kCharId));
        presence.Bind(kCharId, kChannel, nullptr);

        tmapsvr::HandlerContext ctx;
        ctx.world_client = &wc;
        ctx.presence     = &presence;
        ctx.char_state   = &char_state;
        // ctx.player_service deliberately left null.

        RunEnter(/*dbload=*/0, kCharId, kKey, ctx);

        EXPECT(wc.sends == 1);
        EnterAck a;
        EXPECT(Decode(wc.last_body, a));
        EXPECT(a.result  == kCnSuccess);
        EXPECT(a.char_id == kCharId);
        EXPECT(a.name    == "Hero");
        EXPECT(a.level   == 77);
        EXPECT(a.channel == kChannel);
    }

    // --- Scenario C: identity nowhere → CN_INTERNAL --------------------
    // No cache, empty player service. The handler must still ACK so
    // TWorld unwinds the pending enter instead of timing out.
    {
        CapturingWorldClient            wc;
        tmapsvr::FakePlayerService      players;     // empty
        tmapsvr::InMemoryCharStateStore char_state;  // empty

        tmapsvr::HandlerContext ctx;
        ctx.world_client   = &wc;
        ctx.player_service = &players;
        ctx.char_state     = &char_state;
        // no presence → channel falls back to 0.

        RunEnter(/*dbload=*/1, kCharId, kKey, ctx);

        EXPECT(wc.sends == 1);
        EXPECT(wc.last_id == static_cast<std::uint16_t>(MessageId::MW_ENTERSVR_ACK));
        EnterAck a;
        EXPECT(Decode(wc.last_body, a));
        EXPECT(a.result  == kCnInternal);
        EXPECT(a.char_id == kCharId);   // error ack still identifies the char
        EXPECT(a.key     == kKey);
        EXPECT(a.name.empty());
        EXPECT(a.channel == 0);
    }

    // --- Scenario D: short body is dropped silently (no ack) -----------
    {
        CapturingWorldClient wc;
        tmapsvr::HandlerContext ctx;
        ctx.world_client = &wc;

        std::vector<std::byte> body;             // 0 bytes — truncated REQ
        tmapsvr::wire::WritePOD<std::uint8_t>(body, 1);   // only bDBLoad

        boost::asio::io_context io;
        boost::asio::co_spawn(
            io, tmapsvr::OnMWEnterSvrReq(std::move(body), ctx),
            boost::asio::detached);
        io.run();

        EXPECT(wc.sends == 0);   // malformed REQ → no reply
    }

    if (g_fails == 0)
        std::printf("test_world_enter: load + cache-hit + no-char + short-body "
                    "OK (char=0x%08X)\n", kCharId);
    return g_fails == 0 ? 0 : 1;
}
