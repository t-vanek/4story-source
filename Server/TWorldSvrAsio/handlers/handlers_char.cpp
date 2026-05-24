#include "handlers.h"
#include "../senders/senders.h"
#include "../services/friend_constants.h"
#include "../services/soulmate_constants.h"
#include "../wire_codec.h"

#include "MessageId.h"

#include "fourstory/db/co_offload.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace tworldsvr::handlers {

namespace {

// Special map-server ids (NetCode.h): the Bow battleground (30) and
// Battle Royale (50) instances. A main-session handoff *into* one of
// these is not the normal map-to-map flow, so W6-16's ENTERSVR_ACK
// handoff branch excludes them (legacy OnMW_ENTERSVR_ACK:1343).
constexpr std::uint8_t kBowServerId = 30;
constexpr std::uint8_t kBrServerId  = 50;

// Hydrate a freshly-online char's social graph from a FriendLoad.
// Friend type is derived from the forward/reverse intersection
// (legacy OnDM_FRIENDLIST_ACK): forward-only = FT_FRIEND, mutual =
// FT_FRIENDFRIEND, reverse-only = FT_TARGET. `connected` is resolved
// live from the registry (CharRegistry::Find is shard-lock only —
// safe to call while holding the char's entity lock; region stays 0
// and is resolved live by the FRIENDLIST reader).
void ApplyFriendLoad(const HandlerContext& ctx, TChar& ch,
                     const FriendLoad& fl)
{
    std::unordered_set<std::uint32_t> reverse_ids, forward_ids;
    for (const auto& r : fl.reverse) reverse_ids.insert(r.id);

    std::lock_guard g(ch.lock);

    ch.friend_groups.clear();
    for (const auto& [grp, name] : fl.groups)
        ch.friend_groups.emplace_back(grp, name);

    ch.friends.clear();
    for (const auto& f : fl.forward)
    {
        forward_ids.insert(f.id);
        TFriend tf;
        tf.id        = f.id;
        tf.name      = f.name;
        tf.group     = f.group;
        tf.type      = reverse_ids.count(f.id) ? frnd::kTypeFriendFriend
                                               : frnd::kTypeFriend;
        tf.connected = ctx.chars && ctx.chars->Find(f.id) != nullptr;
        ch.friends.push_back(std::move(tf));
    }
    for (const auto& r : fl.reverse)
    {
        if (forward_ids.count(r.id)) continue; // already a mutual friend
        TFriend tf;
        tf.id        = r.id;
        tf.name      = r.name;
        tf.type      = frnd::kTypeTarget;
        tf.connected = ctx.chars && ctx.chars->Find(r.id) != nullptr;
        ch.friends.push_back(std::move(tf));
    }

    if (fl.has_soulmate && fl.soulmate_target != 0)
    {
        ch.soulmate = TSoulmate{};
        ch.soulmate.target    = fl.soulmate_target;
        ch.soulmate.connected =
            ctx.chars && ctx.chars->Find(fl.soulmate_target) != nullptr;
    }
}

} // namespace

