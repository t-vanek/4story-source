// F3 — admin operation forwarders. Each handler mirrors the
// equivalent legacy entry point in Server/TControlSvr/Handler.cpp:
//
//   CT_ANNOUNCEMENT_REQ   — broadcast to Relay (preferred) or Map
//   CT_USERKICKOUT_REQ    — broadcast to every Map peer
//   CT_USERMOVE_REQ       — single World peer, N targets
//   CT_USERPOSITION_REQ   — single World peer, N movers
//   CT_USERPROTECTED_REQ  — DB SP only, no peer forward
//   CT_CHARMSG_REQ        — Relay broadcast (preferred), fallback World
//   CT_CHATBAN_REQ        — World+Relay broadcast with N-wave ack agg
//   CT_CHATBAN_ACK        — peer → control: aggregate, reply to op
//   CT_CHATBANLIST_REQ    — read from local registry
//   CT_CHATBANLISTDEL_REQ — delete from registry
//   CT_MONSPAWNFIND_REQ   — broadcast to Map peers in the group
//
// Authority gates match the legacy CheckAuthority(MANAGER_*) calls.
// Every successful handler emits an audit record via IAdminAuditLogger
// before forwarding; the legacy LogSmartly("TCONTROL: …") output is
// replaced by structured key=value log lines on the shared audit
// channel.

#include "handlers.h"

#include "../senders.h"
#include "../wire_codec.h"
#include "../services/authority_gate.h"
#include "../services/svr_type.h"
#include "MessageId.h"

#include <spdlog/spdlog.h>

