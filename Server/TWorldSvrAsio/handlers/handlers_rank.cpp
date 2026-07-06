#include "handlers.h"
#include "../services/month_rank_codec.h"
#include "../senders/senders.h"
#include "../wire_codec.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <string>

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
OnFameRankUpdateAck(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte>       body,
                    const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers)
    {
        spdlog::warn("OnFameRankUpdateAck[{}]: peers not wired", ip);
        co_return;
    }
    // Opaque fame-ranking table — forwarded verbatim to every peer.
    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwFameRankUpdateReq(p, body);
    co_return;
}

boost::asio::awaitable<void>
OnHeroSelectAck(std::shared_ptr<PeerSession> peer,
                std::vector<std::byte>       body,
                const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers)
    {
        spdlog::warn("OnHeroSelectAck[{}]: peers not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint16_t battle_zone = 0;
    std::string   hero_name;
    std::int64_t  time_hero = 0;
    if (!r.Read(battle_zone) || !r.ReadString(hero_name) || !r.Read(time_hero))
    {
        spdlog::warn("OnHeroSelectAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwHeroSelectReq(p, battle_zone, hero_name,
            time_hero);
    co_return;
}

boost::asio::awaitable<void>
OnMwMonthRankUpdateAck(std::shared_ptr<PeerSession> peer,
                       std::vector<std::byte>       body,
                       const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.month_rank || !ctx.peers)
    {
        spdlog::warn("OnMwMonthRankUpdateAck[{}]: month_rank/peers "
                     "not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint8_t month = 0, country = 0;
    MonthRanker  ranker{};
    if (!r.Read(month) || !r.Read(country) ||
        !ReadMonthRanker(r, ranker))
    {
        spdlog::warn("OnMwMonthRankUpdateAck[{}]: malformed body "
                     "({} bytes)", ip, body.size());
        co_return;
    }

    // Legacy gate (SSHandler.cpp:11278): stale month or bogus
    // country -> drop.
    if (month != ctx.month_rank->RankMonth() ||
        country >= kMonthRankCountryCount)
        co_return;

    const auto delta = ctx.month_rank->Update(country, ranker);
    if (!delta)
        co_return;                       // landed nowhere

    const auto slots = ctx.month_rank->SnapshotRange(
        country, delta->start_rank, delta->end_rank);
    const MonthRanker warlord = ctx.month_rank->Get(country, 0);
    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwMonthRankUpdateReq(p, month, country,
            delta->start_rank, delta->end_rank, slots,
            delta->new_warlord, warlord);
    co_return;
}

boost::asio::awaitable<void>
OnMwMonthRankResetCharAck(std::shared_ptr<PeerSession> peer,
                          std::vector<std::byte>       body,
                          const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnMwMonthRankResetCharAck[{}]: registries not "
                     "wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0;
    if (!r.Read(char_id))
    {
        spdlog::warn("OnMwMonthRankResetCharAck[{}]: short body "
                     "({} bytes)", ip, body.size());
        co_return;
    }

    auto c = ctx.chars->Find(char_id);
    if (!c) co_return;

    // Mirror to every OTHER map the char is connected to (legacy
    // SSHandler.cpp:13324 skips the reporting peer).
    std::vector<std::uint8_t> con_servers;
    {
        std::lock_guard g(c->lock);
        for (const auto& con : c->cons)
            con_servers.push_back(con.server_id);
    }
    const std::uint8_t sender_id =
        static_cast<std::uint8_t>(peer->Wid() & 0xFF);
    for (std::uint8_t sid : con_servers)
    {
        if (sid == sender_id) continue;
        if (auto p = FindMapPeer(ctx, sid))
            co_await senders::SendMwMonthRankResetCharReq(p, char_id);
    }
    co_return;
}

boost::asio::awaitable<void>
OnMwWarLordSayAck(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers)
    {
        spdlog::warn("OnMwWarLordSayAck[{}]: peers not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint8_t  type = 0, rank_month = 0;
    std::uint32_t char_id = 0;
    std::string   say;
    if (!r.Read(type) || !r.Read(rank_month) || !r.Read(char_id) ||
        !r.ReadString(say))
    {
        spdlog::warn("OnMwWarLordSayAck[{}]: malformed body ({} bytes)",
            ip, body.size());
        co_return;
    }

    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwWarLordSayReq(p, type, rank_month,
            char_id, say);
    co_return;
}

} // namespace tworldsvr::handlers
