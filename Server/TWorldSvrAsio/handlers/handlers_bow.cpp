#include "handlers.h"
#include "../senders/senders.h"
#include "../services/bow_constants.h"
#include "../services/br_constants.h"
#include "../wire_codec.h"

#include "MessageId.h"

#include "fourstory/db/co_offload.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
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

namespace {

// TeleportBOWPlayer(map, TRUE) (TWorldSvr.cpp:7875): roster to the
// BOW server, per-char team stamp + PREPAREFORBOW to their main map
// + the TAddBOWPlayer leg.
boost::asio::awaitable<void>
TeleportBowPlayersIn(const HandlerContext&          ctx,
                     const std::vector<TBowPlayer>& roster)
{
    auto bow_server =
        ctx.bow_server ? ctx.bow_server->Get() : nullptr;
    if (bow_server)
        co_await senders::SendMwAddBowPlayersAck(bow_server, roster);

    for (const auto& p : roster)
    {
        if (!ctx.chars)
            break;
        auto c = ctx.chars->Find(p.char_id);
        if (!c)
            continue;
        std::uint8_t  msi = 0;
        std::uint32_t user_id = 0;
        bool key_ok = false;
        {
            std::lock_guard g(c->lock);
            if (c->key == p.key)
            {
                key_ok = true;
                // Legacy re-stamps the char's country to the team.
                if (c->country != p.country)
                    c->country = p.country;
                msi     = c->main_server_id;
                user_id = c->user_id;
            }
        }
        if (!key_ok)
            continue;
        if (auto main_peer = FindMapPeer(ctx, msi))
            co_await senders::SendMwPrepareForBowReq(main_peer,
                p.char_id, p.key, p.country);
        if (ctx.bow_repo)
            co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                [repo = ctx.bow_repo, cid = p.char_id, user_id]
                { repo->AddPlayer(cid, user_id); });
    }
    co_return;
}

// NotifyBOWNonQueuedPlayers (TWorldSvr.cpp:7943): every online char
// NOT on the Bow roster gets the battle-started nudge on their main.
boost::asio::awaitable<void>
NotifyBowNonQueuedPlayers(const HandlerContext& ctx)
{
    if (!ctx.chars || !ctx.bow)
        co_return;
    // Legacy stages the ids in a std::map (ascending char id) —
    // the shard snapshot is unordered, so sort to keep the frame
    // order byte-identical.
    auto ids = ctx.chars->SnapshotIds();
    std::sort(ids.begin(), ids.end());
    for (auto id : ids)
    {
        if (ctx.bow->Contains(id))
            continue;
        auto c = ctx.chars->Find(id);
        if (!c)
            continue;
        std::uint32_t key = 0;
        std::uint8_t  msi = 0;
        {
            std::lock_guard g(c->lock);
            key = c->key;
            msi = c->main_server_id;
        }
        if (auto main_peer = FindMapPeer(ctx, msi))
            co_await senders::SendMwNotifyNonQueuedPlayerAck(
                main_peer, id, key);
    }
    co_return;
}

// BOWNotify (TWorldSvr.cpp:7868): the status frame to every map.
boost::asio::awaitable<void>
BroadcastBowNotify(const HandlerContext& ctx, std::uint8_t status,
                   std::uint32_t second, std::uint8_t d,
                   std::uint8_t c)
{
    if (!ctx.peers)
        co_return;
    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwBowTimeUpdateAck(p, status, second,
            d, c);
    co_return;
}

} // namespace

