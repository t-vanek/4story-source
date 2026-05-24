#include "handlers.h"
#include "../senders/senders.h"
#include "../wire_codec.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace tworldsvr::handlers {

namespace {

std::shared_ptr<PeerSession>
FindMapPeer(const HandlerContext& ctx, std::uint8_t msi)
{
    if (msi == 0 || !ctx.peers) return nullptr;
    for (auto& p : ctx.peers->Snapshot())
        if (static_cast<std::uint8_t>(p->Wid() & 0xFF) == msi)
            return p;
    return nullptr;
}

// Shared body of the two mail-delivery relays. The packet carries
// (post_id, sender, target, title, type); world routes it to the
// `target` char's map as MW_POSTRECV_REQ, forwarding the bytes
// verbatim (it never interprets the title/type). An offline target
// is dropped — the mail is already persisted DB-side and shown the
// next time they open their mailbox (legacy OnMW_POSTRECV_ACK /
// OnDM_RESERVEDPOSTRECV_ACK both behave this way).
boost::asio::awaitable<void>
RelayPostRecv(std::string_view tag, std::shared_ptr<PeerSession> peer,
              std::vector<std::byte> body, const HandlerContext& ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("{}[{}]: registries not wired", tag, ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t post_id = 0;
    std::string   sender, target;
    if (!r.Read(post_id) || !r.ReadString(sender) || !r.ReadString(target))
    {
        spdlog::warn("{}[{}]: short body ({} bytes)", tag, ip, body.size());
        co_return;
    }

    auto tgt = ctx.chars->FindByName(target);
    if (!tgt) co_return;
    std::uint8_t msi = 0;
    { std::lock_guard g(tgt->lock); msi = tgt->main_server_id; }
    if (auto p = FindMapPeer(ctx, msi))
        co_await senders::SendMwPostRecvReq(p, body);
}

} // namespace

boost::asio::awaitable<void>
OnPostRecvAck(std::shared_ptr<PeerSession> peer,
              std::vector<std::byte>       body,
              const HandlerContext&        ctx)
{
    co_await RelayPostRecv("OnPostRecvAck", std::move(peer), std::move(body),
                           ctx);
}

boost::asio::awaitable<void>
OnReservedPostRecvAck(std::shared_ptr<PeerSession> peer,
                      std::vector<std::byte>       body,
                      const HandlerContext&        ctx)
{
    co_await RelayPostRecv("OnReservedPostRecvAck", std::move(peer),
                           std::move(body), ctx);
}

} // namespace tworldsvr::handlers
