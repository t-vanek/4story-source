#include "handlers.h"
#include "../senders/senders.h"
#include "../services/corps_constants.h"
#include "../services/party_constants.h"
#include "../wire_codec.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <mutex>
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

// Legacy CheckCorpsJoin (TWorldSvr.cpp:4577): reject if both
// parties already belong to a corps; if either's corps is at the
// MAX_CORPS_PARTY cap, reject with kMaxParty. Returns kSuccess (0)
// when the merge is allowed.
std::uint8_t CheckCorpsJoin(const HandlerContext& ctx,
                            std::uint16_t origin_corps,
                            std::uint16_t target_corps)
{
    if (origin_corps != 0 && target_corps != 0)
        return corps::kWrongTarget;

    for (std::uint16_t cid : {origin_corps, target_corps})
    {
        if (cid == 0) continue;
        auto c = ctx.corps ? ctx.corps->Find(cid) : nullptr;
        if (!c) return corps::kWrongTarget;
        std::uint8_t size = 0;
        { std::lock_guard g(c->lock); size = c->Size(); }
        if (size >= corps::kMaxCorpsParty) return corps::kMaxParty;
    }
    return corps::kSuccess;
}

} // namespace

boost::asio::awaitable<void>
OnCorpsAskAck(std::shared_ptr<PeerSession> peer,
              std::vector<std::byte>       body,
              const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.parties)
    {
        spdlog::warn("OnCorpsAskAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::string   target_name;
    if (!r.Read(char_id) || !r.Read(key) || !r.ReadString(target_name))
    {
        spdlog::warn("OnCorpsAskAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto requester = ctx.chars->Find(char_id);
    if (!requester) co_return;
    std::uint32_t actual_key = 0, req_party = 0;
    std::uint8_t  req_country = 0, req_aid = 0;
    std::string   req_name;
    {
        std::lock_guard g(requester->lock);
        actual_key  = requester->key;
        req_party   = requester->party_id;
        req_country = requester->country;
        req_aid     = requester->aid_country;
        req_name    = requester->name;
    }
    if (actual_key != key) co_return;

    auto reply_self = [&](std::uint8_t result)
        -> boost::asio::awaitable<void> {
        co_await senders::SendMwCorpsReplyReq(peer, char_id, key, result,
            target_name);
    };

    auto target = ctx.chars->FindByName(target_name);
    if (!target)
    {
        co_await reply_self(corps::kWrongTarget);
        co_return;
    }
    std::uint32_t t_party = 0, t_charid = 0, t_key = 0;
    std::uint8_t  t_country = 0, t_aid = 0, t_msi = 0;
    {
        std::lock_guard g(target->lock);
        t_party   = target->party_id;
        t_charid  = target->char_id;
        t_key     = target->key;
        t_country = target->country;
        t_aid     = target->aid_country;
        t_msi     = target->main_server_id;
    }

    // Both must be the chief of a (non-arena) party, same war-country.
    bool          req_is_chief = false, req_arena = false;
    std::uint16_t req_corps = 0;
    if (req_party != 0)
        if (auto p = ctx.parties->Find(static_cast<std::uint16_t>(req_party)))
        {
            std::lock_guard pg(p->lock);
            req_is_chief = p->IsChief(char_id);
            req_arena    = p->arena;
            req_corps    = p->corps_id;
        }
    bool          t_is_chief = false, t_arena = false;
    std::uint16_t t_corps = 0;
    if (t_party != 0)
        if (auto p = ctx.parties->Find(static_cast<std::uint16_t>(t_party)))
        {
            std::lock_guard pg(p->lock);
            t_is_chief = p->IsChief(t_charid);
            t_arena    = p->arena;
            t_corps    = p->corps_id;
        }

    if (req_party == 0 || !req_is_chief || t_party == 0 || !t_is_chief ||
        party::WarCountry(req_country, req_aid) !=
            party::WarCountry(t_country, t_aid))
    {
        co_await reply_self(corps::kNoParty);
        co_return;
    }
    if (req_arena || t_arena)
    {
        co_await reply_self(corps::kBusy);
        co_return;
    }

    const std::uint8_t gate = CheckCorpsJoin(ctx, req_corps, t_corps);
    if (gate != corps::kSuccess)
    {
        co_await reply_self(gate);
        co_return;
    }

    // Forward the invite to the target chief's map.
    auto tp = FindMapPeer(ctx, t_msi);
    if (!tp)
    {
        spdlog::info("OnCorpsAskAck[{}]: target '{}' map {} offline — drop",
            ip, target_name, t_msi);
        co_return;
    }
    co_await senders::SendMwCorpsAskReq(tp, t_charid, t_key, char_id, req_name);
    spdlog::info("OnCorpsAskAck[{}]: char_id={} invited '{}' to corps",
        ip, char_id, target_name);
    co_return;
}

} // namespace tworldsvr::handlers
