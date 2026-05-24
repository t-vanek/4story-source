#include "handlers.h"
#include "../senders/senders.h"
#include "../services/bow_constants.h"
#include "../wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <mutex>

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

} // namespace

boost::asio::awaitable<void>
OnAddToBowQueueReq(std::shared_ptr<PeerSession> peer,
                   std::vector<std::byte>       body,
                   const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.bow)
    {
        spdlog::warn("OnAddToBowQueueReq[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnAddToBowQueueReq[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto ch = ctx.chars->Find(char_id);
    if (!ch) co_return;

    // Effective country + guild back-pointer for the queue entry.
    // Mirrors legacy SSHandler.cpp:14046 — when the primary country is
    // past TCONTRY_C the aid_country is used (covers B/N/PEACE chars
    // whose aid still names one of the two warring nations); tactics
    // takes priority over guild for the grouping hint.
    std::uint8_t  effective_country = 0;
    std::uint32_t main_id = 0;
    std::uint32_t guild_hint = 0;
    bool          key_ok = true;
    {
        std::lock_guard g(ch->lock);
        if (ch->key != key) { key_ok = false; }
        else
        {
            const std::uint8_t pc = ch->country;
            effective_country = (pc > bow::kCountryC)
                ? ch->aid_country
                : pc;
            guild_hint = ch->tactics_guild_id
                ? ch->tactics_guild_id
                : ch->guild_id;
            main_id = ch->main_server_id;
        }
    }
    if (!key_ok) co_return;

    const std::uint8_t result = ctx.bow->AddPlayer(char_id, key,
        effective_country, guild_hint);
    const std::uint32_t tick = ctx.bow->Tick();

    auto main_peer = FindMapPeer(ctx, static_cast<std::uint8_t>(main_id));
    if (!main_peer) co_return;

    spdlog::info("OnAddToBowQueueReq[{}]: char_id={} country={} guild={} → {}",
        ip, char_id, effective_country, guild_hint, result);
    co_await senders::SendMwAddToBowQueueAck(main_peer, result, char_id, key,
        tick);
}

boost::asio::awaitable<void>
OnCancelBowQueueReq(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte>       body,
                    const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.bow)
    {
        spdlog::warn("OnCancelBowQueueReq[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnCancelBowQueueReq[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto ch = ctx.chars->Find(char_id);
    if (!ch) co_return;

    std::uint32_t main_id = 0;
    bool          key_ok = true;
    {
        std::lock_guard g(ch->lock);
        if (ch->key != key) key_ok = false;
        else                main_id = ch->main_server_id;
    }
    if (!key_ok) co_return;

    const std::uint8_t result = ctx.bow->RemovePlayer(char_id, key);
    const std::uint32_t tick = ctx.bow->Tick();

    // Legacy also tries BRRegistry::ErasePlayerFromQueue on a Bow miss
    // (the player might have been queued for the Battle Royale instead).
    // The BR subsystem isn't ported yet — when it lands, this branch
    // gets the same Erase call here.

    auto main_peer = FindMapPeer(ctx, static_cast<std::uint8_t>(main_id));
    if (!main_peer) co_return;

    spdlog::info("OnCancelBowQueueReq[{}]: char_id={} → {}", ip, char_id,
        result);
    co_await senders::SendMwCancelBowQueueAck(main_peer, result, char_id, key,
        tick);
}

boost::asio::awaitable<void>
OnBowPointsUpdateReq(std::shared_ptr<PeerSession> peer,
                     std::vector<std::byte>       body,
                     const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.bow)
    {
        spdlog::warn("OnBowPointsUpdateReq[{}]: bow registry not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint8_t country = 0;
    if (!r.Read(country))
    {
        spdlog::warn("OnBowPointsUpdateReq[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    ctx.bow->UpdatePoints(country);
    spdlog::info("OnBowPointsUpdateReq[{}]: country={} → D={} C={}", ip,
        country, ctx.bow->Points(bow::kCountryD),
        ctx.bow->Points(bow::kCountryC));
    co_return;
}

} // namespace tworldsvr::handlers
