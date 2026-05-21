// F5 — patch metadata + castle handlers.
//
//   CT_UPDATEPATCH_REQ      — promote a list of files into TVERSION.
//   CT_PREVERSIONTABLE_REQ  — read TPREVERSION snapshot.
//   CT_PREVERSIONUPDATE_REQ — three-pass batch: beta-to-version,
//                             delete-pre-version, insert new pre,
//                             then send the refreshed table back.
//   CT_CASTLEINFO_REQ       — forward to a MapSvr in the world.
//   CT_CASTLEGUILDCHG_REQ   — forward to the WorldSvr in the world.
//   CT_CASTLEENABLE_REQ     — broadcast SM_BATTLESTATUS to WorldSvr.
//   CT_CASTLEINFO_ACK /
//   CT_CASTLEGUILDCHG_ACK   — peer → control: route back to the
//                             originating operator (DWORD manager_id
//                             at the head of the body).

#include "handlers.h"

#include "../senders.h"
#include "../wire_codec.h"
#include "../services/authority_gate.h"
#include "../services/svr_type.h"
#include "MessageId.h"

#include "fourstory/db/co_offload.h"

#include <spdlog/spdlog.h>

namespace tcontrolsvr::handlers {

namespace {

constexpr std::uint8_t kBattleTypeCastle = 1;   // legacy BT_CASTLE

} // namespace

boost::asio::awaitable<void>
OnUpdatePatchReq(std::shared_ptr<OperatorSession> op,
                 std::vector<std::byte> body,
                 const HandlerContext& ctx)
{
    if (!op->LoggedIn() || !HasAuthority(*op, OperatorRole::Control))
    {
        if (ctx.audit)
            ctx.audit->LogAuthorityDenied(op->UserId(),
                op->AuthorityRaw(), "update_patch");
        co_await senders::SendAuthorityAck(op->Wire());
        co_return;
    }

    wire::Reader r(body);
    std::uint16_t count = 0;
    if (!r.Read(count))
    {
        spdlog::warn("CT_UPDATEPATCH_REQ malformed (no count)");
        co_return;
    }
    spdlog::info("CT_UPDATEPATCH_REQ count={} op='{}'", count, op->UserId());
    for (std::uint16_t i = 0; i < count; ++i)
    {
        PatchUpdateRow row{};
        std::uint32_t size = 0;
        if (!r.ReadString(row.path) || !r.ReadString(row.name) ||
            !r.Read(size))
            break;
        row.size = size;
        if (ctx.patch_meta)
        {
            co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                [pm = ctx.patch_meta, &row] { pm->UpdatePatch(row, 0); });
        }
        if (ctx.audit)
            ctx.audit->LogAdminAction(op->UserId(), "update_patch",
                row.path + "/" + row.name);
    }
}

boost::asio::awaitable<void>
OnPreVersionTableReq(std::shared_ptr<OperatorSession> op,
                     std::vector<std::byte> /*body*/,
                     const HandlerContext& ctx)
{
    if (!op->LoggedIn() || !HasAuthority(*op, OperatorRole::Control))
    {
        if (ctx.audit)
            ctx.audit->LogAuthorityDenied(op->UserId(),
                op->AuthorityRaw(), "pre_version_table");
        co_await senders::SendAuthorityAck(op->Wire());
        co_return;
    }
    std::vector<senders::PreVersionAckRow> rows;
    if (ctx.patch_meta)
    {
        auto pre = co_await fourstory::db::CoOffloadIf(ctx.db_pool,
            [pm = ctx.patch_meta] { return pm->ListPreVersions(); });
        for (const auto& r : pre)
            rows.push_back({r.beta_ver, r.path, r.name, r.size});
    }
    co_await senders::SendPreVersionTableAck(op->Wire(), rows);
}

boost::asio::awaitable<void>
OnPreVersionUpdateReq(std::shared_ptr<OperatorSession> op,
                      std::vector<std::byte> body,
                      const HandlerContext& ctx)
{
    if (!op->LoggedIn() || !HasAuthority(*op, OperatorRole::Control))
    {
        if (ctx.audit)
            ctx.audit->LogAuthorityDenied(op->UserId(),
                op->AuthorityRaw(), "pre_version_update");
        co_await senders::SendAuthorityAck(op->Wire());
        co_return;
    }
    if (!ctx.patch_meta) co_return;

    wire::Reader r(body);
    std::uint16_t count = 0;

    // Phase 1 — promote betas.
    if (!r.Read(count)) co_return;
    for (std::uint16_t i = 0; i < count; ++i)
    {
        std::uint32_t beta = 0;
        if (!r.Read(beta)) break;
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [pm = ctx.patch_meta, beta] { pm->BetaToVersion(beta); });
    }

    // Phase 2 — delete betas.
    if (!r.Read(count)) co_return;
    for (std::uint16_t i = 0; i < count; ++i)
    {
        std::uint32_t beta = 0;
        if (!r.Read(beta)) break;
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [pm = ctx.patch_meta, beta] { pm->DeletePreVersion(beta); });
    }

    // Phase 3 — insert new pre.
    if (!r.Read(count)) co_return;
    for (std::uint16_t i = 0; i < count; ++i)
    {
        PatchUpdateRow row{};
        std::uint32_t size = 0;
        if (!r.ReadString(row.path) || !r.ReadString(row.name) ||
            !r.Read(size))
            break;
        row.size = size;
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [pm = ctx.patch_meta, &row] { pm->UpdatePrePatch(row); });
    }

    // Reply with the refreshed PreVersion table.
    auto pre = co_await fourstory::db::CoOffloadIf(ctx.db_pool,
        [pm = ctx.patch_meta] { return pm->ListPreVersions(); });
    std::vector<senders::PreVersionAckRow> rows;
    rows.reserve(pre.size());
    for (const auto& pr : pre)
        rows.push_back({pr.beta_ver, pr.path, pr.name, pr.size});
    co_await senders::SendPreVersionTableAck(op->Wire(), rows);
}

