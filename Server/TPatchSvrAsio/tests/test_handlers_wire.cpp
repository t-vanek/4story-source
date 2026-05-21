// Wire-level tests for TPatchSvrAsio handlers — exercises each
// CT_* request handler against a FakePatchRepository, captures the
// emitted ACK packet off the loopback wire, and asserts on the
// full header + body bytes.
//
// Scope: PatchSession::SendPacket is the codec under test (8-byte
// header + folded checksum + body). The handlers compose body bytes
// via WriteString / WritePOD; this test locks down both the codec
// framing and the ACK body layout (field order, count widths, IP
// network byte order).
//
// No database: a FakePatchRepository implements IPatchRepository
// with canned responses. Run on every build — no env gating.

#include "handlers.h"
#include "patch_server.h"
#include "patch_session.h"
#include "services/patch_repository.h"

#include "MessageId.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace asio = boost::asio;

namespace {

int g_passed = 0;
int g_failed = 0;
void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

// FakePatchRepository — implements IPatchRepository with canned data
// injected via public fields. Lets each test assert on the exact
// row set the handler will emit.
struct FakePatchRepository : tpatchsvr::IPatchRepository
{
    std::vector<tpatchsvr::PatchFile> patches;
    std::vector<tpatchsvr::PatchFile> pre_patches;
    std::vector<tpatchsvr::PatchFile> interface_files;
    std::uint32_t                     min_beta = 0;
    std::uint32_t                     last_promoted_beta = 0;
    int                               promote_call_count = 0;
    std::uint32_t                     last_since = 0;
    std::uint8_t                      last_option = 0;

    std::vector<tpatchsvr::PatchFile>
    ListPatchesSince(std::uint32_t from_version) override
    {
        last_since = from_version;
        return patches;
    }
    std::vector<tpatchsvr::PatchFile>
    ListPrePatchesSince(std::uint32_t beta_version) override
    {
        last_since = beta_version;
        return pre_patches;
    }
    std::vector<tpatchsvr::PatchFile>
    ListInterfaceFiles(std::uint8_t option) override
    {
        last_option = option;
        return interface_files;
    }
    std::uint32_t MinBetaVersion() override
    {
        return min_beta;
    }
    void MarkPreVersionComplete(std::uint32_t beta_version) override
    {
        last_promoted_beta = beta_version;
        ++promote_call_count;
    }
};

// Loopback rig: a paired (client, server) tcp socket. The handler
// runs against `server`; the test reads ACK bytes off `client`.
struct LoopbackPair
{
    asio::io_context io;
    asio::ip::tcp::socket client;
    asio::ip::tcp::socket server;

    LoopbackPair() : client(io), server(io)
    {
        asio::ip::tcp::acceptor acc(io,
            asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0));
        client.connect(acc.local_endpoint());
        acc.accept(server);
    }
};

// Read one framed packet from the client side: 8-byte header then
// body of (wSize - 8) bytes. Returns {wID, body}. Throws on framing
// or read failure.
struct CapturedPacket
{
    std::uint16_t          wId;
    std::uint32_t          dwChkSum;
    std::vector<std::byte> body;
};

CapturedPacket ReadOnePacket(asio::ip::tcp::socket& sock)
{
    tpatchsvr::PacketHeader hdr{};
    asio::read(sock, asio::buffer(&hdr, sizeof(hdr)));
    if (hdr.wSize < sizeof(hdr))
        throw std::runtime_error("wSize too small");
    const std::size_t body_size = hdr.wSize - sizeof(hdr);
    std::vector<std::byte> body(body_size);
    if (body_size > 0)
        asio::read(sock, asio::buffer(body.data(), body_size));
    return { hdr.wID, hdr.dwChkSum, std::move(body) };
}

// Decode helpers: mirror handlers.cpp WriteString / WritePOD.
struct BodyReader
{
    const std::vector<std::byte>& buf;
    std::size_t off = 0;
    explicit BodyReader(const std::vector<std::byte>& b) : buf(b) {}

    template <class T> T ReadPOD()
    {
        if (off + sizeof(T) > buf.size())
            throw std::runtime_error("BodyReader: ReadPOD overrun");
        T v{};
        std::memcpy(&v, buf.data() + off, sizeof(T));
        off += sizeof(T);
        return v;
    }
    std::string ReadString()
    {
        const auto len = ReadPOD<std::int32_t>();
        if (len < 0 || off + len > buf.size())
            throw std::runtime_error("BodyReader: ReadString overrun");
        std::string s(reinterpret_cast<const char*>(buf.data() + off), len);
        off += len;
        return s;
    }
    bool AtEnd() const { return off == buf.size(); }
};

