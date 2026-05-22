// W3a-2 unit test for PeerRegistry. PureLogic — does not start the
// server, does not open a socket. Constructs PeerSessions from a
// throwaway WorldSession+socket pair so the wrapper has a valid
// shared_ptr<WorldSession> to hold; the wire is never read or
// written.
//
// Scenarios:
//   1. Empty registry: Size()==0, Find returns nullptr.
//   2. Register a peer with wID=42 → Find(42) returns the same
//      shared_ptr, Size()==1.
//   3. Register with wID=0 returns false (sentinel reserved).
//   4. Register a second peer with the same wID returns false;
//      the original entry is retained.
//   5. Unregister returns the removed peer; idempotent on retry.
//   6. Snapshot returns every registered entry.

#include "../peer_session.h"
#include "../services/peer_registry.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <cstdint>
#include <cstdio>
#include <memory>

namespace {

int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

// Build a PeerSession over an *unconnected* socket. The peer
// registry only stores the shared_ptr and reads `wid` / `nation`
// — it never goes near the underlying socket. This keeps the test
// pure-logic and dependency-free.
std::shared_ptr<tworldsvr::PeerSession> MakePeer(boost::asio::io_context& io,
                                                  std::uint16_t wid,
                                                  std::uint8_t nation = 0)
{
    boost::asio::ip::tcp::socket sock(io);
    auto wire = std::make_shared<tworldsvr::WorldSession>(std::move(sock));
    auto peer = std::make_shared<tworldsvr::PeerSession>(wire);
    peer->SetWid(wid);
    peer->SetNation(nation);
    return peer;
}

} // namespace

int main()
{
    using namespace tworldsvr;
    boost::asio::io_context io;

    // --- Scenario 1: empty ---
    {
        PeerRegistry reg;
        EXPECT(reg.Size() == 0);
        EXPECT(reg.Find(1) == nullptr);
        EXPECT(reg.Snapshot().empty());
    }

    // --- Scenario 2: register / Find ---
    {
        PeerRegistry reg;
        auto p = MakePeer(io, 42, 1);
        EXPECT(reg.Register(p));
        EXPECT(reg.Size() == 1);
        auto found = reg.Find(42);
        EXPECT(found != nullptr);
        EXPECT(found.get() == p.get());
    }

    // --- Scenario 3: wid=0 is reserved ---
    {
        PeerRegistry reg;
        auto p = MakePeer(io, 0);
        EXPECT(!reg.Register(p));
        EXPECT(reg.Size() == 0);
    }

    // --- Scenario 4: duplicate wid retains original ---
    {
        PeerRegistry reg;
        auto first  = MakePeer(io, 7);
        auto second = MakePeer(io, 7);
        EXPECT(reg.Register(first));
        EXPECT(!reg.Register(second));
        auto found = reg.Find(7);
        EXPECT(found.get() == first.get());
    }

    // --- Scenario 5: Unregister idempotent ---
    {
        PeerRegistry reg;
        auto p = MakePeer(io, 99);
        reg.Register(p);
        auto removed = reg.Unregister(99);
        EXPECT(removed.get() == p.get());
        EXPECT(reg.Size() == 0);
        EXPECT(reg.Unregister(99) == nullptr);   // idempotent
        EXPECT(reg.Unregister(0)  == nullptr);   // sentinel
    }

    // --- Scenario 6: Snapshot ---
    {
        PeerRegistry reg;
        reg.Register(MakePeer(io, 1));
        reg.Register(MakePeer(io, 2));
        reg.Register(MakePeer(io, 3));
        auto snap = reg.Snapshot();
        EXPECT(snap.size() == 3);
    }

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_peer_registry (6 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_peer_registry (%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
