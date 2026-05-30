// Unit test: OnMW_ENTERCHAR_REQ — the World→Map per-connection entry
// handshake. TWorld pushes the char's cluster-state composite and waits
// for the map to confirm the connection is ready (its CheckMainCon loop
// blocks until every con ACKs). This proves the map replies
// MW_ENTERCHAR_ACK (char_id, key) and tracks the composite spawn
// position when the char is resident.
//
// Drives the handler directly against fakes (no sockets, no DB): a
// capturing IWorldClient records the outbound packet; an
// InMemoryCharStateStore observes the position update.
//
// Scenarios:
//   A. resident char  → ACK(char_id,key) + char_state position updated
//   B. non-resident   → still ACK (ready handshake), no crash
//   C. short body      → no ACK

#include "handlers.h"
#include "handlers_world.h"
#include "services/char_state_store.h"
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

// Capturing world client — records the last SendPacket so the test can
// decode the ack. IsConnected() true so the handler takes the send path.
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

// Build the MW_ENTERCHAR_REQ composite prefix the handler reads:
// char_id, key, start_act, name, map_id, pos (the fat guild/party/...
// tail past this point is ignored by the ready handshake).
std::vector<std::byte> MakeEnterCharReq(std::uint32_t char_id,
                                        std::uint32_t key,
                                        const std::string& name,
                                        std::uint16_t map_id,
                                        float px, float py, float pz)
{
    std::vector<std::byte> b;
    tmapsvr::wire::WritePOD<std::uint32_t>(b, char_id);
    tmapsvr::wire::WritePOD<std::uint32_t>(b, key);
    tmapsvr::wire::WritePOD<std::uint8_t> (b, 0);       // start_act
    tmapsvr::wire::WriteString            (b, name);
    tmapsvr::wire::WritePOD<std::uint16_t>(b, map_id);
    tmapsvr::wire::WritePOD<float>        (b, px);
    tmapsvr::wire::WritePOD<float>        (b, py);
    tmapsvr::wire::WritePOD<float>        (b, pz);
    return b;
}

void RunEnterChar(std::vector<std::byte> body, const tmapsvr::HandlerContext& ctx)
{
    boost::asio::io_context io;
    boost::asio::co_spawn(
        io, tmapsvr::OnMWEnterCharReq(std::move(body), ctx),
        boost::asio::detached);
    io.run();
}

// Decode MW_ENTERCHAR_ACK = char_id, key (8 bytes).
bool DecodeAck(const std::vector<std::byte>& b, std::uint32_t& char_id,
               std::uint32_t& key)
{
    tmapsvr::wire::Reader r(b.data(), b.size());
    return r.Read(char_id) && r.Read(key);
}

} // namespace

int main()
{
    using tnetlib::protocol::MessageId;

    constexpr std::uint32_t kCharId = 0xABCDEF01;
    constexpr std::uint32_t kKey    = 0x12345678;

    // --- Scenario A: resident char → ACK + position applied -----------
    {
        CapturingWorldClient            wc;
        tmapsvr::InMemoryCharStateStore char_state;
        tmapsvr::CharSnapshot s;
        s.dwCharID = kCharId;
        s.szNAME   = "Hero";
        s.wMapID   = 1;            // stale; the composite should overwrite
        char_state.Store(kCharId, s);

        tmapsvr::HandlerContext ctx;
        ctx.world_client = &wc;
        ctx.char_state   = &char_state;

        RunEnterChar(
            MakeEnterCharReq(kCharId, kKey, "Hero", /*map=*/60,
                             100.5f, 7.0f, -50.25f),
            ctx);

        EXPECT(wc.sends == 1);
        EXPECT(wc.last_id == static_cast<std::uint16_t>(MessageId::MW_ENTERCHAR_ACK));
        EXPECT(wc.last_body.size() == 8);
        std::uint32_t ack_char = 0, ack_key = 0;
        EXPECT(DecodeAck(wc.last_body, ack_char, ack_key));
        EXPECT(ack_char == kCharId);
        EXPECT(ack_key  == kKey);

        // The composite's World-authoritative spawn position landed on
        // the resident snapshot.
        const auto snap = char_state.Get(kCharId);
        EXPECT(snap.has_value());
        if (snap)
        {
            EXPECT(snap->wMapID == 60);
            EXPECT(snap->fPosX  == 100.5f);
            EXPECT(snap->fPosZ  == -50.25f);
        }
    }

    // --- Scenario B: non-resident char → still ACKs -------------------
    // Entry can arrive before the snapshot is cached; the ready
    // handshake must complete regardless so CheckMainCon isn't stalled.
    {
        CapturingWorldClient            wc;
        tmapsvr::InMemoryCharStateStore char_state;   // empty

        tmapsvr::HandlerContext ctx;
        ctx.world_client = &wc;
        ctx.char_state   = &char_state;

        RunEnterChar(
            MakeEnterCharReq(kCharId, kKey, "Hero", 60, 1.f, 2.f, 3.f), ctx);

        EXPECT(wc.sends == 1);
        std::uint32_t ack_char = 0, ack_key = 0;
        EXPECT(DecodeAck(wc.last_body, ack_char, ack_key));
        EXPECT(ack_char == kCharId);
        EXPECT(ack_key  == kKey);
        EXPECT(!char_state.Get(kCharId).has_value());   // nothing materialized
    }

    // --- Scenario C: short body (< 8 bytes) → no ack ------------------
    {
        CapturingWorldClient wc;
        tmapsvr::HandlerContext ctx;
        ctx.world_client = &wc;

        std::vector<std::byte> body;
        tmapsvr::wire::WritePOD<std::uint32_t>(body, kCharId);   // only char_id

        RunEnterChar(std::move(body), ctx);
        EXPECT(wc.sends == 0);
    }

    if (g_fails == 0)
        std::printf("test_world_enterchar: resident + non-resident + short-body "
                    "OK (char=0x%08X)\n", kCharId);
    return g_fails == 0 ? 0 : 1;
}