boost::asio::awaitable<void>
RunBowTick(const HandlerContext& ctx, std::uint32_t clt,
           std::uint64_t now_ms)
{
    if (!ctx.bow)
        co_return;
    auto bow_server =
        ctx.bow_server ? ctx.bow_server->Get() : nullptr;
    const auto acts = ctx.bow->Tick(clt, now_ms,
        bow_server != nullptr);

    // Emission order mirrors the legacy per-path sequences:
    // BODPEACE fires BOW_END (once), then the end fan-out, then the
    // notify; the battle edge notifies first, then BOW_START + the
    // non-queued nudge; match creation teleports between notifies.
    for (const std::uint8_t cmd : acts.commands)
        if (cmd == bow::kCmdEnd && bow_server)
            co_await senders::SendMwBowCommandExecReq(bow_server,
                cmd);
    if (acts.ended)
    {
        if (bow_server)
            co_await senders::SendMwEndBowWarReq(bow_server,
                acts.winner, acts.end_roster);
        if (ctx.bow_repo)
            co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                [repo = ctx.bow_repo] { repo->ClearPlayers(); });
    }
    if (!acts.notifies.empty())
    {
        const auto& n = acts.notifies.front();
        co_await BroadcastBowNotify(ctx, n.status, n.second,
            n.points_d, n.points_c);
    }
    if (acts.teleport_in)
        co_await TeleportBowPlayersIn(ctx, acts.roster);
    for (const std::uint8_t cmd : acts.commands)
        if (cmd == bow::kCmdStart && bow_server)
            co_await senders::SendMwBowCommandExecReq(bow_server,
                cmd);
    if (acts.notify_nonqueued)
        co_await NotifyBowNonQueuedPlayers(ctx);
    for (std::size_t i = 1; i < acts.notifies.size(); ++i)
        co_await BroadcastBowNotify(ctx, acts.notifies[i].status,
            acts.notifies[i].second, acts.notifies[i].points_d,
            acts.notifies[i].points_c);
    co_return;
}

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

    // Legacy gates (SSHandler.cpp:14039-14044): no module → silent
    // drop; the main-map resolve precedes AddPlayerToQueue, so an
    // absent main peer suppresses the mutation too.
    if (!ctx.bow->Configured())
        co_return;
    auto main_peer = FindMapPeer(ctx, static_cast<std::uint8_t>(main_id));
    if (!main_peer) co_return;

    const auto out = ctx.bow->AddPlayer(char_id, key,
        effective_country, guild_hint);
    // The BS_PEACE late-join path teleports the char straight into
    // the running match (BowSystem.cpp:114 TeleportBOWPlayer).
    if (out.late_join)
        co_await TeleportBowPlayersIn(ctx,
            std::vector<TBowPlayer>{out.player});
    const std::uint32_t tick = ctx.bow->Tick();

    spdlog::info("OnAddToBowQueueReq[{}]: char_id={} country={} guild={} → {}",
        ip, char_id, effective_country, guild_hint, out.result);
    co_await senders::SendMwAddToBowQueueAck(main_peer, out.result,
        char_id, key, tick);
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

    // Legacy `!m_pBOWModule` gate (SSHandler.cpp:14074): with no
    // TBOWSETTINGSCHART row the whole request drops silently —
    // including the BR fallback and the ack.
    if (!ctx.bow->Configured())
        co_return;

    std::uint8_t result = ctx.bow->RemovePlayer(char_id, key);
    // Legacy fall-through (SSHandler.cpp:14078): if the Bow remove
    // missed, also try the BR queue — the player might have been
    // queued there instead. The W6-25 BrRegistry::ErasePlayerFromQueue
    // returns the same BOWREG_* enum.
    if (result == bow::kFail && ctx.br)
        result = ctx.br->ErasePlayerFromQueue(char_id, key);
    // Legacy echoes max(bow tick, BR tick) (SSHandler.cpp:14094).
    // The BR tick goes live when its scheduler ports.
    const std::uint32_t tick = std::max(ctx.bow->Tick(),
        ctx.br ? ctx.br->Tick() : 0u);

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

    const auto out = ctx.bow->UpdatePoints(country,
        static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count()));
    if (!out.configured)
        co_return;
    spdlog::info("OnBowPointsUpdateReq[{}]: country={} → D={} C={}", ip,
        country, out.points_d, out.points_c);
    // Legacy UpdatePoints ends in a BOWNotify broadcast
    // (BowSystem.cpp:567).
    co_await BroadcastBowNotify(ctx, out.status, out.second,
        out.points_d, out.points_c);
    co_return;
}

// --- W6-26 leave-battlefield cleanup ------------------------------

boost::asio::awaitable<void>
OnLeaveBattlefieldReq(std::shared_ptr<PeerSession> peer,
                      std::vector<std::byte>       body,
                      const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars)
    {
        spdlog::warn("OnLeaveBattlefieldReq[{}]: char registry not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnLeaveBattlefieldReq[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto ch = ctx.chars->Find(char_id);
    if (!ch) co_return;

    std::uint8_t  channel = 0;
    std::uint16_t map_id = 0;
    {
        std::lock_guard g(ch->lock);
        // Legacy FindTChar(id, key) (SSHandler.cpp:14122) — a stale
        // key is a total no-op.
        if (ch->key != key)
            co_return;
        channel = ch->channel;
        map_id  = ch->map_id;
    }

    // Legacy SSHandler.cpp:14125 routing: BR takes priority via the
    // channel marker (chars route through the BR battleground via
    // channel = BR_SERVER_ID), else Bow via the map_id sentinel.
    if (channel == br::kBrServerId && ctx.br)
    {
        spdlog::info("OnLeaveBattlefieldReq[{}]: char_id={} → BR cleanup",
            ip, char_id);
        ctx.br->ReleaseSinglePlayer(char_id, key);
    }
    else if (map_id == bow::kBowMapId && ctx.bow)
    {
        spdlog::info("OnLeaveBattlefieldReq[{}]: char_id={} → Bow cleanup",
            ip, char_id);
        const auto out = ctx.bow->ReleaseSinglePlayer(char_id, key);
        if (out.release)
        {
            // TeleportBOWPlayer single-char (TWorldSvr.cpp:7918).
            if (auto bow_server =
                    ctx.bow_server ? ctx.bow_server->Get() : nullptr)
                co_await senders::SendMwReleaseSingleBowPlayerReq(
                    bow_server, char_id, key, out.winner);
            std::uint32_t user_id = 0;
            {
                std::lock_guard g(ch->lock);
                user_id = ch->user_id;
            }
            if (ctx.bow_repo)
                co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                    [repo = ctx.bow_repo, user_id]
                    { repo->DeleteSinglePlayer(user_id); });
        }
    }
    // Else: not on a known battlefield map — no-op.
    co_return;
}

