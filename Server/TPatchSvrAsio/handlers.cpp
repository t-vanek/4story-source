#include "handlers.h"
#include "patch_server.h"

#include "MessageId.h"

#include "fourstory/db/co_offload.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstring>

namespace tpatchsvr::handlers {

namespace {

// Length-prefixed STRING write — matches legacy CPacket::operator<<:
// INT32 length followed by raw bytes (no NUL, CP1252).
void WriteString(std::vector<std::byte>& out, const std::string& s)
{
    const std::int32_t len = static_cast<std::int32_t>(s.size());
    const std::byte* lp = reinterpret_cast<const std::byte*>(&len);
    out.insert(out.end(), lp, lp + 4);
    const std::byte* sp = reinterpret_cast<const std::byte*>(s.data());
    out.insert(out.end(), sp, sp + s.size());
}

template <class T>
void WritePOD(std::vector<std::byte>& out, T v)
{
    const auto* p = reinterpret_cast<const std::byte*>(&v);
    out.insert(out.end(), p, p + sizeof(T));
}

// Resolve the configured login_host into a network-order 32-bit IPv4
// address. Inline parser, no dependency on getaddrinfo because the
// config always carries a dotted-decimal string.
std::uint32_t ParseIPv4Network(const std::string& dotted)
{
    int a = 0, b = 0, c = 0, d = 0;
    if (std::sscanf(dotted.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d) != 4)
        return 0;
    return (static_cast<std::uint32_t>(a)      ) |
           (static_cast<std::uint32_t>(b) <<  8) |
           (static_cast<std::uint32_t>(c) << 16) |
           (static_cast<std::uint32_t>(d) << 24);
}

bool ReadDWORD(const std::vector<std::byte>& body, std::size_t off,
               std::uint32_t& out)
{
    if (off + 4 > body.size()) return false;
    std::memcpy(&out, body.data() + off, 4);
    return true;
}

} // namespace

boost::asio::awaitable<void>
OnServiceMonitor(std::shared_ptr<PatchSession> session,
                 std::vector<std::byte> body,
                 const ServerContext& ctx)
{
    // Legacy reads INT64 padding + DWORD tick. We mirror that.
    if (body.size() < 12)
    {
        spdlog::warn("CT_SERVICEMONITOR_ACK body too short ({} bytes)", body.size());
        co_return;
    }
    std::uint32_t tick = 0;
    std::memcpy(&tick, body.data() + 8, 4);

    // Legacy flips m_bSessionType to SESSION_SERVER on the first
    // SERVICEMONITOR_ACK so the stale-client sweep skips this peer.
    // Mirror that here — the sweep above (and the periodic loop in
    // PatchServer) consult IsServerPeer() to decide.
    session->MarkAsServerPeer();

    std::vector<std::byte> reply;
    reply.reserve(8 + 4 * 4);
    WritePOD<std::int64_t>(reply, 0);        // padding INT64 (legacy quirk)
    WritePOD<std::uint32_t>(reply, tick);
    WritePOD<std::uint32_t>(reply, static_cast<std::uint32_t>(ctx.session_count));
    WritePOD<std::uint32_t>(reply, static_cast<std::uint32_t>(ctx.session_count));
    WritePOD<std::uint32_t>(reply, 0);       // dwActiveUser placeholder
    spdlog::info("CT_SERVICEMONITOR_ACK tick={} sessions={}",
        tick, ctx.session_count);
    co_await session->SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CT_SERVICEMONITOR_REQ),
        std::move(reply));

    // Legacy fires the stale-client sweep on every monitor
    // heartbeat (P-6). Replicate the 60-second cap.
    if (ctx.server)
    {
        const auto closed =
            ctx.server->SweepStaleClients(std::chrono::seconds(60));
        if (closed > 0)
            spdlog::info("CT_SERVICEMONITOR sweep closed {} stale "
                         "client session(s)", closed);
    }
}

boost::asio::awaitable<void>
OnServiceDataClear(std::shared_ptr<PatchSession> /*session*/,
                   std::vector<std::byte> /*body*/)
{
    spdlog::debug("CT_SERVICEDATACLEAR_ACK — no-op");
    co_return;
}

namespace {

// Common ack-builder: dispatch header (string + ip + port + count)
// then per-file rows. Legacy CT_PATCH_ACK shape; CT_NEWPATCH_ACK
// inserts a `dwMinBetaVer` between port and count.
void EmitPatchAckHeader(std::vector<std::byte>& out,
                        const std::string& ftp,
                        std::uint32_t login_ip_net,
                        std::uint16_t login_port,
                        std::uint16_t count)
{
    WriteString(out, ftp);
    WritePOD<std::uint32_t>(out, login_ip_net);
    WritePOD<std::uint16_t>(out, login_port);
    WritePOD<std::uint16_t>(out, count);
}

} // namespace

boost::asio::awaitable<void>
OnPatch(std::shared_ptr<PatchSession> session,
        std::vector<std::byte> body,
        const ServerContext& ctx)
{
    std::uint32_t from_version = 0;
    ReadDWORD(body, 0, from_version);
    const auto files = ctx.repo
        ? ctx.repo->ListPatchesSince(from_version)
        : std::vector<PatchFile>{};
    spdlog::info("CT_PATCH_REQ from_version={} → {} files",
        from_version, files.size());

    std::vector<std::byte> reply;
    EmitPatchAckHeader(reply, ctx.ftp_url,
        ParseIPv4Network(ctx.login_host), ctx.login_port,
        static_cast<std::uint16_t>(files.size()));
    for (const auto& f : files)
    {
        WritePOD<std::uint32_t>(reply, f.version);
        WriteString(reply, f.path);
        WriteString(reply, f.name);
        WritePOD<std::uint32_t>(reply, f.size);
    }
    co_await session->SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CT_PATCH_ACK),
        std::move(reply));
}