boost::asio::awaitable<void>
OnAddCharAck(std::shared_ptr<PeerSession>  peer,
             std::vector<std::byte>        body,
             const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0;
    std::uint32_t key     = 0;
    std::uint32_t ip_addr = 0;
    std::uint16_t port    = 0;
    std::uint32_t user_id = 0;

    if (!r.Read(char_id) || !r.Read(key) || !r.Read(ip_addr) ||
        !r.Read(port)    || !r.Read(user_id))
    {
        spdlog::warn("OnAddCharAck[{}]: short body ({} bytes) — dropped",
            ip, body.size());
        co_return;
    }

    if (!ctx.chars)
    {
        spdlog::warn("OnAddCharAck[{}]: char registry not wired — dropped",
            ip);
        co_return;
    }

    // The server byte that owns this char's main session — the
    // legacy LOBYTE(pSERVER->m_wID). Now sourced from the peer
    // wrapper (set by OnRelaysvrReq).
    const std::uint8_t server_id =
        static_cast<std::uint8_t>(peer->Wid() & 0xFF);

    if (auto existing = ctx.chars->Find(char_id))
    {
        std::lock_guard g(existing->lock);
        const bool any_match = std::any_of(
            existing->cons.begin(), existing->cons.end(),
            [&](const TCharCon& c) {
                return c.server_id == server_id
                    && c.ip_addr   == ip_addr
                    && c.port      == port;
            });
        if (existing->key == key && !any_match)
        {
            existing->cons.push_back(TCharCon{
                server_id, ip_addr, port, /*ready*/ false, /*valid*/ true,
            });
            spdlog::info("OnAddCharAck[{}]: additional connect char_id={} "
                         "user_id={} server_id={}", ip, char_id, user_id,
                server_id);
            co_return;
        }
        spdlog::warn("OnAddCharAck[{}]: char_id={} already registered with "
                     "key=0x{:08X} (incoming key=0x{:08X}) — dropping (W3 "
                     "will fire MW_INVALIDCHAR_REQ)",
            ip, char_id, existing->key, key);
        co_return;
    }

    auto ch = std::make_shared<TChar>();
    ch->char_id          = char_id;
    ch->key              = key;
    ch->user_id          = user_id;
    ch->main_server_id   = server_id;
    ch->logout           = false;
    ch->saving           = false;
    ch->db_loading       = false;
    ch->chg_main_id      = 0;
    ch->cons.push_back(TCharCon{
        server_id, ip_addr, port, /*ready*/ false, /*valid*/ true,
    });

    if (!ctx.chars->Insert(std::move(ch)))
    {
        spdlog::info("OnAddCharAck[{}]: lost insert race for char_id={}",
            ip, char_id);
        co_return;
    }
    ctx.chars->MarkUserActive(user_id);

    // W4-15: hydrate the char's friends / groups / soulmate from the
    // persistence layer (legacy loaded this at login via the
    // DM_FRIENDLIST round-trip). Offloaded so SOCI never blocks the
    // io_context; skipped when no repo is wired.
    if (ctx.friend_repo)
    {
        auto fl = co_await fourstory::db::CoOffloadIf(ctx.db_pool,
            [repo = ctx.friend_repo, char_id]
            { return repo->LoadForChar(char_id); });
        if (auto self = ctx.chars->Find(char_id))
            ApplyFriendLoad(ctx, *self, fl);
    }

    spdlog::info("OnAddCharAck[{}]: char_id={} key=0x{:08X} user_id={} "
                 "server_id={} ip={}.{}.{}.{}:{} — registered (total={}, "
                 "active_users={})",
        ip, char_id, key, user_id, server_id,
        (ip_addr >>  0) & 0xFF, (ip_addr >>  8) & 0xFF,
        (ip_addr >> 16) & 0xFF, (ip_addr >> 24) & 0xFF, port,
        ctx.chars->Size(), ctx.chars->ActiveUserCount());

    // TODO W3a-3: SendMW_ENTERSVR_REQ(peer, true, char_id, key).
    co_return;
}

