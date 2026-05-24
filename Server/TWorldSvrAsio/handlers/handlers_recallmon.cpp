#include "handlers.h"
#include "../senders/senders.h"
#include "../wire_codec.h"

#include <spdlog/spdlog.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace tworldsvr::handlers {

namespace {

// Legacy m_dwGenRecallID — a monotonic recall-monster id source
// (++counter). The DB-seed at boot (TWorldSvr.cpp:821) is deferred;
// the counter starts at 0 so the first id is 1.
std::atomic<std::uint32_t> g_recall_id{0};
std::uint32_t GenRecallId() { return ++g_recall_id; }

std::shared_ptr<PeerSession>
FindMapPeer(const HandlerContext& ctx, std::uint8_t msi)
{
    if (msi == 0 || !ctx.peers) return nullptr;
    for (auto& p : ctx.peers->Snapshot())
        if (static_cast<std::uint8_t>(p->Wid() & 0xFF) == msi)
            return p;
    return nullptr;
}

// The valid map-server ids a char is connected to (legacy iterates
// m_mapTCHARCON where m_bValid). Snapshot under the char lock.
std::vector<std::uint8_t>
ValidCons(const std::shared_ptr<TChar>& c)
{
    std::vector<std::uint8_t> out;
    std::lock_guard g(c->lock);
    for (const auto& con : c->cons)
        if (con.valid) out.push_back(con.server_id);
    return out;
}

} // namespace

boost::asio::awaitable<void>
OnCreateRecallMonAck(std::shared_ptr<PeerSession> peer,
                     std::vector<std::byte>       body,
                     const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnCreateRecallMonAck[{}]: registries not wired", ip);
        co_return;
    }

    // Leading fields: char_id, key, mon_id. The rest of the creature
    // state is opaque and forwarded verbatim.
    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, mon_id = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(mon_id))
    {
        spdlog::warn("OnCreateRecallMonAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto c = ctx.chars->Find(char_id);
    if (!c) co_return;
    {
        std::lock_guard g(c->lock);
        if (c->key != key) co_return;
    }

    // World assigns the recall id when the map sent 0; patch it into
    // the forwarded body (mon_id occupies bytes [8, 12)).
    if (mon_id == 0)
    {
        mon_id = GenRecallId();
        std::memcpy(body.data() + 8, &mon_id, sizeof(mon_id));
    }

    for (auto msi : ValidCons(c))
        if (auto p = FindMapPeer(ctx, msi))
            co_await senders::SendMwCreateRecallMonReq(p, body);
    co_return;
}

boost::asio::awaitable<void>
OnRecallMonDataAck(std::shared_ptr<PeerSession> peer,
                   std::vector<std::byte>       body,
                   const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnRecallMonDataAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnRecallMonDataAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto c = ctx.chars->Find(char_id);
    if (!c) co_return;
    {
        std::lock_guard g(c->lock);
        if (c->key != key) co_return;
    }

    for (auto msi : ValidCons(c))
        if (auto p = FindMapPeer(ctx, msi))
            co_await senders::SendMwRecallMonDataReq(p, body);
    co_return;
}

boost::asio::awaitable<void>
OnRecallMonDelAck(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnRecallMonDelAck[{}]: registries not wired", ip);
        co_return;
    }

    // Legacy looks the char up by id only (no key check).
    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0;
    if (!r.Read(char_id))
    {
        spdlog::warn("OnRecallMonDelAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto c = ctx.chars->Find(char_id);
    if (!c) co_return;

    for (auto msi : ValidCons(c))
        if (auto p = FindMapPeer(ctx, msi))
            co_await senders::SendMwRecallMonDelReq(p, body);
    co_return;
}

} // namespace tworldsvr::handlers