namespace tcontrolsvr::handlers {

namespace {

// Common pattern: deny + emit audit + send CT_AUTHORITY_ACK + return.
// Returns true if the operator passes the gate.
boost::asio::awaitable<bool>
GateCheck(const std::shared_ptr<OperatorSession>& op,
          OperatorRole required,
          const std::string& action,
          const HandlerContext& ctx)
{
    if (!op->LoggedIn() || !HasAuthority(*op, required))
    {
        if (ctx.audit)
            ctx.audit->LogAuthorityDenied(op->UserId(),
                op->AuthorityRaw(), action);
        co_await senders::SendAuthorityAck(op->Wire());
        co_return false;
    }
    co_return true;
}

// Iterate live peer connections in a group + type filter. Returns
// the snapshot under the registry's invariants.
std::vector<std::shared_ptr<PeerSession>>
PeersByType(const HandlerContext& ctx,
            std::uint8_t group_id, std::uint8_t type_id)
{
    if (!ctx.peers) return {};
    return ctx.peers->FindByType(group_id, type_id);
}

// All connections of a given type across every group. Mirrors the
// legacy "for every TSVRTEMP if bType == X" iteration.
std::vector<std::shared_ptr<PeerSession>>
PeersByTypeAll(const HandlerContext& ctx, std::uint8_t type_id)
{
    std::vector<std::shared_ptr<PeerSession>> out;
    if (!ctx.peers) return out;
    for (const auto& svc : ctx.peers->Services())
    {
        if (svc.type_id != type_id) continue;
        auto conn = ctx.peers->Connection(svc.service_id);
        if (conn && conn->Wire() && conn->Wire()->IsOpen())
            out.push_back(std::move(conn));
    }
    return out;
}

} // namespace

boost::asio::awaitable<void>
OnAnnouncementReq(std::shared_ptr<OperatorSession> op,
                  std::vector<std::byte> body,
                  const HandlerContext& ctx)
{
    if (!co_await GateCheck(op, OperatorRole::GMLevel1,
                            "announcement", ctx))
        co_return;

    wire::Reader r(body);
    std::uint32_t world_filter = 0;
    std::string   message;
    if (!r.Read(world_filter) || !r.ReadString(message))
    {
        spdlog::warn("CT_ANNOUNCEMENT_REQ malformed body");
        co_return;
    }
    if (ctx.audit)
        ctx.audit->LogAnnouncement(op->UserId(), world_filter, message);

    if (!ctx.peers) co_return;

    // Legacy two-pass forward: try Relay first (one peer per group);
    // if no Relay is live in a group that's targeted, fall back to
    // every Map peer in that group. world_filter == 0 means "every
    // world".
    std::vector<std::uint8_t> needs_map_fallback;
    for (const auto& svc : ctx.peers->Services())
    {
        if (svc.type_id != svr_type::kRlySvr) continue;
        if (world_filter != 0 && world_filter != svc.group_id) continue;
        auto conn = ctx.peers->Connection(svc.service_id);
        if (conn && conn->Wire() && conn->Wire()->IsOpen())
        {
            co_await senders::SendAnnouncementAck(conn->Wire(), message);
            // For a specific world filter the legacy code stops after
            // the first relay match. Mirror that to avoid double-send.
            if (world_filter != 0 && world_filter == svc.group_id)
                co_return;
        }
        else if (world_filter == 0 || world_filter == svc.group_id)
        {
            needs_map_fallback.push_back(svc.group_id);
        }
    }
    for (auto gid : needs_map_fallback)
    {
        for (const auto& map_peer : PeersByType(ctx, gid, svr_type::kMapSvr))
            co_await senders::SendAnnouncementAck(map_peer->Wire(), message);
    }
}

boost::asio::awaitable<void>
OnUserKickoutReq(std::shared_ptr<OperatorSession> op,
                 std::vector<std::byte> body,
                 const HandlerContext& ctx)
{
    if (!co_await GateCheck(op, OperatorRole::GMLevel2, "kick", ctx))
        co_return;

    wire::Reader r(body);
    std::string user;
    if (!r.ReadString(user))
    {
        spdlog::warn("CT_USERKICKOUT_REQ malformed body");
        co_return;
    }
    if (ctx.audit)
        ctx.audit->LogKick(op->UserId(), user, AdminOutcome::Success);

    for (const auto& peer : PeersByTypeAll(ctx, svr_type::kMapSvr))
        co_await senders::SendUserKickoutAck(peer->Wire(), user);
}

boost::asio::awaitable<void>
OnUserMoveReq(std::shared_ptr<OperatorSession> op,
              std::vector<std::byte> body,
              const HandlerContext& ctx)
{
    if (!co_await GateCheck(op, OperatorRole::GMLevel2, "move", ctx))
        co_return;

    wire::Reader r(body);
    std::uint8_t  world   = 0;
    std::uint8_t  channel = 0;
    std::uint16_t map_id  = 0;
    float         x = 0, y = 0, z = 0;
    std::uint16_t count   = 0;
    if (!r.Read(world)   || !r.Read(channel) || !r.Read(map_id) ||
        !r.Read(x) || !r.Read(y) || !r.Read(z) || !r.Read(count))
    {
        spdlog::warn("CT_USERMOVE_REQ malformed body");
        co_return;
    }

    // Legacy resolves to the WorldSvr in the named group. There's
    // exactly one per group; pick the first live connection.
    auto worlds = PeersByType(ctx, world, svr_type::kWorldSvr);
    if (worlds.empty())
    {
        spdlog::info("CT_USERMOVE_REQ world={} — no WorldSvr peer live",
            world);
        co_return;
    }
    auto& target = worlds.front();

    for (std::uint16_t i = 0; i < count; ++i)
    {
        std::string user;
        if (!r.ReadString(user)) break;
        if (ctx.audit)
            ctx.audit->LogMove(op->UserId(), user, world, map_id);
        co_await senders::SendUserMoveAck(target->Wire(),
            user, channel, map_id, x, y, z);
    }
}

boost::asio::awaitable<void>
OnUserPositionReq(std::shared_ptr<OperatorSession> op,
                  std::vector<std::byte> body,
                  const HandlerContext& ctx)
{
    if (!co_await GateCheck(op, OperatorRole::GMLevel2,
                            "user_position", ctx))
        co_return;

    wire::Reader r(body);
    std::uint8_t  world = 0;
    std::string   target_name;
    std::uint16_t count = 0;
    if (!r.Read(world) || !r.ReadString(target_name) || !r.Read(count))
    {
        spdlog::warn("CT_USERPOSITION_REQ malformed body");
        co_return;
    }
    auto worlds = PeersByType(ctx, world, svr_type::kWorldSvr);
    if (worlds.empty())
    {
        spdlog::info("CT_USERPOSITION_REQ world={} — no WorldSvr peer live",
            world);
        co_return;
    }
    auto& peer = worlds.front();

    for (std::uint16_t i = 0; i < count; ++i)
    {
        std::string mover;
        if (!r.ReadString(mover)) break;
        if (ctx.audit)
            ctx.audit->LogTeleportTo(op->UserId(), mover, target_name);
        co_await senders::SendUserPositionAck(peer->Wire(),
            target_name, mover);
    }
}

boost::asio::awaitable<void>
OnUserProtectedReq(std::shared_ptr<OperatorSession> op,
                   std::vector<std::byte> body,
                   const HandlerContext& ctx)
{
    if (!co_await GateCheck(op, OperatorRole::GMLevel1, "ban", ctx))
        co_return;

    wire::Reader r(body);
    std::string  user, reason;
    std::uint32_t duration = 0;
    std::uint8_t  permanent = 0;
    if (!r.ReadString(user) || !r.Read(duration) ||
        !r.ReadString(reason) || !r.Read(permanent))
    {
        spdlog::warn("CT_USERPROTECTED_REQ malformed body");
        co_await senders::SendUserProtectedAck(op->Wire(), 0);
        co_return;
    }

    std::uint8_t ret = 0;
    if (ctx.user_ban)
    {
        ret = ctx.user_ban->AddBan(user, duration, reason,
                                   permanent, op->UserId());
    }
    if (ctx.audit)
        ctx.audit->LogBan(op->UserId(), user, duration, permanent, reason,
            ret ? AdminOutcome::Success : AdminOutcome::Failed);

    co_await senders::SendUserProtectedAck(op->Wire(), ret);
}

boost::asio::awaitable<void>
OnCharMsgReq(std::shared_ptr<OperatorSession> op,
             std::vector<std::byte> body,
             const HandlerContext& ctx)
{
    if (!co_await GateCheck(op, OperatorRole::GMLevel2, "char_msg", ctx))
        co_return;

    wire::Reader r(body);
    std::string user, message;
    if (!r.ReadString(user) || !r.ReadString(message))
    {
        spdlog::warn("CT_CHARMSG_REQ malformed body");
        co_return;
    }
    if (ctx.audit)
        ctx.audit->LogCharMsg(op->UserId(), user, message);

    if (!ctx.peers) co_return;
    std::vector<std::uint8_t> needs_world_fallback;
    for (const auto& svc : ctx.peers->Services())
    {
        if (svc.type_id != svr_type::kRlySvr) continue;
        auto conn = ctx.peers->Connection(svc.service_id);
        if (conn && conn->Wire() && conn->Wire()->IsOpen())
            co_await senders::SendCharMsgAck(conn->Wire(), user, message);
        else
            needs_world_fallback.push_back(svc.group_id);
    }
    for (auto gid : needs_world_fallback)
    {
        for (const auto& w : PeersByType(ctx, gid, svr_type::kWorldSvr))
            co_await senders::SendCharMsgAck(w->Wire(), user, message);
    }
}

boost::asio::awaitable<void>
OnChatBanReq(std::shared_ptr<OperatorSession> op,
             std::vector<std::byte> body,
             const HandlerContext& ctx)
{
    if (!co_await GateCheck(op, OperatorRole::GMLevel3, "chat_ban", ctx))
        co_return;

    wire::Reader r(body);
    std::string user, reason;
    std::uint16_t minutes = 0;
    if (!r.ReadString(user) || !r.Read(minutes) || !r.ReadString(reason))
    {
        spdlog::warn("CT_CHATBAN_REQ malformed body");
        co_return;
    }
    if (ctx.audit)
        ctx.audit->LogChatBan(op->UserId(), user, minutes, reason);

    if (!ctx.peers || !ctx.chat_bans) co_return;

    // Count expected World acks before allocating the seq — the
    // aggregator needs to know how many CT_CHATBAN_ACK responses to
    // wait for. Legacy counts only World peers (not Relay).
    std::uint32_t world_count = 0;
    std::vector<std::shared_ptr<PeerSession>> forward_to;
    for (const auto& svc : ctx.peers->Services())
    {
        if (svc.type_id != svr_type::kWorldSvr &&
            svc.type_id != svr_type::kRlySvr) continue;
        auto conn = ctx.peers->Connection(svc.service_id);
        if (!conn || !conn->Wire() || !conn->Wire()->IsOpen()) continue;
        forward_to.push_back(conn);
        if (svc.type_id == svr_type::kWorldSvr) world_count += 1;
    }
    if (forward_to.empty())
    {
        // Nothing to forward to — report failure right away.
        co_await senders::SendChatBanAck(op->Wire(), 0);
        co_return;
    }

    const std::uint32_t seq = ctx.chat_bans->CreateBan(
        op->UserId(), user, minutes, reason, world_count, op->ManagerSeq());
    for (const auto& peer : forward_to)
        co_await senders::SendChatBanReq(peer->Wire(),
            user, minutes, seq, op->ManagerSeq());

    // If no World peers were live (only Relays got the broadcast),
    // there's no ack coming back — the legacy code returned without
    // replying. Match that contract (operator sees no ack until
    // someone responds).
}

boost::asio::awaitable<void>
OnPeerChatBanAck(std::shared_ptr<PeerSession> /*peer*/,
                 std::vector<std::byte> body,
                 const HandlerContext& ctx)
{
    wire::Reader r(body);
    std::uint8_t  bret = 0;
    std::uint32_t seq = 0, manager_id = 0;
    if (!r.Read(bret) || !r.Read(seq) || !r.Read(manager_id))
    {
        spdlog::warn("peer CT_CHATBAN_ACK malformed body");
        co_return;
    }
    if (!ctx.chat_bans) co_return;
    const auto result = ctx.chat_bans->ApplyAck(seq, bret);
    if (!result.completed) co_return;

    if (!ctx.operators) co_return;
    auto op_sess = ctx.operators->FindBySeq(result.manager_seq
        ? result.manager_seq : manager_id);
    if (!op_sess) co_return;
    co_await senders::SendChatBanAck(op_sess->Wire(),
        result.success ? 1 : 0);
}

boost::asio::awaitable<void>
OnChatBanListReq(std::shared_ptr<OperatorSession> op,
                 std::vector<std::byte> /*body*/,
                 const HandlerContext& ctx)
{
    if (!co_await GateCheck(op, OperatorRole::All,
                            "chat_ban_list", ctx))
        co_return;

    if (!ctx.chat_bans)
    {
        co_await senders::SendChatBanListAck(op->Wire(), {});
        co_return;
    }
    const auto bans = ctx.chat_bans->List();
    std::vector<senders::ChatBanRow> rows;
    rows.reserve(bans.size());
    for (const auto& b : bans)
        rows.push_back({b.seq, b.operator_id, b.target_user,
                        b.minutes, b.reason, b.created_unix});
    co_await senders::SendChatBanListAck(op->Wire(), rows);
}

boost::asio::awaitable<void>
OnChatBanListDelReq(std::shared_ptr<OperatorSession> op,
                    std::vector<std::byte> body,
                    const HandlerContext& ctx)
{
    if (!co_await GateCheck(op, OperatorRole::All,
                            "chat_ban_list_del", ctx))
        co_return;

    wire::Reader r(body);
    std::uint32_t id = 0;
    if (!r.Read(id))
    {
        spdlog::warn("CT_CHATBANLISTDEL_REQ malformed body");
        co_return;
    }
    if (ctx.chat_bans) ctx.chat_bans->Delete(id);
}

boost::asio::awaitable<void>
OnMonSpawnFindReq(std::shared_ptr<OperatorSession> op,
                  std::vector<std::byte> body,
                  const HandlerContext& ctx)
{
    if (!co_await GateCheck(op, OperatorRole::GMLevel1,
                            "mon_spawn_find", ctx))
        co_return;

    wire::Reader r(body);
    std::uint8_t  group = 0, channel = 0;
    std::uint16_t map_id = 0, spawn_id = 0;
    if (!r.Read(group) || !r.Read(channel) ||
        !r.Read(map_id) || !r.Read(spawn_id))
    {
        spdlog::warn("CT_MONSPAWNFIND_REQ malformed body");
        co_return;
    }
    for (const auto& peer : PeersByType(ctx, group, svr_type::kMapSvr))
        co_await senders::SendMonSpawnFindAck(peer->Wire(),
            op->ManagerSeq(), channel, map_id, spawn_id);
}

} // namespace tcontrolsvr::handlers
