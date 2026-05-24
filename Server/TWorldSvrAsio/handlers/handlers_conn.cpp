#include "handlers.h"
#include "../senders/senders.h"
#include "../wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <vector>

namespace tworldsvr::handlers {

namespace {

// TCONNECT_RESULT::CN_SUCCESS (NetCode.h:321).
constexpr std::uint8_t kConSuccess = 0;

std::shared_ptr<PeerSession>
FindMapPeer(const HandlerContext& ctx, std::uint8_t msi)
{
    if (msi == 0 || !ctx.peers) return nullptr;
    for (auto& p : ctx.peers->Snapshot())
        if (static_cast<std::uint8_t>(p->Wid() & 0xFF) == msi)
            return p;
    return nullptr;
}

// --- cession queue (legacy PushConCess / PopConCess) ---------------
// Forward-declared because PopConCess + BeginTeleport are mutually
// recursive (a non-main teleport pops + replays the next entry) and
// OnCheckMainAck drives PopConCess when a main session is confirmed.
boost::asio::awaitable<void>
BeginTeleport(std::shared_ptr<PeerSession> peer,
              std::vector<std::byte>       body,
              const HandlerContext&        ctx);
boost::asio::awaitable<void>
PopConCess(std::shared_ptr<TChar> ch, const HandlerContext& ctx);

// Push a deferred handoff packet. Returns true if the char already had
// one in flight (this one must wait); false if it's now the only entry
// and the caller should process it immediately. Mirrors the legacy
// PushConCess "queue.size() > 1" gate.
bool PushConCess(TChar& ch, std::uint8_t server_id, std::uint16_t msg_id,
                 const std::vector<std::byte>& body)
{
    std::lock_guard g(ch.lock);
    ch.con_cess.push(ConCessEntry{server_id, msg_id, body});
    return ch.con_cess.size() > 1;
}

// Shared body of MW_CONLIST_ACK and MW_MAPSVRLIST_ACK — the legacy
// handlers (SSHandler.cpp:2020 / 2133) are byte-identical, so one
// implementation serves both. `what` only flavours the log line.
boost::asio::awaitable<void>
ReconcileConList(std::shared_ptr<PeerSession> peer,
                 std::vector<std::byte>       body,
                 const HandlerContext&        ctx,
                 const char*                  what)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("{}[{}]: registries not wired", what, ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  count = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(count))
    {
        spdlog::warn("{}[{}]: short body ({} bytes)", what, ip, body.size());
        co_return;
    }

    // The reporting map is always part of the needed set (legacy
    // inserts LOBYTE(pSERVER->m_wID) after reading the list).
    const std::uint8_t reporting_id =
        static_cast<std::uint8_t>(peer->Wid() & 0xFF);

    std::vector<std::uint8_t> needed;
    needed.reserve(static_cast<std::size_t>(count) + 1);
    for (std::uint8_t i = 0; i < count; ++i)
    {
        std::uint8_t sid = 0;
        if (!r.Read(sid))
        {
            spdlog::warn("{}[{}]: truncated server list (char_id={})",
                what, ip, char_id);
            co_return;
        }
        needed.push_back(sid);
    }
    needed.push_back(reporting_id);
    std::sort(needed.begin(), needed.end());
    needed.erase(std::unique(needed.begin(), needed.end()), needed.end());

    auto ch = ctx.chars->Find(char_id);
    if (!ch)
    {
        // Char gone (or never existed) — tell the reporting map to
        // drop the client (legacy SendMW_DELCHAR_REQ(id,key,TRUE,FALSE)).
        spdlog::info("{}[{}]: char_id={} not registered — DELCHAR", what, ip,
            char_id);
        co_await senders::SendMwDelCharReq(peer, char_id, key,
            /*logout=*/1, /*save=*/0);
        co_return;
    }