// --- W6-27 BattleMode status + CM teleport ------------------------

namespace {
// SYSTEM_BOW / SYSTEM_BR (NetCode.h:2733).
constexpr std::uint8_t kSystemBow = 0;
constexpr std::uint8_t kSystemBr  = 1;

// Legacy TCONTRY_N (NetCode.h:1096) — emitted as the "no winner"
// sentinel in MW_BATTLEMODESTATUS_ACK when no Bow match is running.
constexpr std::uint8_t kCountryN  = 3;
} // namespace

boost::asio::awaitable<void>
OnBattleModeStatusReq(std::shared_ptr<PeerSession> peer,
                      std::vector<std::byte>       body,
                      const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnBattleModeStatusReq[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnBattleModeStatusReq[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto ch = ctx.chars->Find(char_id);
    if (!ch) co_return;

    std::uint8_t  main_id = 0;
    bool          key_ok = true;
    {
        std::lock_guard g(ch->lock);
        if (ch->key != key) key_ok = false;
        else                main_id = ch->main_server_id;
    }
    if (!key_ok) co_return;

    auto main_peer = FindMapPeer(ctx, main_id);
    if (!main_peer) co_return;

    // W6-54: the Bow half is live (legacy SSSender.cpp:3954 forwards
    // m_bStatus / m_dwStart / m_bWinner when pBOW exists); the BR
    // half stays quiescent until its scheduler ports.
    std::uint8_t  bow_status = 0;
    std::uint32_t bow_start  = 0;
    std::uint8_t  bow_winner = kCountryN;
    if (ctx.bow && ctx.bow->Configured())
    {
        bow_status = ctx.bow->Status();
        bow_start  = ctx.bow->NextStart();
        bow_winner = ctx.bow->Winner();
    }
    co_await senders::SendMwBattleModeStatusAck(main_peer, char_id, key,
        bow_status, bow_start, bow_winner,
        /*br_status=*/0,  /*br_start=*/0,  /*br_type=*/0);
    spdlog::info("OnBattleModeStatusReq[{}]: char_id={} → bow status={}",
        ip, char_id, bow_status);
}

boost::asio::awaitable<void>
OnCmTeleportBattleModeReq(std::shared_ptr<PeerSession> peer,
                          std::vector<std::byte>       body,
                          const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars)
    {
        spdlog::warn("OnCmTeleportBattleModeReq[{}]: char registry not wired",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  system_type = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(system_type))
    {
        spdlog::warn("OnCmTeleportBattleModeReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    auto ch = ctx.chars->Find(char_id);
    if (!ch) co_return;

    std::uint32_t guild_hint = 0;
    bool          key_ok = true;
    {
        std::lock_guard g(ch->lock);
        if (ch->key != key) key_ok = false;
        else
            guild_hint = ch->tactics_guild_id
                ? ch->tactics_guild_id
                : ch->guild_id;
    }
    if (!key_ok) co_return;

    switch (system_type)
    {
    case kSystemBow:
    {
        if (!ctx.bow)
        {
            spdlog::warn("OnCmTeleportBattleModeReq[{}]: bow not wired", ip);
            co_return;
        }
        // Legacy hard-codes TCONTRY_C + Admin=TRUE here
        // (SSHandler.cpp:14397) — the admin bypass rides the
        // late-join branch when a match is running.
        if (!ctx.bow->Configured())
            co_return;
        const auto out = ctx.bow->AddPlayer(char_id, key,
            bow::kCountryC, guild_hint, /*admin=*/true);
        if (out.late_join)
            co_await TeleportBowPlayersIn(ctx,
                std::vector<TBowPlayer>{out.player});
        spdlog::info("OnCmTeleportBattleModeReq[{}]: char_id={} → SYSTEM_BOW "
                     "result={}", ip, char_id, out.result);
        break;
    }
    case kSystemBr:
        // Legacy body is empty here (SSHandler.cpp:14400 — a TODO
        // in the original). No-op until the BR force-add semantics
        // are defined.
        spdlog::info("OnCmTeleportBattleModeReq[{}]: char_id={} → SYSTEM_BR "
                     "(legacy no-op)", ip, char_id);
        break;
    default:
        spdlog::warn("OnCmTeleportBattleModeReq[{}]: char_id={} unknown "
                     "system_type={}", ip, char_id, system_type);
        break;
    }
    co_return;
}

} // namespace tworldsvr::handlers
