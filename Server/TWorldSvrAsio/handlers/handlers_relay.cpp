#include "handlers.h"
#include "../senders/senders.h"
#include "../wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <cstdint>

namespace tworldsvr::handlers {

boost::asio::awaitable<void>
OnRelaysvrReq(std::shared_ptr<PeerSession>  peer,
              std::vector<std::byte>        body,
              const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();

    // Body: { WORD wID }.
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

    // Set the nation flag regardless of whether registration
    // succeeds — it's cheap and the peer needs it for the ACK.
    peer->SetNation(ctx.nation);

    // Try to register BEFORE tagging the PeerSession with the
    // wID. If the registry already holds wID=`wid` (a duplicate
    // connect from the same map server), the second session must
    // NOT carry that wID — otherwise its disconnect cleanup
    // path (WorldServer::HandleConnection) would Unregister the
    // original peer.
    //
    // We mint a temporary PeerSession alias with the candidate
    // wID so Register can do the dedup check, but only commit
    // the wID onto the real peer if Register succeeds.
    peer->SetWid(wid);
    if (!ctx.peers->Register(peer))
    {
        // Roll back: scrub the wID so the disconnect path skips
        // Unregister and leaves the original entry intact.
        peer->SetWid(0);
        spdlog::warn("OnRelaysvrReq[{}]: wID={} already registered — "
                     "ignoring duplicate connect (this session stays "
                     "anonymous)", ip, wid);
        co_return;
    }

    spdlog::info("OnRelaysvrReq[{}]: wID={} registered as map peer "
                 "(nation={}, registry_size={})",
        ip, wid, ctx.nation, ctx.peers->Size());

    // Reply with the cluster's bootstrap info. Operator + svr_msg
    // lists are empty in W3a-2 because the operator system isn't
    // ported yet (Server/TControlSvrAsio handles operator auth
    // separately) and the server-message table is W5+ scope. The
    // map server treats both as optional bootstrap data; an empty
    // table is the same as "no operators connected yet".
    co_await senders::SendRwRelaysvrAck(peer, ctx.nation,
        /*operators=*/{},
        /*svr_msgs=*/{});

    // TODO W3a-3: broadcast SendMW_RELAYCONNECT_REQ(0) to every
    // *other* map peer so they all learn the relay is live (legacy
    // RWHandler.cpp:13-15).
    co_return;
}

} // namespace tworldsvr::handlers