    // Reconcile under the char lock, snapshotting everything the
    // post-lock sends need (README §5: never hold the lock across a
    // co_await / peer lookup).
    bool          key_ok = true;
    std::uint8_t  main_id = 0, channel = 0;
    std::uint16_t map_id = 0;
    float         px = 0, py = 0, pz = 0;
    std::vector<std::uint8_t> new_servers;     // needed but not yet a con
    std::vector<std::uint8_t> live_servers;    // remaining cons after reconcile
    {
        std::lock_guard g(ch->lock);
        if (ch->key != key)
        {
            key_ok = false;
        }
        else
        {
            main_id = ch->main_server_id;
            channel = ch->channel;
            map_id  = ch->map_id;
            px = ch->pos_x; py = ch->pos_y; pz = ch->pos_z;

            // Drop connections the map no longer reports → dead_cons.
            auto is_dropped = [&](const TCharCon& c) {
                const bool keep = std::binary_search(
                    needed.begin(), needed.end(), c.server_id);
                if (!keep) ch->dead_cons.push_back(c.server_id);
                return !keep;
            };
            ch->cons.erase(
                std::remove_if(ch->cons.begin(), ch->cons.end(), is_dropped),
                ch->cons.end());

            for (const auto& c : ch->cons)
                live_servers.push_back(c.server_id);

            // Servers the char must newly connect to = needed minus
            // the connections it already has.
            for (std::uint8_t sid : needed)
                if (std::find_if(ch->cons.begin(), ch->cons.end(),
                        [sid](const TCharCon& c) {
                            return c.server_id == sid;
                        }) == ch->cons.end())
                    new_servers.push_back(sid);
        }
    }

    if (!key_ok)
    {
        spdlog::warn("{}[{}]: char_id={} key mismatch — DELCHAR",
            what, ip, char_id);
        co_await senders::SendMwDelCharReq(peer, char_id, key,
            /*logout=*/1, /*save=*/0);
        co_return;
    }

    auto main_peer = FindMapPeer(ctx, main_id);
    if (!main_peer)
    {
        // No main session — suspected stale/hijacked connect; tell
        // the reporting map to drop the client.
        spdlog::info("{}[{}]: char_id={} main_server_id={} offline — "
                     "INVALIDCHAR", what, ip, char_id, main_id);
        co_await senders::SendMwInvalidCharReq(peer, char_id, key,
            /*release_main=*/0);
        co_return;
    }

    if (!new_servers.empty())
    {
        // Ask the main map to route the char to the new servers; it
        // answers MW_ROUTE_ACK, forwarded down to the client.
        spdlog::info("{}[{}]: char_id={} routing to {} new server(s) "
                     "via main={}", what, ip, char_id, new_servers.size(),
            main_id);
        co_await senders::SendMwRouteListReq(main_peer, char_id, key,
            new_servers);
        co_return;
    }

    // No new connections needed — re-confirm the main session on
    // every remaining connection (legacy CheckMainCON).
    spdlog::info("{}[{}]: char_id={} CHECKMAIN to {} connection(s)",
        what, ip, char_id, live_servers.size());
    for (std::uint8_t sid : live_servers)
        if (auto p = FindMapPeer(ctx, sid))
            co_await senders::SendMwCheckMainReq(p, char_id, key,
                channel, map_id, px, py, pz);
}

// BeginTeleport — the actual teleport once the cession queue lets it
// run (legacy CTWorldSvrModule::OnBeginTeleport). The teleport must
// originate from the char's main map; from any other map it's ignored
// and the queue advances. On success world updates the char's
// destination and tells every connected map to start moving the avatar.
boost::asio::awaitable<void>
BeginTeleport(std::shared_ptr<PeerSession> peer,
              std::vector<std::byte>       body,
              const HandlerContext&        ctx)
{
    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  same_channel = 0, channel = 0;
    std::uint16_t map_id = 0;
    float         px = 0, py = 0, pz = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(same_channel) ||
        !r.Read(channel) || !r.Read(map_id) ||
        !r.Read(px) || !r.Read(py) || !r.Read(pz))
        co_return;

