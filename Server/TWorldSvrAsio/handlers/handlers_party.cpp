#include "handlers.h"
#include "../senders/senders.h"
#include "../services/party_constants.h"
#include "../wire_codec.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <mutex>
#include <string>

namespace tworldsvr::handlers {

namespace {

// Find the map peer serving a given main_server_id (msi), or
// nullptr if that map is offline. Mirrors legacy FindMapSvr —
// same helper the guild invite/answer relay paths use.
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
OnPartyAddAck(std::shared_ptr<PeerSession> peer,
              std::vector<std::byte>       body,
              const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnPartyAddAck[{}]: registries not wired — dropped", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::string   request_name, target_name;
    std::uint8_t  obtain_type = 0;
    std::uint32_t max_hp = 0, hp = 0, max_mp = 0, mp = 0;
    if (!r.ReadString(request_name) || !r.ReadString(target_name) ||
        !r.Read(obtain_type) ||
        !r.Read(max_hp) || !r.Read(hp) || !r.Read(max_mp) || !r.Read(mp))
    {
        spdlog::warn("OnPartyAddAck[{}]: short body ({} bytes) — dropped",
            ip, body.size());
        co_return;
    }

    auto requester = ctx.chars->FindByName(request_name);
    if (!requester)
    {
        // Legacy: !pRequest → silent drop (the inviter logged off
        // between the client request and this packet).
        spdlog::info("OnPartyAddAck[{}]: requester='{}' offline — drop",
            ip, request_name);
        co_return;
    }
    auto target = ctx.chars->FindByName(target_name);

    // Inviting yourself (same TChar) is a no-op drop (legacy
    // pRequest == pTarget short-circuit).
    if (target && target.get() == requester.get())
        co_return;

    std::uint32_t req_char_id = 0, req_key = 0, req_party_id = 0;
    std::uint8_t  req_country = 0, req_aid = 0;
    {
        std::lock_guard g(requester->lock);
        req_char_id  = requester->char_id;
        req_key      = requester->key;
        req_party_id = requester->party_id;
        req_country  = requester->country;
        req_aid      = requester->aid_country;
    }

    // All failure replies go back to the requester's map — the
    // originating peer (legacy FindMapSvr(pRequest->m_bMainID)).
    auto reply_requester =
        [&](std::uint8_t result) -> boost::asio::awaitable<void> {
        co_await senders::SendMwPartyAddReq(peer, req_char_id, req_key,
            request_name, target_name, obtain_type, result, /*request=*/0);
    };

    if (!target)
    {
        co_await reply_requester(party::kNoUser);
        co_return;
    }

    std::uint32_t tgt_char_id = 0, tgt_key = 0, tgt_party_id = 0;
    std::uint8_t  tgt_country = 0, tgt_aid = 0, tgt_msi = 0;
    bool          tgt_waiter  = false;
    {
        std::lock_guard g(target->lock);
        tgt_char_id  = target->char_id;
        tgt_key      = target->key;
        tgt_party_id = target->party_id;
        tgt_country  = target->country;
        tgt_aid      = target->aid_country;
        tgt_waiter   = target->party_waiter;
        tgt_msi      = target->main_server_id;
    }

    if (tgt_waiter)
    {
        co_await reply_requester(party::kWaiters);
        co_return;
    }
    if (tgt_party_id != 0)
    {
        co_await reply_requester(party::kAlready);
        co_return;
    }
    if (party::WarCountry(tgt_country, tgt_aid) !=
        party::WarCountry(req_country, req_aid))
    {
        co_await reply_requester(party::kCountry);
        co_return;
    }

    // Requester-already-in-a-party gate: only the chief of a
    // non-full, non-arena party may add a member.
    if (req_party_id != 0 && ctx.parties)
    {
        if (auto pty = ctx.parties->Find(req_party_id))
        {
            bool         is_chief = false, arena = false;
            std::uint8_t size     = 0;
            {
                std::lock_guard pg(pty->lock);
                is_chief = pty->IsChief(req_char_id);
                size     = pty->Size();
                arena    = pty->arena;
            }
            if (!is_chief)
            {
                co_await reply_requester(party::kNotChief);
                co_return;
            }
            if (size >= party::kMaxPartyMember)
            {
                co_await reply_requester(party::kFull);
                co_return;
            }
            if (arena)
            {
                // Legacy: silent drop — can't grow an arena party.
                spdlog::info("OnPartyAddAck[{}]: requester char_id={} in "
                             "arena party {} — drop", ip, req_char_id,
                    req_party_id);
                co_return;
            }
        }
        else
        {
            spdlog::warn("OnPartyAddAck[{}]: requester char_id={} has stale "
                         "party_id={} — treating as partyless",
                ip, req_char_id, req_party_id);
        }
    }

    // SetCharStatus(pRequest): stash the inviter's current combat
    // stats so the W3b-2 JOIN broadcast can ship them.
    {
        std::lock_guard g(requester->lock);
        requester->max_hp = max_hp;
        requester->hp     = hp;
        requester->max_mp = max_mp;
        requester->mp     = mp;
    }

    // Forward the AGREE dialog to the target's map peer.
    auto target_peer = FindMapPeer(ctx, tgt_msi);
    if (!target_peer)
    {
        spdlog::info("OnPartyAddAck[{}]: target='{}' main_server_id={} "
                     "offline — invite dropped", ip, target_name, tgt_msi);
        co_return;
    }
    co_await senders::SendMwPartyAddReq(target_peer, tgt_char_id, tgt_key,
        request_name, target_name, obtain_type, party::kAgree, req_char_id);
    {
        std::lock_guard g(target->lock);
        target->party_waiter = true;
    }
    spdlog::info("OnPartyAddAck[{}]: char_id={} invited '{}' (msi={}) "
                 "obtain={} — AGREE relayed", ip, req_char_id, target_name,
        tgt_msi, obtain_type);
    co_return;
}

} // namespace tworldsvr::handlers
