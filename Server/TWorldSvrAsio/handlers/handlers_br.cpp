#include "handlers.h"
#include "../senders/senders.h"
#include "../services/br_constants.h"
#include "../wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <mutex>
#include <string>
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

// UpdateBRTeam — broadcast MW_UPDATEBRTEAM_ACK to every member of
// the chief's team, each on their own map peer (legacy
// TWorldSvr.cpp:8014). Snapshots the team under the registry's
// read lock; sends outside the lock.
boost::asio::awaitable<void>
BroadcastUpdateBrTeam(const HandlerContext& ctx, std::uint32_t chief_id)
{
    if (!ctx.br || !ctx.chars) co_return;
    std::string chief_name;
    bool        team_ready = false;
    auto roster = ctx.br->SnapshotTeam(chief_id, chief_name, team_ready);
    if (roster.empty()) co_return;

    // Per-recipient send. The roster snapshot includes ready flags
    // which the sender ships verbatim — same shape for every
    // recipient, only the leading (char_id, key) differs.
    std::vector<senders::UpdateBrTeamRow> rows;
    rows.reserve(roster.size());
    for (const auto& p : roster)
        rows.push_back({p.char_id, p.name, p.ready ? std::uint8_t{1} : std::uint8_t{0}});

    for (const auto& p : roster)
    {
        auto member_ch = ctx.chars->Find(p.char_id);
        if (!member_ch) continue;
        std::uint8_t  msi = 0;
        std::uint32_t key = 0;
        {
            std::lock_guard g(member_ch->lock);
            msi = member_ch->main_server_id;
            key = member_ch->key;
        }
        auto map_peer = FindMapPeer(ctx, msi);
        if (!map_peer) continue;
        co_await senders::SendMwUpdateBrTeamAck(map_peer, p.char_id, key,
            chief_name, team_ready ? std::uint8_t{1} : std::uint8_t{0}, rows);
    }
}

} // namespace