boost::asio::awaitable<void>
OnEnterSvrAck(std::shared_ptr<PeerSession>  peer,
              std::vector<std::byte>        body,
              const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars)
    {
        spdlog::warn("OnEnterSvrAck[{}]: char registry not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, region = 0, rank_point = 0, user_ip = 0;
    std::string   name;
    std::uint8_t  level = 0, real_sex = 0, klass = 0, race = 0, sex = 0,
                  face = 0, hair = 0, helmet_hide = 0, country = 0,
                  aid_country = 0, channel = 0, logout = 0, save = 0,
                  result = 0;
    std::uint16_t map_id = 0, title_id = 0;
    float         pos_x = 0, pos_y = 0, pos_z = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.ReadString(name) ||
        !r.Read(level) || !r.Read(real_sex) || !r.Read(klass) ||
        !r.Read(race) || !r.Read(sex) || !r.Read(face) || !r.Read(hair) ||
        !r.Read(helmet_hide) || !r.Read(country) || !r.Read(aid_country) ||
        !r.Read(region) || !r.Read(channel) || !r.Read(map_id) ||
        !r.Read(pos_x) || !r.Read(pos_y) || !r.Read(pos_z) ||
        !r.Read(logout) || !r.Read(save) || !r.Read(result) ||
        !r.Read(title_id) || !r.Read(rank_point) || !r.Read(user_ip))
    {
        spdlog::warn("OnEnterSvrAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto self = ctx.chars->Find(char_id);
    if (!self) co_return;            // stale enter (legacy DELCHAR — deferred)
    {
        std::lock_guard g(self->lock);
        if (self->key != key) co_return;          // INVALIDCHAR — deferred
    }
    if (result != 0) co_return;      // enter error (legacy CONRESULT — deferred)

    // Index the name (Rename keeps FindByName coherent). On a map
    // change the name is already ours → idempotent; a true collision
    // with another char is logged but the rest of the load proceeds.
    if (!name.empty() && !ctx.chars->Rename(char_id, name))
        spdlog::warn("OnEnterSvrAck[{}]: char_id={} name '{}' index collision",
            ip, char_id, name);

    // Bulk-set the identity the incremental handlers later refine.
    {
        std::lock_guard g(self->lock);
        self->level       = level;
        self->real_sex    = real_sex;
        self->klass       = klass;
        self->race        = race;
        self->sex         = sex;
        self->face        = face;
        self->hair        = hair;
        self->helmet_hide = helmet_hide;
        self->country     = country;
        self->aid_country = aid_country;
        self->region      = region;
        self->channel     = channel;
        self->map_id      = map_id;
        self->pos_x       = pos_x;
        self->pos_y       = pos_y;
        self->pos_z       = pos_z;
        self->logout      = logout != 0;
    }

    // W6-16: main-session handoff completion. If chg_main_id is set
    // (and not a Bow/BR battleground), this ENTERSVR_ACK is the new
    // main confirming it loaded the handed-off char (the W6-15
    // RELEASEMAIN_ACK → ENTERSVR_REQ → here chain). Clear the flag and
    // ask the new main for the char's full server list (MAPSVRLIST_REQ
    // → re-enters the W6-13 reconcile). This is a map move, NOT a fresh
    // login, so skip the friend fan-out and return early.
    std::uint8_t chg = 0;
    { std::lock_guard g(self->lock); chg = self->chg_main_id; }
    if (chg != 0 && chg != kBowServerId && chg != kBrServerId)
    {
        { std::lock_guard g(self->lock); self->chg_main_id = 0; }
        co_await senders::SendMwMapSvrListReq(peer, char_id, key,
            channel, map_id, pos_x, pos_y, pos_z);
        spdlog::info("OnEnterSvrAck[{}]: char_id={} handoff complete (old "
                     "main={}) — MAPSVRLIST to new main", ip, char_id, chg);
        co_return;
    }

    // Announce arrival to online friends (now that name/region exist).
    co_await NotifyFriendsOnLogin(ctx, self);

    spdlog::info("OnEnterSvrAck[{}]: char_id={} '{}' entered (lvl={} map={} "
                 "region={})", ip, char_id, name, level, map_id, region);
    co_return;
}

boost::asio::awaitable<void>
OnCloseCharAck(std::shared_ptr<PeerSession>  peer,
               std::vector<std::byte>        body,
               const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0;
    std::uint32_t key     = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnCloseCharAck[{}]: short body ({} bytes) — dropped",
            ip, body.size());
        co_return;
    }

    if (!ctx.chars)
    {
        spdlog::warn("OnCloseCharAck[{}]: char registry not wired — dropped",
            ip);
        co_return;
    }

    auto removed = ctx.chars->Remove(char_id);
    if (!removed)
    {
        spdlog::info("OnCloseCharAck[{}]: char_id={} not in registry "
                     "(stale close) — TODO W3a-3: SendMW_DELCHAR_REQ",
            ip, char_id);
        co_return;
    }

    std::uint32_t removed_user_id = 0;
    {
        std::lock_guard g(removed->lock);
        if (removed->key != key)
            spdlog::warn("OnCloseCharAck[{}]: key mismatch for char_id={} "
                         "(registry=0x{:08X} incoming=0x{:08X}) — removed anyway",
                ip, char_id, removed->key, key);
        removed_user_id = removed->user_id;
    }

    // W4-7: fan out the offline presence to this char's friends +
    // soulmate (legacy LeaveFriend / LeaveSoulmate). `removed` is
    // out of the registry but kept alive by the shared_ptr.
    co_await NotifyFriendsOnLogout(ctx, removed);
    co_await NotifySoulmateOnLogout(ctx, removed);
    // W4-12: drop the char from any open TMS conferences (LeaveTMS).
    co_await NotifyTmsOnLogout(ctx, removed);

    bool any_other = false;
    for (auto other_id : ctx.chars->SnapshotIds())
    {
        if (auto other = ctx.chars->Find(other_id))
        {
            std::lock_guard g(other->lock);
            if (other->user_id == removed_user_id)
            {
                any_other = true;
                break;
            }
        }
    }
    if (!any_other)
        ctx.chars->MarkUserInactive(removed_user_id);

    spdlog::info("OnCloseCharAck[{}]: char_id={} removed user_id={} "
                 "user_still_active={} (total={})",
        ip, char_id, removed_user_id, any_other, ctx.chars->Size());
    co_return;
}

