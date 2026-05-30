// Unit test: OnMW_ROUTELIST_REQ — TWorld asks the map to resolve a set
// of cluster server ids to their live endpoints; the map answers
// MW_ROUTE_ACK with the resolved (ip, port, server_id) tuples.
//
// Driven against a capturing IWorldClient + a FakeServerRouteResolver.
// Unknown ids are omitted; with no resolver the map replies an empty
// route list (the production SOCI resolver is a follow-up).

#include "handlers.h"
#include "handlers_world.h"
#include "services/server_route_resolver.h"
#include "services/world_client.h"
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

// MW_ROUTELIST_REQ body: char_id, key, count, count×server_id.
std::vector<std::byte> MakeRouteListReq(std::uint32_t char_id,
                                        std::uint32_t key,
                                        const std::vector<std::uint8_t>& ids)
{
    std::vector<std::byte> b;
    tmapsvr::wire::WritePOD<std::uint32_t>(b, char_id);
    tmapsvr::wire::WritePOD<std::uint32_t>(b, key);
    tmapsvr::wire::WritePOD<std::uint8_t> (b, static_cast<std::uint8_t>(ids.size()));
    for (const auto id : ids)
        tmapsvr::wire::WritePOD<std::uint8_t>(b, id);
    return b;
}

void Run(std::vector<std::byte> body, const tmapsvr::HandlerContext& ctx)
{
    boost::asio::io_context io;
    boost::asio::co_spawn(
        io, tmapsvr::OnMWRouteListReq(std::move(body), ctx),
        boost::asio::detached);
    io.run();
}

} // namespace

int main()
{
    using tnetlib::protocol::MessageId;

    constexpr std::uint32_t kCharId = 0xABCDEF01;
    constexpr std::uint32_t kKey    = 0x12345678;

    // --- resolver hit (one id unknown, omitted) -----------------------
    {
        CapturingWorldClient            wc;
        tmapsvr::FakeServerRouteResolver resolver;
        resolver.Add(0x05, 0x0100007F, 5815);   // 127.0.0.1
        resolver.Add(0x42, 0x0A00000A, 7016);   // 10.0.0.10

        tmapsvr::HandlerContext ctx;
        ctx.world_client   = &wc;
        ctx.route_resolver = &resolver;
        ctx.expected_group = 1;

        // 0x07 is unknown → dropped from the result.
        Run(MakeRouteListReq(kCharId, kKey, { 0x05, 0x07, 0x42 }), ctx);

        EXPECT(wc.sends == 1);
        EXPECT(wc.last_id == static_cast<std::uint16_t>(MessageId::MW_ROUTE_ACK));

        tmapsvr::wire::Reader r(wc.last_body.data(), wc.last_body.size());
        std::uint32_t ack_char = 0, ack_key = 0;
        std::uint8_t  count = 0;
        EXPECT(r.Read(ack_char) && r.Read(ack_key) && r.Read(count));
        EXPECT(ack_char == kCharId);
        EXPECT(ack_key  == kKey);
        EXPECT(count == 2);   // 0x07 omitted

        std::uint32_t ip = 0; std::uint16_t port = 0; std::uint8_t sid = 0;
        EXPECT(r.Read(ip) && r.Read(port) && r.Read(sid));
        EXPECT(ip == 0x0100007F && port == 5815 && sid == 0x05);
        EXPECT(r.Read(ip) && r.Read(port) && r.Read(sid));
        EXPECT(ip == 0x0A00000A && port == 7016 && sid == 0x42);
        EXPECT(r.Eof());
    }

    // --- no resolver configured → empty MW_ROUTE_ACK ------------------
    {
        CapturingWorldClient wc;
        tmapsvr::HandlerContext ctx;
        ctx.world_client = &wc;   // route_resolver left null

        Run(MakeRouteListReq(kCharId, kKey, { 0x05, 0x42 }), ctx);

        EXPECT(wc.sends == 1);
        tmapsvr::wire::Reader r(wc.last_body.data(), wc.last_body.size());
        std::uint32_t ack_char = 0, ack_key = 0;
        std::uint8_t  count = 0xFF;
        EXPECT(r.Read(ack_char) && r.Read(ack_key) && r.Read(count));
        EXPECT(ack_char == kCharId);
        EXPECT(count == 0);
        EXPECT(r.Eof());
    }

    // --- short body → no reply ----------------------------------------
    {
        CapturingWorldClient wc;
        tmapsvr::HandlerContext ctx;
        ctx.world_client = &wc;

        std::vector<std::byte> body;
        tmapsvr::wire::WritePOD<std::uint32_t>(body, kCharId);   // char_id only
        Run(std::move(body), ctx);
        EXPECT(wc.sends == 0);
    }

    if (g_fails == 0)
        std::printf("test_world_routelist: resolve + empty + short-body OK "
                    "(char=0x%08X)\n", kCharId);
    return g_fails == 0 ? 0 : 1;
}