    auto ch = ctx.chars->Find(char_id);
    if (!ch) co_return;          // char gone (legacy CloseChar — deferred)

    const std::uint8_t reporting_id =
        static_cast<std::uint8_t>(peer->Wid() & 0xFF);
    std::uint8_t main_id = 0;
    { std::lock_guard g(ch->lock); main_id = ch->main_server_id; }

    auto main_peer = FindMapPeer(ctx, main_id);
    if (!main_peer)
    {
        // Main offline — legacy CloseChar (logout teardown, a later
        // slice). Advance the queue so it doesn't stall.
        spdlog::warn("BeginTeleport: char_id={} main={} offline "
                     "(CloseChar deferred) — advancing cession queue",
            char_id, main_id);
        co_await PopConCess(ch, ctx);
        co_return;
    }
    if (reporting_id != main_id)
    {
        // Teleport didn't come from the main session — ignore + advance.
        spdlog::info("BeginTeleport: char_id={} request from non-main {} "
                     "(main={}) — skipped", char_id, reporting_id, main_id);
        co_await PopConCess(ch, ctx);
        co_return;
    }

    // Move the char to the destination and announce to every map it is
    // connected to (the entry stays at the queue front until the
    // teleport's CHECKMAIN round-trip completes and pops it).
    std::vector<std::uint8_t> targets;
    {
        std::lock_guard g(ch->lock);
        ch->channel = channel; ch->map_id = map_id;
        ch->pos_x = px; ch->pos_y = py; ch->pos_z = pz;
        for (const auto& c : ch->cons)
            if (c.valid) targets.push_back(c.server_id);
    }
    spdlog::info("BeginTeleport: char_id={} → channel={} map={} "
                 "(STARTTELEPORT to {} con(s))", char_id, channel, map_id,
        targets.size());
    for (std::uint8_t sid : targets)
        if (auto p = FindMapPeer(ctx, sid))
            co_await senders::SendMwStartTeleportReq(p, char_id, key,
                channel, map_id, px, py, pz);
}

// PopConCess — the in-flight handoff completed; drop it and replay the
// next queued one (legacy CTWorldSvrModule::PopConCess).
boost::asio::awaitable<void>
PopConCess(std::shared_ptr<TChar> ch, const HandlerContext& ctx)
{
    ConCessEntry next;
    bool has_next = false;
    {
        std::lock_guard g(ch->lock);
        if (ch->con_cess.empty()) co_return;
        ch->con_cess.pop();
        if (!ch->con_cess.empty())
        {
            next = ch->con_cess.front();   // stays in queue until it completes
            has_next = true;
        }
    }
    if (!has_next) co_return;

    auto peer = FindMapPeer(ctx, next.server_id);
    if (!peer)
    {
        // Reporting map gone — skip this entry and try the one after.
        co_await PopConCess(ch, ctx);
        co_return;
    }
    switch (tnetlib::protocol::ToMessageId(next.msg_id))
    {
    case tnetlib::protocol::MessageId::MW_BEGINTELEPORT_ACK:
        co_await BeginTeleport(peer, std::move(next.body), ctx);
        break;
    default:
        // CHECKCONNECT_ACK replay lands with that handler (later slice).
        co_await PopConCess(ch, ctx);
        break;
    }
}

} // namespace

boost::asio::awaitable<void>
OnConListAck(std::shared_ptr<PeerSession> peer,
             std::vector<std::byte>       body,
             const HandlerContext&        ctx)
{
    co_await ReconcileConList(std::move(peer), std::move(body), ctx,
        "OnConListAck");
}

boost::asio::awaitable<void>
OnMapSvrListAck(std::shared_ptr<PeerSession> peer,
                std::vector<std::byte>       body,
                const HandlerContext&        ctx)
{
    co_await ReconcileConList(std::move(peer), std::move(body), ctx,
        "OnMapSvrListAck");
}

