#include "handlers.h"
#include "../senders/senders.h"
#include "../services/soulmate_constants.h"
#include "../wire_codec.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

namespace tworldsvr::handlers {

namespace {

// One char's describe-fields needed to fill the partner's TSoulmate.
struct Mate
{
    std::uint32_t id = 0, region = 0;
    std::uint8_t  level = 0, klass = 0;
    std::string   name;
};

Mate SnapshotMate(TChar& c)   // caller holds c.lock
{
    return Mate{c.char_id, c.region, c.level, c.klass, c.name};
}

// Set `self`'s soulmate to `m` (caller holds self.lock).
void SetSoulmate(TChar& self, const Mate& m)
{
    self.soulmate = TSoulmate{m.id, m.name, m.level, m.klass, true, m.region};
}

} // namespace

boost::asio::awaitable<void>
NotifySoulmateOnLogout(const HandlerContext& ctx, std::shared_ptr<TChar> who)
{
    if (!ctx.chars || !who) co_return;
    std::uint32_t who_id = 0, partner = 0;
    bool          connected = false;
    {
        std::lock_guard g(who->lock);
        who_id    = who->char_id;
        partner   = who->soulmate.target;
        connected = who->soulmate.connected;
    }
    if (partner != 0 && connected)
        if (auto p = ctx.chars->Find(partner))
        {
            std::lock_guard g(p->lock);
            if (p->soulmate.target == who_id)
            {
                p->soulmate.connected = false;
                p->soulmate.region = 0;
            }
        }
    co_return;   // legacy LeaveSoulmate sends no packet
}

boost::asio::awaitable<void>
OnSoulmateSearchAck(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte>       body,
                    const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars) { spdlog::warn("OnSoulmateSearchAck[{}]: no chars", ip);
        co_return; }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  min_level = 0, npc_inven = 0, npc_item = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(min_level) ||
        !r.Read(npc_inven) || !r.Read(npc_item))
    {
        spdlog::warn("OnSoulmateSearchAck[{}]: short body", ip);
        co_return;
    }

    auto searcher = ctx.chars->Find(char_id);
    if (!searcher) co_return;
    std::uint8_t  s_country = 0, s_level = 0, s_realsex = 0, s_sex = 0;
    Mate          self_mate;
    {
        std::lock_guard g(searcher->lock);
        if (searcher->key != key) co_return;
        s_country = searcher->country; s_level = searcher->level;
        s_realsex = searcher->real_sex; s_sex = searcher->sex;
        self_mate = SnapshotMate(*searcher);
    }

    struct Cand { Mate mate; std::uint8_t realsex = 0, sex = 0;
                  bool has_sm = false; };
    std::vector<Cand> cands;
    std::uint8_t best = min_level;
    for (auto id : ctx.chars->SnapshotIds())
    {
        if (id == char_id) continue;
        auto c = ctx.chars->Find(id);
        if (!c) continue;
        Cand cand;
        std::uint8_t c_country = 0, c_level = 0;
        {
            std::lock_guard g(c->lock);
            c_country = c->country; c_level = c->level;
            cand.realsex = c->real_sex; cand.sex = c->sex;
            cand.has_sm = (c->soulmate.target != 0);
            cand.mate = SnapshotMate(*c);
        }
        if (c_country != s_country) continue;
        if (std::abs(int(s_level) - int(c_level)) >= soulmate::kLevelWindow)
            continue;
        if (best > c_level) { cands.clear(); cands.push_back(cand);
                              best = c_level; }
        else if (best == c_level) cands.push_back(cand);
    }

    if (cands.empty())
    {
        co_await senders::SendMwSoulmateSearchReq(peer, char_id, key,
            soulmate::kNotFound, 0, std::string{}, 0, npc_inven, npc_item);
        co_return;
    }

    auto filter = [&](auto pred) {
        if (cands.size() <= 1) return;
        std::vector<Cand> t;
        for (const auto& c : cands) if (pred(c)) t.push_back(c);
        if (!t.empty()) cands = std::move(t);
    };
    filter([&](const Cand& c) { return c.realsex != s_realsex; });
    filter([&](const Cand& c) { return !c.has_sm; });
    filter([&](const Cand& c) { return c.sex != s_sex; });

    const Mate match = cands.front().mate;

    // Register the mutual pairing in-memory (DB persistence deferred).
    { std::lock_guard g(searcher->lock); SetSoulmate(*searcher, match); }
    if (auto m = ctx.chars->Find(match.id))
    { std::lock_guard g(m->lock); SetSoulmate(*m, self_mate); }

    co_await senders::SendMwSoulmateSearchReq(peer, char_id, key,
        soulmate::kSuccess, match.id, match.name, match.region, npc_inven,
        npc_item);
    spdlog::info("OnSoulmateSearchAck[{}]: char_id={} matched {} ('{}')",
        ip, char_id, match.id, match.name);
    co_return;
}