// Run a single handler against the loopback rig. Spins up a thread
// to drive io_context (handler SendPacket is async) while the test
// thread reads the response synchronously.
template <class HandlerFn>
CapturedPacket DriveHandler(LoopbackPair& rig, HandlerFn&& fn)
{
    auto session = std::make_shared<tpatchsvr::PatchSession>(
        std::move(rig.server));
    asio::co_spawn(rig.io, fn(session), asio::detached);

    std::thread worker([&] { rig.io.run(); });
    CapturedPacket pkt = ReadOnePacket(rig.client);
    worker.join();
    return pkt;
}

// ---------------------------------------------------------------------------

void TestOnPatchEmitsPatchAck()
{
    std::printf("[wire — OnPatch → CT_PATCH_ACK shape]\n");
    LoopbackPair rig;
    FakePatchRepository fake;
    fake.patches = {
        {123u, 999u, "patch", "a.dat", 4096u},
        {200u, 999u, "patch/ui", "b.dat", 16384u},
    };

    tpatchsvr::handlers::ServerContext ctx{};
    ctx.repo = &fake;
    ctx.ftp_url    = "ftp://example/patch";
    ctx.login_host = "10.20.30.40";
    ctx.login_port = 2110;

    std::vector<std::byte> body(4);
    const std::uint32_t since = 100;
    std::memcpy(body.data(), &since, 4);

    const auto pkt = DriveHandler(rig,
        [&](std::shared_ptr<tpatchsvr::PatchSession> s)
        { return tpatchsvr::handlers::OnPatch(std::move(s), body, ctx); });

    Check(pkt.wId == tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::CT_PATCH_ACK),
        "wID == CT_PATCH_ACK");
    Check(fake.last_since == 100,
        "repo.ListPatchesSince saw request's from_version");

    BodyReader br(pkt.body);
    Check(br.ReadString() == "ftp://example/patch", "ftp_url field");
    const auto ip_net = br.ReadPOD<std::uint32_t>();
    // 10.20.30.40 little-endian-on-wire = 0x281E140A
    Check(ip_net == 0x281E140Au, "login_host parsed to network-order IPv4");
    Check(br.ReadPOD<std::uint16_t>() == 2110, "login_port field");
    Check(br.ReadPOD<std::uint16_t>() == 2,    "count = 2 (uint16)");

    Check(br.ReadPOD<std::uint32_t>() == 123u, "row[0].version");
    Check(br.ReadString() == "patch",          "row[0].path");
    Check(br.ReadString() == "a.dat",          "row[0].name");
    Check(br.ReadPOD<std::uint32_t>() == 4096u,"row[0].size");

    Check(br.ReadPOD<std::uint32_t>() == 200u,    "row[1].version");
    Check(br.ReadString() == "patch/ui",          "row[1].path");
    Check(br.ReadString() == "b.dat",             "row[1].name");
    Check(br.ReadPOD<std::uint32_t>() == 16384u,  "row[1].size");

    Check(br.AtEnd(), "body fully consumed (no trailing bytes)");
}

void TestOnNewPatchInsertsMinBeta()
{
    std::printf("[wire — OnNewPatch → CT_NEWPATCH_ACK with min_beta]\n");
    LoopbackPair rig;
    FakePatchRepository fake;
    fake.patches = { {500u, 700u, "p", "f.dat", 64u} };
    fake.min_beta = 700u;

    tpatchsvr::handlers::ServerContext ctx{};
    ctx.repo = &fake;
    ctx.ftp_url    = "ftp://x/y";
    ctx.login_host = "127.0.0.1";
    ctx.login_port = 9000;

    std::vector<std::byte> body(4);
    const std::uint32_t since = 0;
    std::memcpy(body.data(), &since, 4);

    const auto pkt = DriveHandler(rig,
        [&](std::shared_ptr<tpatchsvr::PatchSession> s)
        { return tpatchsvr::handlers::OnNewPatch(std::move(s), body, ctx); });

    Check(pkt.wId == tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::CT_NEWPATCH_ACK),
        "wID == CT_NEWPATCH_ACK");

    BodyReader br(pkt.body);
    Check(br.ReadString() == "ftp://x/y",       "ftp_url");
    br.ReadPOD<std::uint32_t>(); // ip (already covered)
    Check(br.ReadPOD<std::uint16_t>() == 9000,  "login_port");
    Check(br.ReadPOD<std::uint32_t>() == 700u,  "min_beta inlined before count");
    Check(br.ReadPOD<std::uint16_t>() == 1,     "count");
    Check(br.ReadPOD<std::uint32_t>() == 500u,  "row.version");
    Check(br.ReadString() == "p",               "row.path");
    Check(br.ReadString() == "f.dat",           "row.name");
    Check(br.ReadPOD<std::uint32_t>() == 64u,   "row.size");
    Check(br.ReadPOD<std::uint32_t>() == 700u,  "row.beta_ver");
    Check(br.AtEnd(), "body fully consumed");
}