boost::asio::awaitable<void>
OnCheckMainAck(std::shared_ptr<PeerSession> peer,
               std::vector<std::byte>       body,
               const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnCheckMainAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnCheckMainAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto ch = ctx.chars->Find(char_id);
    if (!ch)
    {
        spdlog::info("OnCheckMainAck[{}]: char_id={} not registered — DELCHAR",
            ip, char_id);
        co_await senders::SendMwDelCharReq(peer, char_id, key,
            /*logout=*/1, /*save=*/0);
        co_return;
    }

    const std::uint8_t responding_id =
        static_cast<std::uint8_t>(peer->Wid() & 0xFF);

    // Snapshot under the char lock. For the "responder is main" case
    // we also drain dead_cons + capture the live con set here (the
    // responder is `peer`, so its main peer is guaranteed present —
    // no INVALIDCHAR can intervene before the drain). For the
    // "main changes" case we defer the main_server_id / save mutation
    // until after the old-main lookup succeeds (legacy checks
    // pMAIN before reassigning).
    bool          key_ok = true, is_main = false;
    std::uint8_t  old_main_id = 0, channel = 0;
    std::uint16_t map_id = 0;
    float         px = 0, py = 0, pz = 0;
    std::vector<std::uint8_t> dead;          // drained dead_cons
    std::vector<std::uint8_t> live_servers;  // current con set
    {
        std::lock_guard g(ch->lock);
        if (ch->key != key)
        {
            key_ok = false;
        }
        else
        {
            old_main_id = ch->main_server_id;
            channel = ch->channel; map_id = ch->map_id;
            px = ch->pos_x; py = ch->pos_y; pz = ch->pos_z;
            is_main = (responding_id == old_main_id);
            if (is_main)
            {
                dead.swap(ch->dead_cons);
                for (const auto& c : ch->cons)
                    live_servers.push_back(c.server_id);
            }
        }
    }

    if (!key_ok)
    {
        spdlog::warn("OnCheckMainAck[{}]: char_id={} key mismatch — DELCHAR",
            ip, char_id);
        co_await senders::SendMwDelCharReq(peer, char_id, key,
            /*logout=*/1, /*save=*/0);
        co_return;
    }

    auto main_peer = FindMapPeer(ctx, old_main_id);
    if (!main_peer)
    {
        spdlog::info("OnCheckMainAck[{}]: char_id={} main_server_id={} "
                     "offline — INVALIDCHAR", ip, char_id, old_main_id);
        co_await senders::SendMwInvalidCharReq(peer, char_id, key,
            /*release_main=*/0);
        co_return;
    }

    // TODO (full-logout slice): legacy runs `if(!m_bSave) CloseChar`
    // here — the friend/TMS/party/guild/tactics teardown + DELCHAR of
    // every connection. CloseChar is a subsystem of its own; the
    // logout path lands in a later slice.

    if (is_main)
    {
        // Responder owns the main session: close the dead connections
        // (ClearDeadCON) and confirm the connection set to the main.
        spdlog::info("OnCheckMainAck[{}]: char_id={} main confirmed "
                     "(server={}); closing {} dead con(s), CONRESULT {} "
                     "live", ip, char_id, old_main_id, dead.size(),
            live_servers.size());
        for (std::uint8_t sid : dead)
            if (auto p = FindMapPeer(ctx, sid))
                co_await senders::SendMwCloseCharReq(p, char_id, key);
        co_await senders::SendMwConResultReq(main_peer, char_id, key,
            kConSuccess, live_servers);
        // The confirmed main session completes any in-flight cession
        // entry (teleport / connect); pop it and replay the next.
        co_await PopConCess(ch, ctx);
        co_return;
    }

    // Responder is a different map → hand the main session over: tell
    // the old main to release, then re-point main at the responder.
    spdlog::info("OnCheckMainAck[{}]: char_id={} main handoff {} -> {}",
        ip, char_id, old_main_id, responding_id);
    co_await senders::SendMwReleaseMainReq(main_peer, char_id, key,
        channel, map_id, px, py, pz);
    {
        std::lock_guard g(ch->lock);
        ch->main_server_id = responding_id;
        ch->saving = false;
    }
}