boost::asio::awaitable<void>
OnAddToBrQueueReq(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.br)
    {
        spdlog::warn("OnAddToBrQueueReq[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  only_ready = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(only_ready))
    {
        spdlog::warn("OnAddToBrQueueReq[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto ch = ctx.chars->Find(char_id);
    if (!ch) co_return;

    std::uint8_t  main_id = 0, klass = 0;
    std::string   name;
    bool          key_ok = true;
    {
        std::lock_guard g(ch->lock);
        if (ch->key != key) key_ok = false;
        else
        {
            main_id = ch->main_server_id;
            klass   = ch->klass;
            name    = ch->name;
        }
    }
    if (!key_ok) co_return;

    if (only_ready)
    {
        // Ready signal — flag the team or the solo player and
        // broadcast the team refresh (legacy parity SSHandler.cpp:14152).
        bool changed = false;
        if (ctx.br->GetPremadePlayerCountByChief(char_id) > 0)
            changed = ctx.br->FlagTeamReady(char_id);
        else
            changed = ctx.br->FlagPlayerReady(char_id, key);
        if (changed)
        {
            const std::uint32_t chief = ctx.br->GetChiefIdByMateId(char_id);
            if (chief)
                co_await BroadcastUpdateBrTeam(ctx, chief);
        }
        spdlog::info("OnAddToBrQueueReq[{}]: char_id={} ready signal "
                     "(changed={})", ip, char_id, changed);
        co_return;
    }

    const std::uint8_t result = ctx.br->AddPlayerToQueue(char_id, key,
        klass, name);
    const std::uint32_t tick = ctx.br->Tick();

    auto main_peer = FindMapPeer(ctx, main_id);
    if (!main_peer) co_return;

    spdlog::info("OnAddToBrQueueReq[{}]: char_id={} → {}", ip, char_id, result);
    co_await senders::SendMwAddToBrQueueAck(main_peer, result, char_id, key,
        tick);
}

boost::asio::awaitable<void>
OnBrTeamMateAddReq(std::shared_ptr<PeerSession> peer,
                   std::vector<std::byte>       body,
                   const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.br)
    {
        spdlog::warn("OnBrTeamMateAddReq[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::string   mate_name;
    if (!r.Read(char_id) || !r.Read(key) || !r.ReadString(mate_name))
    {
        spdlog::warn("OnBrTeamMateAddReq[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto inviter = ctx.chars->Find(char_id);
    if (!inviter) co_return;
    std::uint8_t  inviter_main = 0;
    std::string   inviter_name;
    bool          key_ok = true;
    {
        std::lock_guard g(inviter->lock);
        if (inviter->key != key) key_ok = false;
        else
        {
            inviter_main = inviter->main_server_id;
            inviter_name = inviter->name;
        }
    }
    if (!key_ok) co_return;

    auto mate = ctx.chars->FindByName(mate_name);
    if (!mate || mate.get() == inviter.get())
    {
        // Target offline or chief tried to invite themselves → reply
        // NOTFOUND on the chief's map (legacy SSHandler.cpp:14202).
        if (auto p = FindMapPeer(ctx, inviter_main))
            co_await senders::SendMwBrTeamMateAddAck(p, br::kTeamAddNotFound,
                char_id, key, /*name=*/"");
        co_return;
    }

    std::uint8_t  mate_main = 0;
    std::uint32_t mate_id = 0, mate_key = 0;
    {
        std::lock_guard g(mate->lock);
        mate_main = mate->main_server_id;
        mate_id   = mate->char_id;
        mate_key  = mate->key;
    }

    // Forward the invite to the *target's* map carrying the
    // requester's name (legacy SSHandler.cpp:14224). The actual team
    // mutation happens on the BRTEAMMATEADDRESULT_ACK reply.
    if (auto p = FindMapPeer(ctx, mate_main))
        co_await senders::SendMwBrTeamMateAddAck(p, br::kTeamAddSuccess,
            mate_id, mate_key, inviter_name);
    spdlog::info("OnBrTeamMateAddReq[{}]: char_id={} invited '{}' (mate={})",
        ip, char_id, mate_name, mate_id);
}

boost::asio::awaitable<void>
OnBrTeamMateDelReq(std::shared_ptr<PeerSession> peer,
                   std::vector<std::byte>       body,
                   const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.br)
    {
        spdlog::warn("OnBrTeamMateDelReq[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::string   target_name;
    if (!r.Read(char_id) || !r.Read(key) || !r.ReadString(target_name))
    {
        spdlog::warn("OnBrTeamMateDelReq[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto requester = ctx.chars->Find(char_id);
    auto target    = ctx.chars->FindByName(target_name);
    if (!requester || !target) co_return;

    // Legacy gate: drop the target only when the requester has a
    // premade team OR is dropping themselves (SSHandler.cpp:14250).
    const bool requester_is_chief =
        ctx.br->GetPremadePlayerCountByChief(char_id) > 0;
    if (!requester_is_chief && requester.get() != target.get()) co_return;

    std::uint32_t target_id = 0;
    {
        std::lock_guard g(target->lock);
        target_id = target->char_id;
    }
    ctx.br->ErasePlayerFromPremade(target_id);
    spdlog::info("OnBrTeamMateDelReq[{}]: char_id={} dropped '{}' (target={})",
        ip, char_id, target_name, target_id);
    co_return;
}

boost::asio::awaitable<void>
OnBrTeamMateAddResultAck(std::shared_ptr<PeerSession> peer,
                         std::vector<std::byte>       body,
                         const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.br)
    {
        spdlog::warn("OnBrTeamMateAddResultAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t mate_id = 0, mate_key = 0;
    std::uint8_t  result = 0;
    std::string   chief_name;
    if (!r.Read(mate_id) || !r.Read(mate_key) || !r.Read(result) ||
        !r.ReadString(chief_name))
    {
        spdlog::warn("OnBrTeamMateAddResultAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    if (result != br::kTeamAddSuccess)
    {
        // Refuse / busy / etc — forward the result code to the
        // chief's map (legacy SSHandler.cpp:14327 default branch).
        auto chief = ctx.chars->FindByName(chief_name);
        if (!chief) co_return;
        std::uint8_t  chief_main = 0;
        std::uint32_t chief_id = 0, chief_key = 0;
        {
            std::lock_guard g(chief->lock);
            chief_main = chief->main_server_id;
            chief_id   = chief->char_id;
            chief_key  = chief->key;
        }
        if (auto p = FindMapPeer(ctx, chief_main))
            co_await senders::SendMwBrTeamMateAddAck(p, result, chief_id,
                chief_key, /*name=*/"");
        co_return;
    }

    // SUCCESS — promote the mate into the chief's premade team
    // (with the two cap gates the legacy enforces).
    auto mate  = ctx.chars->Find(mate_id);
    auto chief = ctx.chars->FindByName(chief_name);
    if (!mate || !chief) co_return;

    std::uint8_t  mate_class = 0, chief_main = 0, chief_class = 0;
    std::string   mate_name;
    std::uint32_t chief_id = 0, chief_key = 0;
    {
        std::lock_guard g(mate->lock);
        if (mate->key != mate_key) co_return;
        mate_class = mate->klass;
        mate_name  = mate->name;
    }
    {
        std::lock_guard g(chief->lock);
        chief_id    = chief->char_id;
        chief_key   = chief->key;
        chief_main  = chief->main_server_id;
        chief_class = chief->klass;
    }

    // Already in a team or chief at the cap → reply chief
    // ALREADYINTEAM (legacy parity SSHandler.cpp:14285 / 14299).
    if (ctx.br->FindPlayerInPremade(mate_id) ||
        ctx.br->GetPremadePlayerCountByChief(chief_id) + 1
            > br::kTeamMaxCount3v3)
    {
        if (auto p = FindMapPeer(ctx, chief_main))
            co_await senders::SendMwBrTeamMateAddAck(p, br::kTeamAddAlreadyInTeam,
                chief_id, chief_key, /*name=*/"");
        co_return;
    }

    ctx.br->JoinPremadeTeam(chief_id, chief_key, chief_class, chief_name,
        mate_id, mate_key, mate_class, mate_name);
    spdlog::info("OnBrTeamMateAddResultAck[{}]: mate={} joined chief={} "
                 "('{}')", ip, mate_id, chief_id, chief_name);
    co_await BroadcastUpdateBrTeam(ctx, chief_id);
}

boost::asio::awaitable<void>
OnVoteForBrMapReq(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.br)
    {
        spdlog::warn("OnVoteForBrMapReq[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::string   map_name;
    std::uint8_t  mode = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.ReadString(map_name) ||
        !r.Read(mode))
    {
        spdlog::warn("OnVoteForBrMapReq[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto ch = ctx.chars->Find(char_id);
    if (!ch) co_return;
    std::uint32_t user_id = 0;
    bool          key_ok = true;
    {
        std::lock_guard g(ch->lock);
        if (ch->key != key) key_ok = false;
        else                user_id = ch->user_id;
    }
    if (!key_ok) co_return;

    // Legacy: map name takes priority over mode; mode == 0xFF (-1)
    // means "no mode vote".
    if (!map_name.empty())
    {
        ctx.br->VoteForMap(user_id, map_name);
        spdlog::info("OnVoteForBrMapReq[{}]: user_id={} voted map '{}'", ip,
            user_id, map_name);
    }
    else if (mode != 0xFF)
    {
        ctx.br->VoteForMode(user_id, mode);
        spdlog::info("OnVoteForBrMapReq[{}]: user_id={} voted mode {}", ip,
            user_id, mode);
    }
    co_return;
}

} // namespace tworldsvr::handlers