boost::asio::awaitable<void>
OnRegionAck(std::shared_ptr<PeerSession>  peer,
            std::vector<std::byte>        body,
            const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars)
    {
        spdlog::warn("OnRegionAck[{}]: char registry not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, region = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(region))
    {
        spdlog::warn("OnRegionAck[{}]: short body ({} bytes)", ip, body.size());
        co_return;
    }

    auto c = ctx.chars->Find(char_id);
    if (!c) co_return;
    std::uint32_t soulmate_target = 0;
    struct FE { std::uint32_t id; std::uint8_t type; bool connected; };
    std::vector<FE> friends;
    bool ok = false;
    {
        std::lock_guard g(c->lock);
        if (c->key == key)
        {
            c->region = region;
            soulmate_target = c->soulmate.target;
            for (const auto& f : c->friends)
                friends.push_back({f.id, f.type, f.connected});
            ok = true;
        }
    }
    if (!ok) co_return;

    // Mirror the new region into the soulmate partner's view (and
    // mark it connected — a region update implies we're online).
    if (soulmate_target != 0)
        if (auto p = ctx.chars->Find(soulmate_target))
        {
            std::lock_guard g(p->lock);
            if (p->soulmate.target == char_id)
            { p->soulmate.connected = true; p->soulmate.region = region; }
        }

    // Mirror it into each connected real-friend's reverse entry.
    for (const auto& fe : friends)
    {
        if (fe.type == frnd::kTypeFriend || !fe.connected) continue;
        if (auto p = ctx.chars->Find(fe.id))
        {
            std::lock_guard g(p->lock);
            for (auto& pf : p->friends)
                if (pf.id == char_id) pf.region = region;
        }
    }
    co_return;
}

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
OnLevelUpAck(std::shared_ptr<PeerSession>  peer,
             std::vector<std::byte>        body,
             const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars)
    {
        spdlog::warn("OnLevelUpAck[{}]: char registry not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  level = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(level))
    {
        spdlog::warn("OnLevelUpAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto c = ctx.chars->Find(char_id);
    if (!c) co_return;
    std::uint8_t  main_msi = 0;
    std::vector<std::uint8_t> other_msis;
    std::uint32_t sm_target = 0;
    std::uint8_t  sm_known_level = 0;
    bool ok = false;
    {
        std::lock_guard g(c->lock);
        if (c->key == key)
        {
            c->level = level;
            main_msi = c->main_server_id;
            for (const auto& con : c->cons)
                if (con.valid && con.server_id != main_msi)
                    other_msis.push_back(con.server_id);
            sm_target = c->soulmate.target;
            sm_known_level = c->soulmate.level;
            ok = true;
        }
    }
    if (!ok) co_return;

    // Fan the new level to the char's other map connections.
    for (auto msi : other_msis)
        if (auto p = FindMapPeer(ctx, msi))
            co_await senders::SendMwLevelUpReq(p, char_id, key, level);

    if (sm_target != 0)
    {
        // Keep the partner's view of our level current.
        if (auto p = ctx.chars->Find(sm_target))
        {
            std::lock_guard g(p->lock);
            if (p->soulmate.target == char_id) p->soulmate.level = level;
        }
        // Auto-dissolve if the level gap now exceeds the window
        // (legacy CheckSoulmateEnd).
        if (std::abs(int(level) - int(sm_known_level)) >
            soulmate::kLevelWindow)
        {
            { std::lock_guard g(c->lock);
              if (c->soulmate.target == sm_target) c->soulmate = TSoulmate{}; }
            if (auto p = ctx.chars->Find(sm_target))
            { std::lock_guard g(p->lock);
              if (p->soulmate.target == char_id) p->soulmate = TSoulmate{}; }
            if (ctx.friend_repo)
                co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                    [repo = ctx.friend_repo, char_id, sm_target]
                    { repo->DelSoulmate(char_id, sm_target);
                      repo->DelSoulmate(sm_target, char_id); });
            spdlog::info("OnLevelUpAck[{}]: char_id={} level {} dissolved "
                         "soulmate {} (gap > {})", ip, char_id, level,
                sm_target, soulmate::kLevelWindow);
        }
    }
    co_return;
}

boost::asio::awaitable<void>
OnCharStatInfoAck(std::shared_ptr<PeerSession>  peer,
                  std::vector<std::byte>        body,
                  const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers) co_return;

    wire::Reader r(body.data(), body.size());
    std::uint32_t req_char_id = 0, char_id = 0;
    if (!r.Read(req_char_id) || !r.Read(char_id))
    {
        spdlog::warn("OnCharStatInfoAck[{}]: short body", ip);
        co_return;
    }

    auto target = ctx.chars->Find(char_id);
    if (!target) co_return;
    std::uint8_t msi = 0;
    { std::lock_guard g(target->lock); msi = target->main_server_id; }
    if (auto p = FindMapPeer(ctx, msi))
        co_await senders::SendMwCharStatInfoAnsReq(p, req_char_id, char_id);
    co_return;
}

boost::asio::awaitable<void>
OnCharStatInfoAnsAck(std::shared_ptr<PeerSession>  peer,
                     std::vector<std::byte>        body,
                     const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers) co_return;

    wire::Reader r(body.data(), body.size());
    std::uint32_t req_char_id = 0;
    if (!r.Read(req_char_id))
    {
        spdlog::warn("OnCharStatInfoAnsAck[{}]: short body", ip);
        co_return;
    }

    auto requester = ctx.chars->Find(req_char_id);
    if (!requester) co_return;
    std::uint8_t msi = 0;
    { std::lock_guard g(requester->lock); msi = requester->main_server_id; }
    // Relay the gathered stat block back verbatim.
    if (auto p = FindMapPeer(ctx, msi))
        co_await senders::SendMwCharStatInfoReq(p, body);
    co_return;
}

boost::asio::awaitable<void>
OnPetRidingAck(std::shared_ptr<PeerSession>  peer,
               std::vector<std::byte>        body,
               const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnPetRidingAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, riding = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(riding))
    {
        spdlog::warn("OnPetRidingAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto c = ctx.chars->Find(char_id);
    if (!c) co_return;
    const std::uint8_t origin_msi =
        static_cast<std::uint8_t>(peer->Wid() & 0xFF);
    std::vector<std::uint8_t> other_msis;
    bool ok = false;
    {
        std::lock_guard g(c->lock);
        if (c->key == key)
        {
            c->riding = riding;
            // Fan to every other connection (legacy excludes the
            // originating server, which already applied it locally).
            for (const auto& con : c->cons)
                if (con.valid && con.server_id != origin_msi)
                    other_msis.push_back(con.server_id);
            ok = true;
        }
    }
    if (!ok) co_return;

    for (auto msi : other_msis)
        if (auto p = FindMapPeer(ctx, msi))
            co_await senders::SendMwPetRidingReq(p, char_id, key, riding);
    co_return;
}

boost::asio::awaitable<void>
OnHelmetHideAck(std::shared_ptr<PeerSession>  peer,
                std::vector<std::byte>        body,
                const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars)
    {
        spdlog::warn("OnHelmetHideAck[{}]: char registry not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  hide = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(hide))
    {
        spdlog::warn("OnHelmetHideAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto c = ctx.chars->Find(char_id);
    if (!c) co_return;
    bool ok = false;
    {
        std::lock_guard g(c->lock);
        if (c->key == key) { c->helmet_hide = hide; ok = true; }
    }
    if (!ok) co_return;

    // Confirm back to the originating map (legacy echoes to pSERVER).
    co_await senders::SendMwHelmetHideReq(peer, char_id, key, hide);
    co_return;
}

} // namespace tworldsvr::handlers