void TestOnChangeInterfaceAppendsInterfacePath()
{
    std::printf("[wire — OnChangeInterface → CT_NEWPATCH_ACK '/interface']\n");
    LoopbackPair rig;
    FakePatchRepository fake;
    fake.interface_files = { {7u, 0u, "(ignored)", "ui.png", 256u} };

    tpatchsvr::handlers::ServerContext ctx{};
    ctx.repo = &fake;
    ctx.ftp_url    = "ftp://srv/root";
    ctx.login_host = "1.2.3.4";
    ctx.login_port = 1;

    std::vector<std::byte> body = { std::byte{3} };

    const auto pkt = DriveHandler(rig,
        [&](std::shared_ptr<tpatchsvr::PatchSession> s)
        { return tpatchsvr::handlers::OnChangeInterface(std::move(s), body, ctx); });

    Check(pkt.wId == tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::CT_NEWPATCH_ACK),
        "wID == CT_NEWPATCH_ACK (legacy alias)");
    Check(fake.last_option == 3, "repo.ListInterfaceFiles saw option byte");

    BodyReader br(pkt.body);
    Check(br.ReadString() == "ftp://srv/root/interface",
        "ftp_url suffixed with /interface");
    br.ReadPOD<std::uint32_t>(); // ip
    br.ReadPOD<std::uint16_t>(); // port
    Check(br.ReadPOD<std::uint32_t>() == 0u,
        "min_beta forced to 0 on interface path");
    Check(br.ReadPOD<std::uint16_t>() == 1,    "count");
    Check(br.ReadPOD<std::uint32_t>() == 7u,   "row.version");
    Check(br.ReadString().empty(),             "row.path emitted as empty");
    Check(br.ReadString() == "ui.png",         "row.name");
    Check(br.ReadPOD<std::uint32_t>() == 256u, "row.size");
    Check(br.ReadPOD<std::uint32_t>() == 0u,   "row.beta_ver forced to 0");
    Check(br.AtEnd(), "body fully consumed");
}

void TestOnPrePatchEmitsPrePatchAck()
{
    std::printf("[wire — OnPrePatch → CT_PREPATCH_ACK shape]\n");
    LoopbackPair rig;
    FakePatchRepository fake;
    fake.pre_patches = { {0u, 42u, "beta", "x.zip", 8u} };

    tpatchsvr::handlers::ServerContext ctx{};
    ctx.repo = &fake;
    ctx.pre_ftp_url = "ftp://beta";
    ctx.login_host  = "5.6.7.8";
    ctx.login_port  = 4321;

    std::vector<std::byte> body(4);
    const std::uint32_t since = 41;
    std::memcpy(body.data(), &since, 4);

    const auto pkt = DriveHandler(rig,
        [&](std::shared_ptr<tpatchsvr::PatchSession> s)
        { return tpatchsvr::handlers::OnPrePatch(std::move(s), body, ctx); });

    Check(pkt.wId == tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::CT_PREPATCH_ACK),
        "wID == CT_PREPATCH_ACK");
    Check(fake.last_since == 41,
        "repo.ListPrePatchesSince saw request's from_beta");

    BodyReader br(pkt.body);
    Check(br.ReadString() == "ftp://beta",     "pre_ftp_url field");
    br.ReadPOD<std::uint32_t>(); // ip
    Check(br.ReadPOD<std::uint16_t>() == 4321, "login_port");
    Check(br.ReadPOD<std::uint16_t>() == 1,    "count");
    Check(br.ReadPOD<std::uint32_t>() == 42u,  "row.beta_ver (first field)");
    Check(br.ReadString() == "beta",           "row.path");
    Check(br.ReadString() == "x.zip",          "row.name");
    Check(br.ReadPOD<std::uint32_t>() == 8u,   "row.size");
    Check(br.AtEnd(), "body fully consumed");
}