namespace {

// Castle handlers all gate on MANAGER_ALL — keep the wrapper here.
boost::asio::awaitable<bool>
CastleGate(const std::shared_ptr<OperatorSession>& op,
           const std::string& action, const HandlerContext& ctx)
{
    if (!op->LoggedIn() || !HasAuthority(*op, OperatorRole::All))
    {
        if (ctx.audit)
            ctx.audit->LogAuthorityDenied(op->UserId(),
                op->AuthorityRaw(), action);
        co_await senders::SendAuthorityAck(op->Wire());
        co_return false;
    }
    co_return true;
}

} // namespace

boost::asio::awaitable<void>
OnCastleInfoReq(std::shared_ptr<OperatorSession> op,
                std::vector<std::byte> body,
                const HandlerContext& ctx)
{
    if (!co_await CastleGate(op, "castle_info", ctx)) co_return;
    wire::Reader r(body);
    std::uint8_t world = 0;
    if (!r.Read(world)) co_return;
    if (!ctx.peers) co_return;
    auto maps = ctx.peers->FindByType(world, svr_type::kMapSvr);
    if (maps.empty()) co_return;
    co_await senders::SendCastleInfoReq(maps.front()->Wire(),
        op->ManagerSeq());
}

boost::asio::awaitable<void>
OnCastleGuildChgReq(std::shared_ptr<OperatorSession> op,
                    std::vector<std::byte> body,
                    const HandlerContext& ctx)
{
    if (!co_await CastleGate(op, "castle_guild_change", ctx)) co_return;
    wire::Reader r(body);
    std::uint8_t  world = 0;
    std::uint16_t castle_id = 0;
    std::uint32_t def_guild = 0, atk_guild = 0;
    std::int64_t  t = 0;
    if (!r.Read(world)     || !r.Read(castle_id) ||
        !r.Read(def_guild) || !r.Read(atk_guild) || !r.Read(t))
        co_return;
    if (!ctx.peers) co_return;
    for (const auto& peer : ctx.peers->FindByType(world, svr_type::kWorldSvr))
        co_await senders::SendCastleGuildChangeReq(peer->Wire(),
            castle_id, def_guild, atk_guild, op->ManagerSeq(), t);
}

boost::asio::awaitable<void>
OnCastleEnableReq(std::shared_ptr<OperatorSession> op,
                  std::vector<std::byte> body,
                  const HandlerContext& ctx)
{
    if (!co_await CastleGate(op, "castle_enable", ctx)) co_return;
    wire::Reader r(body);
    std::uint8_t  world = 0;
    std::uint8_t  status = 0;
    std::uint32_t seconds = 0;
    if (!r.Read(world) || !r.Read(status) || !r.Read(seconds)) co_return;
    if (!ctx.peers) co_return;
    for (const auto& peer : ctx.peers->FindByType(world, svr_type::kWorldSvr))
        co_await senders::SendBattleStatusReq(peer->Wire(),
            kBattleTypeCastle, status, seconds);
}

namespace {

boost::asio::awaitable<void>
RouteAckBackToOperator(std::vector<std::byte> body,
                       std::uint16_t out_id,
                       const HandlerContext& ctx)
{
    wire::Reader r(body);
    std::uint32_t manager_id = 0;
    if (!r.Read(manager_id)) co_return;
    if (!ctx.operators) co_return;
    auto op = ctx.operators->FindBySeq(manager_id);
    if (!op || !op->Wire() || !op->Wire()->IsOpen()) co_return;
    co_await senders::SendRawForward(op->Wire(), out_id, body);
}

} // namespace

boost::asio::awaitable<void>
OnPeerCastleInfoAck(std::shared_ptr<PeerSession> /*peer*/,
                    std::vector<std::byte> body,
                    const HandlerContext& ctx)
{
    co_await RouteAckBackToOperator(std::move(body),
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CT_CASTLEINFO_ACK),
        ctx);
}

boost::asio::awaitable<void>
OnPeerCastleGuildChgAck(std::shared_ptr<PeerSession> /*peer*/,
                        std::vector<std::byte> body,
                        const HandlerContext& ctx)
{
    co_await RouteAckBackToOperator(std::move(body),
        tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CT_CASTLEGUILDCHG_ACK),
        ctx);
}

} // namespace tcontrolsvr::handlers
