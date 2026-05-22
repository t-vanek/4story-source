#include "handlers.h"
#include "../senders/senders.h"
#include "../wire_codec.h"

#include "MessageId.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <spdlog/spdlog.h>

#include <cstdint>
#include <mutex>

namespace tworldsvr::handlers {

boost::asio::awaitable<void>
OnRelaysvrReq(std::shared_ptr<PeerSession>  peer,
              std::vector<std::byte>        body,
              const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();

    wire::Reader r(body.data(), body.size());
    std::uint16_t wid = 0;
    if (!r.Read(wid))
    {
        spdlog::warn("OnRelaysvrReq[{}]: short body ({} bytes) — dropped",
            ip, body.size());
        co_return;
    }
    if (wid == 0)
    {
        spdlog::warn("OnRelaysvrReq[{}]: wID=0 reserved as 'not registered' "
                     "— dropped", ip);
        co_return;
    }

    if (!ctx.peers)
    {
        spdlog::warn("OnRelaysvrReq[{}]: peer registry not wired — dropped",
            ip);
        co_return;
    }

    peer->SetNation(ctx.nation);
    peer->SetWid(wid);
    if (!ctx.peers->Register(peer))
    {
        peer->SetWid(0);
        spdlog::warn("OnRelaysvrReq[{}]: wID={} already registered — "
                     "ignoring duplicate connect (this session stays "
                     "anonymous)", ip, wid);
        co_return;
    }

    spdlog::info("OnRelaysvrReq[{}]: wID={} registered as map peer "
                 "(nation={}, registry_size={})",
        ip, wid, ctx.nation, ctx.peers->Size());

    co_await senders::SendRwRelaysvrAck(peer, ctx.nation,
        /*operators=*/{},
        /*svr_msgs=*/{});

    // W3a-3: fan-out RELAYCONNECT to every other registered peer
    // so they know a relay is live. Legacy parity (RWHandler.cpp:
    // 13-15): `(*it).second->SendMW_RELAYCONNECT_REQ(0)`. We pass
    // bRelayOn=0 to match — this is the "the relay just came up"
    // notification, not the per-char "open a relay channel" one
    // (that's bRelayOn=1, see OnRelayConnectReq).
    //
    // Each broadcast is spawned as an independent coroutine on
    // the io_context so a slow socket doesn't stall the OnRelaysvr
    // handler. spdlog logs the fan-out count for ops visibility.
    auto others = ctx.peers->SnapshotExcept(wid);
    if (!others.empty() && ctx.io != nullptr)
    {
        for (auto& other : others)
        {
            boost::asio::co_spawn(*ctx.io,
                senders::SendMwRelayconnectReq(other, /*char_id=*/0,
                    /*relay_on=*/0),
                boost::asio::detached);
        }
        spdlog::info("OnRelaysvrReq[{}]: broadcast RELAYCONNECT to {} "
                     "other peer(s)", ip, others.size());
    }
    co_return;
}

boost::asio::awaitable<void>
OnEnterCharReq(std::shared_ptr<PeerSession>  peer,
               std::vector<std::byte>        body,
               const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();

    if (!ctx.chars)
    {
        spdlog::warn("OnEnterCharReq[{}]: char registry not wired — dropped",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0;
    std::string   strName;
    if (!r.Read(char_id) || !r.ReadString(strName))
    {
        spdlog::warn("OnEnterCharReq[{}]: short body ({} bytes) — dropped",
            ip, body.size());
        co_return;
    }

    // Legacy gate (RWHandler.cpp:36-43): lookup by name, then
    // verify the dwCharID matches. Either miss → reply bResult=0
    // so the relay map closes the entry attempt down. We use the
    // sharded name index introduced in W3a-3.
    auto tchar = ctx.chars->FindByName(strName);
    bool ok = false;
    std::uint8_t  reply_country     = 0;
    std::uint8_t  reply_aid_country = 0;
    std::uint16_t reply_map_id      = 0;
    std::uint32_t reply_guild_id    = 0;
    std::string   reply_name        = strName;
    if (tchar)
    {
        std::lock_guard g(tchar->lock);
        if (tchar->char_id == char_id)
        {
            ok                = true;
            reply_country     = tchar->country;
            reply_aid_country = tchar->aid_country;
            reply_map_id      = tchar->map_id;
            reply_name        = tchar->name;
            reply_guild_id    = tchar->guild_id;
        }
    }
    if (!ok)
    {
        spdlog::info("OnEnterCharReq[{}]: char_id={} name='{}' miss "
                     "(replying bResult=0)", ip, char_id, strName);
        co_await senders::SendRwEntercharAck(
            peer, char_id, reply_name, /*result=*/0,
            /*country=*/0, /*aid_country=*/0,
            /*guild_id=*/0, /*guild_chief=*/0, /*duty=*/0,
            /*party_id=*/0, /*party_chief_id=*/0,
            /*corps_id=*/0, /*general_id=*/0,
            /*tactics_id=*/0, /*tactics_chief=*/0,
            /*map_id=*/0, /*unit_id=*/0);
        co_return;
    }

    // W3a-4: resolve guild fields from the GuildRegistry. Legacy
    // pulls these from pChar->m_pGuild + pChar->m_pGuild->FindMember.
    // The chief id is on the guild itself; the requesting char's
    // duty comes from their member row. Disorganised guilds return
    // zeros (legacy parity — relay treats disorg as "no guild").
    std::uint32_t reply_guild_chief = 0;
    std::uint8_t  reply_duty        = 0;
    if (reply_guild_id != 0 && ctx.guilds)
    {
        if (auto g = ctx.guilds->Find(reply_guild_id))
        {
            std::lock_guard gl(g->lock);
            if (!g->disorg)
            {
                reply_guild_chief = g->chief_char_id;
                if (const auto* member = g->FindMember(char_id))
                    reply_duty = member->duty;
            }
            else
            {
                reply_guild_id = 0;
            }
        }
        else
        {
            // Stale guild_id on the char — the guild was unloaded.
            // Treat as "no guild" but log so ops can see it.
            spdlog::warn("OnEnterCharReq[{}]: char_id={} carried "
                         "stale guild_id={} (guild not in registry)",
                ip, char_id, reply_guild_id);
            reply_guild_id = 0;
        }
    }

    // Party / corps / tactics state is still zero-default — those
    // registries don't exist yet (W3b party / corps, W3a-4b+
    // tactics).
    spdlog::info("OnEnterCharReq[{}]: char_id={} name='{}' country={} "
                 "guild_id={} guild_chief={} duty={} map={} — "
                 "replying bResult=1",
        ip, char_id, reply_name, reply_country, reply_guild_id,
        reply_guild_chief, reply_duty, reply_map_id);
    co_await senders::SendRwEntercharAck(
        peer, char_id, reply_name, /*result=*/1,
        reply_country, reply_aid_country,
        reply_guild_id, reply_guild_chief, reply_duty,
        /*party_id=*/0, /*party_chief_id=*/0,
        /*corps_id=*/0, /*general_id=*/0,
        /*tactics_id=*/0, /*tactics_chief=*/0,
        reply_map_id, /*unit_id=*/0);

    // TODO W4 (chat): CheckChatBan(pChar, TRUE) — pending chat-ban
    // registry. The legacy module re-sends an unmute / mute notice
    // here so the rejoined client picks up its current ban state.
    co_return;
}

boost::asio::awaitable<void>
OnRelayConnectReq(std::shared_ptr<PeerSession>  peer,
                  std::vector<std::byte>        body,
                  const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();

    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnRelayConnectReq[{}]: registries not wired — dropped",
            ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0;
    if (!r.Read(char_id))
    {
        spdlog::warn("OnRelayConnectReq[{}]: short body ({} bytes) — dropped",
            ip, body.size());
        co_return;
    }

    auto tchar = ctx.chars->Find(char_id);
    if (!tchar)
    {
        // Legacy parity (RWHandler.cpp:121): silent drop — the
        // relay's lookup table is allowed to be stale; the client
        // will retry through the normal connect flow.
        spdlog::debug("OnRelayConnectReq[{}]: char_id={} not in registry",
            ip, char_id);
        co_return;
    }

    std::uint8_t main_server_id = 0;
    {
        std::lock_guard g(tchar->lock);
        main_server_id = tchar->main_server_id;
    }
    if (main_server_id == 0)
    {
        spdlog::warn("OnRelayConnectReq[{}]: char_id={} main_server_id=0 "
                     "(char registered before its peer set wID) — dropped",
            ip, char_id);
        co_return;
    }

    // Look up the main map's peer by wID. The legacy code uses
    // FindMapSvr(bMainID) which is a singleton-by-byte; PeerRegistry
    // is keyed by the full 16-bit wID. The convention so far
    // (matches OnAddCharAck) is bMainID = LOBYTE(wID) — so we
    // scan the registry for any peer whose LOBYTE matches.
    //
    // W3a-4 may switch to a denormalised by-byte index if profiling
    // shows this scan is hot; with O(10..50) registered peers it's
    // cheap enough today.
    std::shared_ptr<PeerSession> main_peer;
    for (auto& p : ctx.peers->Snapshot())
    {
        if (static_cast<std::uint8_t>(p->Wid() & 0xFF) == main_server_id)
        {
            main_peer = std::move(p);
            break;
        }
    }
    if (!main_peer)
    {
        spdlog::info("OnRelayConnectReq[{}]: char_id={} main_server_id={} "
                     "not in peer registry (map server offline?) — dropped",
            ip, char_id, main_server_id);
        co_return;
    }

    co_await senders::SendMwRelayconnectReq(main_peer, char_id,
        /*relay_on=*/1);
    spdlog::info("OnRelayConnectReq[{}]: char_id={} routed to "
                 "main_server_id={} (wID={})",
        ip, char_id, main_server_id, main_peer->Wid());
}

} // namespace tworldsvr::handlers