boost::asio::awaitable<void>
OnReleaseMainAck(std::shared_ptr<PeerSession> peer,
                 std::vector<std::byte>       body,
                 const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnReleaseMainAck[{}]: registries not wired", ip);
        co_return;
    }

    // Body is opaque past these three fields — the tail is the char's
    // saved state, forwarded verbatim to the new main.
    wire::Reader r(body.data(), body.size());
    std::uint8_t  db_load = 0;
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(db_load) || !r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnReleaseMainAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto ch = ctx.chars->Find(char_id);
    bool found = ch != nullptr;
    std::uint8_t new_main_id = 0;
    if (found)
    {
        std::lock_guard g(ch->lock);
        if (ch->key != key) found = false;
        else                new_main_id = ch->main_server_id;
    }
    if (!found)
    {
        spdlog::info("OnReleaseMainAck[{}]: char_id={} not registered / key "
                     "mismatch — DELCHAR", ip, char_id);
        co_await senders::SendMwDelCharReq(peer, char_id, key,
            /*logout=*/1, /*save=*/0);
        co_return;
    }

    // The new main is whatever CHECKMAIN_ACK re-pointed main at; the
    // responder here is the *old* main that just released.
    auto main_peer = FindMapPeer(ctx, new_main_id);
    if (!main_peer)
    {
        spdlog::info("OnReleaseMainAck[{}]: char_id={} new main={} offline — "
                     "INVALIDCHAR", ip, char_id, new_main_id);
        co_await senders::SendMwInvalidCharReq(peer, char_id, key,
            /*release_main=*/1);
        co_return;
    }

    // Forward the released char to the new main; it loads the char and
    // replies MW_ENTERSVR_ACK to complete the handoff.
    co_await senders::SendMwEnterSvrReq(main_peer, body);
    {
        std::lock_guard g(ch->lock);
        ch->chg_main_id = static_cast<std::uint8_t>(peer->Wid() & 0xFF);
    }
    spdlog::info("OnReleaseMainAck[{}]: char_id={} forwarded to new main={} "
                 "(old main={})", ip, char_id, new_main_id, ch->chg_main_id);
}

boost::asio::awaitable<void>
OnBeginTeleportAck(std::shared_ptr<PeerSession> peer,
                   std::vector<std::byte>       body,
                   const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnBeginTeleportAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  same_channel = 0, channel = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(same_channel) ||
        !r.Read(channel))
    {
        spdlog::warn("OnBeginTeleportAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto ch = ctx.chars->Find(char_id);
    bool ok = ch != nullptr;
    if (ok)
    {
        std::lock_guard g(ch->lock);
        if (ch->key != key) ok = false;
    }
    if (!ok)
    {
        spdlog::info("OnBeginTeleportAck[{}]: char_id={} not registered / key "
                     "mismatch — DELCHAR", ip, char_id);
        co_await senders::SendMwDelCharReq(peer, char_id, key,
            /*logout=*/1, /*save=*/0);
        co_return;
    }

    // Same-channel teleport: no map handoff, just record the channel.
    if (same_channel)
    {
        std::lock_guard g(ch->lock);
        ch->channel = channel;
        co_return;
    }

    // Serialize against any in-flight handoff. If this is the only
    // entry, run it now; otherwise it waits for PopConCess to replay it.
    const std::uint8_t reporting_id =
        static_cast<std::uint8_t>(peer->Wid() & 0xFF);
    if (PushConCess(*ch, reporting_id,
            tnetlib::protocol::ToUint16(
                tnetlib::protocol::MessageId::MW_BEGINTELEPORT_ACK),
            body))
    {
        spdlog::info("OnBeginTeleportAck[{}]: char_id={} deferred behind an "
                     "in-flight handoff", ip, char_id);
        co_return;
    }
    co_await BeginTeleport(std::move(peer), std::move(body), ctx);
}

} // namespace tworldsvr::handlers
