// Unit test: OnMW_CHECKMAIN_REQ — TWorld asks each candidate map "do you
// own the cell this char stands in?"; only the owner replies
// MW_CHECKMAIN_ACK(char_id, key) so the cluster can settle the
// authoritative main session.
//
// The modern map answers when the char is resident here (loaded snapshot
// or live session) — the in-process stand-in for the legacy IsMainCell
// cell-ownership check. Driven against a capturing IWorldClient.

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

// MW_CHECKMAIN_REQ body: char_id, key, channel, map_id, pos x/y/z.
std::vector<std::byte> MakeCheckMainReq(std::uint32_t char_id,
                                        std::uint32_t key,
                                        std::uint8_t channel,
                                        std::uint16_t map_id)
{
    std::vector<std::byte> b;
    tmapsvr::wire::WritePOD<std::uint32_t>(b, char_id);
    tmapsvr::wire::WritePOD<std::uint32_t>(b, key);
    tmapsvr::wire::WritePOD<std::uint8_t> (b, channel);
    tmapsvr::wire::WritePOD<std::uint16_t>(b, map_id);
    tmapsvr::wire::WritePOD<float>        (b, 10.0f);
    tmapsvr::wire::WritePOD<float>        (b, 0.0f);
    tmapsvr::wire::WritePOD<float>        (b, 20.0f);
    return b;
}

void Run(std::vector<std::byte> body, const tmapsvr::HandlerContext& ctx)
{
    boost::asio::io_context io;
    boost::asio::co_spawn(
        io, tmapsvr::OnMWCheckMainReq(std::move(body), ctx),
        boost::asio::detached);
    io.run();
}

} // namespace

int main()
{
    using tnetlib::protocol::MessageId;

    constexpr std::uint32_t kCharId = 0xABCDEF01;
    constexpr std::uint32_t kKey    = 0x12345678;

    // --- resident char → MW_CHECKMAIN_ACK -----------------------------
    {
        CapturingWorldClient            wc;
        tmapsvr::InMemoryCharStateStore char_state;
        tmapsvr::CharSnapshot s;
        s.dwCharID = kCharId;
        char_state.Store(kCharId, s);

        tmapsvr::HandlerContext ctx;
        ctx.world_client = &wc;
        ctx.char_state   = &char_state;

        Run(MakeCheckMainReq(kCharId, kKey, /*ch=*/1, /*map=*/60), ctx);

        EXPECT(wc.sends == 1);
        EXPECT(wc.last_id == static_cast<std::uint16_t>(MessageId::MW_CHECKMAIN_ACK));
        EXPECT(wc.last_body.size() == 8);
        tmapsvr::wire::Reader r(wc.last_body.data(), wc.last_body.size());
        std::uint32_t ack_char = 0, ack_key = 0;
        EXPECT(r.Read(ack_char) && r.Read(ack_key));
        EXPECT(ack_char == kCharId);
        EXPECT(ack_key  == kKey);
    }

    // --- non-resident char → no ack -----------------------------------
    {
        CapturingWorldClient            wc;
        tmapsvr::InMemoryCharStateStore char_state;   // empty

        tmapsvr::HandlerContext ctx;
        ctx.world_client = &wc;
        ctx.char_state   = &char_state;

        Run(MakeCheckMainReq(kCharId, kKey, 1, 60), ctx);
        EXPECT(wc.sends == 0);
    }

    // --- short body → no ack ------------------------------------------
    {
        CapturingWorldClient            wc;
        tmapsvr::InMemoryCharStateStore char_state;
        char_state.Store(kCharId, tmapsvr::CharSnapshot{});

        tmapsvr::HandlerContext ctx;
        ctx.world_client = &wc;
        ctx.char_state   = &char_state;

        std::vector<std::byte> body;
        tmapsvr::wire::WritePOD<std::uint32_t>(body, kCharId);   // char_id only
        Run(std::move(body), ctx);
        EXPECT(wc.sends == 0);
    }

    if (g_fails == 0)
        std::printf("test_world_checkmain: resident + non-resident + short-body "
                    "OK (char=0x%08X)\n", kCharId);
    return g_fails == 0 ? 0 : 1;
}