void TestOnServiceMonitorReplyEchoesTickAndCount()
{
    std::printf("[wire — OnServiceMonitor → CT_SERVICEMONITOR_REQ echo]\n");
    LoopbackPair rig;

    tpatchsvr::handlers::ServerContext ctx{};
    ctx.session_count = 17;
    ctx.server = nullptr;

    // Body: INT64 padding + DWORD tick (legacy quirk).
    std::vector<std::byte> body(12, std::byte{0});
    const std::uint32_t tick = 0xCAFEBABE;
    std::memcpy(body.data() + 8, &tick, 4);

    const auto pkt = DriveHandler(rig,
        [&](std::shared_ptr<tpatchsvr::PatchSession> s)
        { return tpatchsvr::handlers::OnServiceMonitor(
            std::move(s), body, ctx); });

    Check(pkt.wId == tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::CT_SERVICEMONITOR_REQ),
        "wID == CT_SERVICEMONITOR_REQ");

    BodyReader br(pkt.body);
    Check(br.ReadPOD<std::int64_t>() == 0,
        "leading INT64 padding (legacy quirk)");
    Check(br.ReadPOD<std::uint32_t>() == 0xCAFEBABEu,
        "tick echoed from request");
    Check(br.ReadPOD<std::uint32_t>() == 17u,
        "session count (first slot)");
    Check(br.ReadPOD<std::uint32_t>() == 17u,
        "session count (second slot)");
    Check(br.ReadPOD<std::uint32_t>() == 0u,
        "active user placeholder = 0");
    Check(br.AtEnd(), "body fully consumed");
}

void TestOnPrePatchCompleteCallsRepoAndCloses()
{
    std::printf("[wire — OnPrePatchComplete promotes + closes session]\n");
    asio::io_context io;
    asio::ip::tcp::acceptor acc(io,
        asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0));
    asio::ip::tcp::socket client(io);
    client.connect(acc.local_endpoint());
    asio::ip::tcp::socket server_sock(io);
    acc.accept(server_sock);

    auto session = std::make_shared<tpatchsvr::PatchSession>(
        std::move(server_sock));

    FakePatchRepository fake;
    tpatchsvr::handlers::ServerContext ctx{};
    ctx.repo = &fake;

    std::vector<std::byte> body(4);
    const std::uint32_t beta = 555u;
    std::memcpy(body.data(), &beta, 4);

    asio::co_spawn(io,
        tpatchsvr::handlers::OnPrePatchComplete(session, body, ctx),
        asio::detached);
    io.run();

    Check(fake.promote_call_count == 1,
        "repo.MarkPreVersionComplete called exactly once");
    Check(fake.last_promoted_beta == 555u,
        "beta_ver propagated from request");
    Check(!session->IsOpen(),
        "session closed after PRECOMPLETE");
}

void TestChecksumMismatchClosesSession()
{
    std::printf("[wire — bad-checksum frame closes Run loop]\n");
    asio::io_context io;
    asio::ip::tcp::acceptor acc(io,
        asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0));
    asio::ip::tcp::socket client(io);
    client.connect(acc.local_endpoint());
    asio::ip::tcp::socket server_sock(io);
    acc.accept(server_sock);

    auto session = std::make_shared<tpatchsvr::PatchSession>(
        std::move(server_sock));

    // Forge a packet with deliberately wrong checksum.
    tpatchsvr::PacketHeader hdr{};
    const std::vector<std::byte> body = {
        std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE}, std::byte{0xEF}
    };
    hdr.wSize    = static_cast<std::uint16_t>(sizeof(hdr) + body.size());
    hdr.wID      = 0x1234;
    hdr.dwChkSum = 0; // wrong: the fold over DEADBEEF is non-zero
    asio::write(client, asio::buffer(&hdr, sizeof(hdr)));
    asio::write(client, asio::buffer(body.data(), body.size()));

    bool handler_fired = false;
    asio::co_spawn(io, session->Run(
        [&](std::shared_ptr<tpatchsvr::PatchSession>,
            tpatchsvr::DecodedPacket) -> asio::awaitable<void>
        {
            handler_fired = true;
            co_return;
        }), asio::detached);
    io.run();

    Check(!handler_fired,
        "handler not invoked when checksum mismatches");
}

} // namespace

int main()
{
    std::printf("=== tpatchsvr_asio wire-level handler test ===\n");
    try
    {
        TestOnPatchEmitsPatchAck();
        TestOnNewPatchInsertsMinBeta();
        TestOnChangeInterfaceAppendsInterfacePath();
        TestOnPrePatchEmitsPrePatchAck();
        TestOnServiceMonitorReplyEchoesTickAndCount();
        TestOnPrePatchCompleteCallsRepoAndCloses();
        TestChecksumMismatchClosesSession();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