boost::asio::awaitable<void>
OnSoulmateRegAck(std::shared_ptr<PeerSession> peer,
                 std::vector<std::byte>       body,
                 const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars) { spdlog::warn("OnSoulmateRegAck[{}]: no chars", ip);
        co_return; }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::string   name;
    std::uint8_t  reg = 0, npc_inven = 0, npc_item = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.ReadString(name) ||
        !r.Read(reg) || !r.Read(npc_inven) || !r.Read(npc_item))
    {
        spdlog::warn("OnSoulmateRegAck[{}]: short body", ip);
        co_return;
    }

    auto self = ctx.chars->Find(char_id);
    if (!self) co_return;
    std::uint8_t s_country = 0, s_level = 0;
    Mate         self_mate;
    {
        std::lock_guard g(self->lock);
        if (self->key != key) co_return;
        s_country = self->country; s_level = self->level;
        self_mate = SnapshotMate(*self);
    }

    auto reply = [&](std::uint8_t result, std::uint32_t sm,
                     const std::string& sname, std::uint32_t region)
        -> boost::asio::awaitable<void> {
        co_await senders::SendMwSoulmateRegReq(peer, char_id, key, result, reg,
            npc_inven, npc_item, sm, sname, region);
    };

    auto target = ctx.chars->FindByName(name);
    if (!target)
    {
        co_await reply(soulmate::kNotFound, 0, std::string{}, 0);
        co_return;
    }
    std::uint8_t t_country = 0, t_level = 0;
    Mate         t_mate;
    {
        std::lock_guard g(target->lock);
        t_country = target->country; t_level = target->level;
        t_mate = SnapshotMate(*target);
    }
    if (t_country != s_country ||
        std::abs(int(t_level) - int(s_level)) > soulmate::kLevelWindow)
    {
        co_await reply(soulmate::kFail, 0, std::string{}, 0);
        co_return;
    }

    if (reg)
    {
        // Commit the mutual pairing (DB persistence deferred).
        { std::lock_guard g(self->lock); SetSoulmate(*self, t_mate); }
        if (auto t2 = ctx.chars->Find(t_mate.id))
        { std::lock_guard g(t2->lock); SetSoulmate(*t2, self_mate); }
    }
    co_await reply(soulmate::kSuccess, t_mate.id, t_mate.name, t_mate.region);
    co_return;
}

boost::asio::awaitable<void>
OnSoulmateEndAck(std::shared_ptr<PeerSession> peer,
                 std::vector<std::byte>       body,
                 const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars) { spdlog::warn("OnSoulmateEndAck[{}]: no chars", ip);
        co_return; }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnSoulmateEndAck[{}]: short body", ip);
        co_return;
    }

    auto self = ctx.chars->Find(char_id);
    if (!self) co_return;
    std::uint32_t partner = 0;
    {
        std::lock_guard g(self->lock);
        if (self->key != key) co_return;
        partner = self->soulmate.target;
    }
    if (partner == 0)
    {
        co_await senders::SendMwSoulmateEndReq(peer, char_id, key,
            soulmate::kFail, 0);
        co_return;
    }

    // Clear both sides (DB persistence deferred).
    { std::lock_guard g(self->lock); self->soulmate = TSoulmate{}; }
    if (auto p = ctx.chars->Find(partner))
    {
        std::lock_guard g(p->lock);
        if (p->soulmate.target == char_id) p->soulmate = TSoulmate{};
    }
    co_await senders::SendMwSoulmateEndReq(peer, char_id, key,
        soulmate::kSuccess,
        static_cast<std::uint32_t>(std::time(nullptr)));
    spdlog::info("OnSoulmateEndAck[{}]: char_id={} dissolved pairing with {}",
        ip, char_id, partner);
    co_return;
}

} // namespace tworldsvr::handlers