boost::asio::awaitable<void>
OnNewPatch(std::shared_ptr<PatchSession> session,
           std::vector<std::byte> body,
           const ServerContext& ctx)
{
    std::uint32_t from_version = 0;
    ReadDWORD(body, 0, from_version);
    const auto files = ctx.repo
        ? ctx.repo->ListPatchesSince(from_version)
        : std::vector<PatchFile>{};
    const std::uint32_t min_beta = ctx.repo
        ? ctx.repo->MinBetaVersion() : 0;
    spdlog::info("CT_NEWPATCH_REQ from_version={} min_beta={} → {} files",
        from_version, min_beta, files.size());

    std::vector<std::byte> reply;
    WriteString(reply, ctx.ftp_url);
    WritePOD<std::uint32_t>(reply, ParseIPv4Network(ctx.login_host));
    WritePOD<std::uint16_t>(reply, ctx.login_port);
    WritePOD<std::uint32_t>(reply, min_beta);
    WritePOD<std::uint16_t>(reply, static_cast<std::uint16_t>(files.size()));
    for (const auto& f : files)
    {
        WritePOD<std::uint32_t>(reply, f.version);
        WriteString(reply, f.path);
        WriteString(reply, f.name);
        WritePOD<std::uint32_t>(reply, f.size);
        WritePOD<std::uint32_t>(reply, f.beta_ver);
    }
    co_await session->SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CT_NEWPATCH_ACK),
        std::move(reply));
}

boost::asio::awaitable<void>
OnChangeInterface(std::shared_ptr<PatchSession> session,
                  std::vector<std::byte> body,
                  const ServerContext& ctx)
{
    std::uint8_t option = body.empty()
        ? std::uint8_t{0}
        : static_cast<std::uint8_t>(body[0]);
    const auto files = ctx.repo
        ? ctx.repo->ListInterfaceFiles(option)
        : std::vector<PatchFile>{};
    spdlog::info("CT_CHANGEIF_REQ option={} → {} files", option, files.size());

    std::vector<std::byte> reply;
    const std::string if_ftp = ctx.ftp_url + "/interface";
    WriteString(reply, if_ftp);
    WritePOD<std::uint32_t>(reply, ParseIPv4Network(ctx.login_host));
    WritePOD<std::uint16_t>(reply, ctx.login_port);
    WritePOD<std::uint32_t>(reply, 0);  // dwMinBetaVer = NULL in legacy
    WritePOD<std::uint16_t>(reply, static_cast<std::uint16_t>(files.size()));
    for (const auto& f : files)
    {
        WritePOD<std::uint32_t>(reply, f.version);
        WriteString(reply, std::string{}); // szPath empty
        WriteString(reply, f.name);
        WritePOD<std::uint32_t>(reply, f.size);
        WritePOD<std::uint32_t>(reply, 0); // dwBetaVer = NULL
    }
    co_await session->SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CT_NEWPATCH_ACK),
        std::move(reply));
}

boost::asio::awaitable<void>
OnPrePatch(std::shared_ptr<PatchSession> session,
           std::vector<std::byte> body,
           const ServerContext& ctx)
{
    std::uint32_t from_beta = 0;
    ReadDWORD(body, 0, from_beta);
    const auto files = ctx.repo
        ? ctx.repo->ListPrePatchesSince(from_beta)
        : std::vector<PatchFile>{};
    spdlog::info("CT_PREPATCH_REQ from_beta={} → {} files",
        from_beta, files.size());

    std::vector<std::byte> reply;
    EmitPatchAckHeader(reply, ctx.pre_ftp_url,
        ParseIPv4Network(ctx.login_host), ctx.login_port,
        static_cast<std::uint16_t>(files.size()));
    for (const auto& f : files)
    {
        WritePOD<std::uint32_t>(reply, f.beta_ver);
        WriteString(reply, f.path);
        WriteString(reply, f.name);
        WritePOD<std::uint32_t>(reply, f.size);
    }
    co_await session->SendPacket(
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CT_PREPATCH_ACK),
        std::move(reply));
}

boost::asio::awaitable<void>
OnPatchStart(std::shared_ptr<PatchSession> session,
             std::vector<std::byte> /*body*/)
{
    spdlog::info("CT_PATCHSTART_REQ — closing session");
    session->Close();
    co_return;
}

boost::asio::awaitable<void>
OnCtrlSvr(std::shared_ptr<PatchSession> /*session*/,
          std::vector<std::byte> /*body*/)
{
    spdlog::debug("CT_CTRLSVR_REQ heartbeat");
    co_return;
}

boost::asio::awaitable<void>
OnPrePatchComplete(std::shared_ptr<PatchSession> session,
                   std::vector<std::byte> body,
                   const ServerContext& ctx)
{
    std::uint32_t beta_ver = 0;
    ReadDWORD(body, 0, beta_ver);
    if (ctx.repo)
    {
        // MarkPreVersionComplete runs a MERGE+DELETE transaction —
        // the slowest call path in this server. Offload to a worker
        // when a db_pool is wired so the patch_session coroutine
        // doesn't block the io_context for the duration of the txn.
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.repo, beta_ver] {
                repo->MarkPreVersionComplete(beta_ver);
            });
    }
    spdlog::info("CT_PREPATCHCOMPLETE_REQ beta_ver={} — closing", beta_ver);
    session->Close();
    co_return;
}

} // namespace tpatchsvr::handlers
