// Byte-layout suite for client_senders — the CS_ acks the map relays
// down to the client as a result of inbound World→Map cluster traffic.
// AsioSession::SendPacket needs a live socket, so the encoders are what
// the tests pin (same approach as test_world_senders for the MW_ side).

#include "services/client_senders.h"
#include "wire_codec.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

} // namespace

int main()
{
    using namespace tmapsvr;

    // --- CS_ADDCONNECT_ACK: BYTE count + count×(u32 ip, u16 port, u8 id) ---
    {
        std::vector<ConnectRoute> routes = {
            { 0x0100007F, 5815, 0x05 },   // 127.0.0.1
            { 0x0A00000A, 7016, 0x42 },   // 10.0.0.10
        };
        auto b = EncodeAddConnectAck(routes);

        // 1 (count) + 2 × (4 + 2 + 1) = 1 + 14 = 15 bytes
        EXPECT(b.size() == 15);

        wire::Reader r(b.data(), b.size());
        std::uint8_t count = 0;
        EXPECT(r.Read(count));
        EXPECT(count == 2);

        std::uint32_t ip = 0; std::uint16_t port = 0; std::uint8_t sid = 0;
        EXPECT(r.Read(ip) && r.Read(port) && r.Read(sid));
        EXPECT(ip == 0x0100007F);
        EXPECT(port == 5815);
        EXPECT(sid == 0x05);

        EXPECT(r.Read(ip) && r.Read(port) && r.Read(sid));
        EXPECT(ip == 0x0A00000A);
        EXPECT(port == 7016);
        EXPECT(sid == 0x42);

        EXPECT(r.Eof());
    }

    // --- empty list → single zero count byte ---------------------------
    {
        auto b = EncodeAddConnectAck({});
        EXPECT(b.size() == 1);
        EXPECT(std::to_integer<int>(b[0]) == 0);
    }

    if (g_fails == 0)
        std::printf("test_client_senders: addconnect-ack layout OK\n");
    return g_fails == 0 ? 0 : 1;
}
