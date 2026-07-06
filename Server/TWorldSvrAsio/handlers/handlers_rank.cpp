#include "handlers.h"
#include "../services/month_rank_codec.h"

#include "fourstory/db/co_offload.h"
#include "../senders/senders.h"
#include "../wire_codec.h"

#include <spdlog/spdlog.h>

#include <algorithm>

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

namespace {

// MonthRankDesc (TWorldType.h:1051): MonthPoint desc, MonthWin desc,
// MonthLose desc, then char-id asc.
bool MonthRankDesc(const MonthRanker& a, const MonthRanker& b)
{
    if (a.month_point != b.month_point)
        return a.month_point > b.month_point;
    if (a.month_win != b.month_win)
        return a.month_win > b.month_win;
    if (a.month_lose != b.month_lose)
        return a.month_lose > b.month_lose;
    return a.char_id < b.char_id;
}

} // namespace

boost::asio::awaitable<void>
RunMonthRankRollover(const HandlerContext& ctx)
{
    if (!ctx.month_rank || !ctx.peers)
    {
        spdlog::warn("RunMonthRankRollover: month_rank/peers not wired");
        co_return;
    }
    if (!ctx.month_rank_repo)
    {
        spdlog::warn("RunMonthRankRollover: month_rank_repo not wired "
                     "- rollover skipped (no DB configured)");
        co_return;
    }

    const std::uint8_t month = ctx.month_rank->RankMonth();

    // 1. Zero every guild month bank (legacy SSHandler.cpp:11017).
    if (ctx.guilds)
    {
        for (std::uint32_t gid : ctx.guilds->SnapshotIds())
        {
            if (auto g = ctx.guilds->Find(gid))
            {
                std::lock_guard lk(g->lock);
                g->pvp_month_point = 0;
                g->rank_month      = 0;
            }
        }
    }

    // 2. Cross-country total-rank array (legacy SSHandler.cpp:11031).
    const auto table = ctx.month_rank->SnapshotTable();
    std::array<MonthRanker, kTotalMonthRankCount> total{};
    {
        std::vector<MonthRanker> pool;
        for (std::size_t c = 0; c < kMonthRankCountryCount; ++c)
            for (std::size_t j = 1; j < kFirstGradeGroupCount; ++j)
                pool.push_back(table[c][j]);
        std::sort(pool.begin(), pool.end(), MonthRankDesc);
        for (std::size_t t = 0;
             t < pool.size() && t + 1 < kTotalMonthRankCount; ++t)
            total[t + 1] = pool[t];

        std::size_t top = 0;                     // COUNTRY_DEFUGEL
        for (std::size_t c = 0; c < kMonthRankCountryCount; ++c)
            if (table[top][0].total_point < table[c][0].total_point)
                top = c;
        for (std::size_t c = 0; c < kMonthRankCountryCount; ++c)
        {
            if (c == top)
                total[0] = table[c][0];
            else
                total[kTotalMonthRankCount - c - 1] = table[c][0];
            // (When top == 0 the country-2 warlord overwrites slot
            // 48 of the sorted pool - legacy collision kept as-is.)
        }
    }

    // 3. LastFameRank <- total[0..8] (mutates even when the persist
    // later fails - legacy does this in the SM phase).
    {
        MonthRankRegistry::FameRank fame{};
        for (std::size_t t = 0; t < kFameRankCount; ++t)
            fame[t] = total[t];
        ctx.month_rank->SetLastFameRank(fame);
    }

    // 4. Row list with total-rank indices (legacy k-trick: slot 0
    // matches only once).
    struct Row { std::uint8_t t; std::uint8_t j; MonthRanker r; };
    std::vector<Row> rows;
    {
        std::size_t k = 0;
        for (std::size_t c = 0; c < kMonthRankCountryCount; ++c)
        {
            for (std::size_t j = 0; j < kFirstGradeGroupCount; ++j)
            {
                std::size_t t = k;
                for (; t < kTotalMonthRankCount; ++t)
                    if (table[c][j].char_id == total[t].char_id)
                        break;
                rows.push_back({static_cast<std::uint8_t>(t),
                                static_cast<std::uint8_t>(j),
                                table[c][j]});
                if (t == 0)
                    k = 1;
            }
        }
    }

    // 5. Persist (one offload task - mirrors the DM round-trip).
    struct Outcome
    {
        bool success = false;
        std::optional<MonthRanker> new_top;
    };
    const Outcome oc = co_await fourstory::db::CoOffloadIf(
        ctx.db_pool,
        [repo = ctx.month_rank_repo, month, rows]() -> Outcome
        {
            Outcome out{};
            if (!repo->InitMonthRank(month))
                return out;
            std::uint32_t top_point = 0;
            for (const auto& row : rows)
            {
                if (row.r.char_id == 0 ||
                    (row.j != 0 && row.r.month_point == 0))
                    continue;                    // legacy skip
                if (row.t == 0)
                    top_point = row.r.total_point;
                if (!repo->SaveRanker(month, row.j, row.t, row.r))
                    return out;                  // first failure stops
            }
            std::uint8_t next = static_cast<std::uint8_t>(month + 1);
            if (next > 12) next -= 12;
            if (!repo->InitMonthRank(next))
                return out;
            if (!repo->InitMonthPvPoint(month, top_point, out.new_top))
                return out;
            out.success = true;
            return out;
        });

    if (!oc.success)
    {
        spdlog::warn("RunMonthRankRollover: persist for month {} "
                     "failed - reset/broadcast skipped (legacy "
                     "bRet=FALSE drop)", month);
        co_return;
    }

    // 6. ACK phase (legacy SSHandler.cpp:11200).
    if (oc.new_top)
    {
        auto fame = ctx.month_rank->LastFameRank();
        fame[0] = *oc.new_top;
        ctx.month_rank->SetLastFameRank(fame);
    }
    {
        MonthRankRegistry::FirstGrade group{};
        for (std::size_t c = 0; c < kMonthRankCountryCount; ++c)
            for (std::size_t j = 0; j < kFirstGradeGroupCount; ++j)
                group[c][j] = table[c][j];
        ctx.month_rank->SetFirstGradeGroup(group);
    }

    const auto fame  = ctx.month_rank->LastFameRank();
    const auto group = ctx.month_rank->FirstGradeGroup();
    for (auto& p : ctx.peers->Snapshot())
    {
        co_await senders::SendMwMonthRankResetReq(p, month, fame);
        co_await senders::SendMwFirstGradeGroupReq(p, month, group);
    }

    ctx.month_rank->ResetForNewMonth();
    spdlog::info("RunMonthRankRollover: month {} closed - {} row(s) "
                 "eligible, new month {}", month, rows.size(),
                 ctx.month_rank->RankMonth());
    co_return;
}

boost::asio::awaitable<void>
OnSmMonthRankSaveReq(std::shared_ptr<PeerSession> peer,
                     std::vector<std::byte>       body,
                     const HandlerContext&        ctx)
{
    if (!body.empty())
        spdlog::debug("OnSmMonthRankSaveReq[{}]: ignored unexpected "
                      "body ({} bytes)",
            peer->Wire()->RemoteIPv4(), body.size());
    co_await RunMonthRankRollover(ctx);
}

} // namespace tworldsvr::handlers
