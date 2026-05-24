#include "handlers.h"
#include "../senders/senders.h"
#include "../services/bow_constants.h"
#include "../services/br_constants.h"
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

    std::uint8_t result = ctx.bow->RemovePlayer(char_id, key);
    // Legacy fall-through (SSHandler.cpp:14078): if the Bow remove
    // missed, also try the BR queue — the player might have been
    // queued there instead. The W6-25 BrRegistry::ErasePlayerFromQueue
    // returns the same BOWREG_* enum, so we forward whichever
    // succeeded; the legacy `max(bow.tick, br.tick)` collapses to
    // bow.tick here because both modules currently have a 0 tick.
    if (result == bow::kFail && ctx.br)
        result = ctx.br->ErasePlayerFromQueue(char_id, key);
    const std::uint32_t tick = ctx.bow->Tick();

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
        ctx.bow->ReleaseSinglePlayer(char_id, key);
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

    // Quiescent payload — same shape the legacy emits when both
    // pBOW + pBR are null (SSSender.cpp:3962 / 3978). The scheduler
    // / status state machine isn't ported yet, so neither registry
    // exposes the running-match fields the legacy would otherwise
    // forward (m_bStatus / m_dwStart / m_bWinner / m_bType).
    co_await senders::SendMwBattleModeStatusAck(main_peer, char_id, key,
        /*bow_status=*/0, /*bow_start=*/0, /*bow_winner=*/kCountryN,
        /*br_status=*/0,  /*br_start=*/0,  /*br_type=*/0);
    spdlog::info("OnBattleModeStatusReq[{}]: char_id={} → quiescent status",
        ip, char_id);
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
        // (SSHandler.cpp:14397). Our BowRegistry::AddPlayer doesn't
        // model the BS_ALARM gate or the Admin bypass — it accepts
        // unconditionally — so calling it with country=C is enough.
        const std::uint8_t result = ctx.bow->AddPlayer(char_id, key,
            bow::kCountryC, guild_hint);
        spdlog::info("OnCmTeleportBattleModeReq[{}]: char_id={} → SYSTEM_BOW "
                     "result={}", ip, char_id, result);
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
