// W6-45 — CMGift family: the operator / in-game GM cash-gift
// delivery pipeline + the gift-catalogue list & update tools.
// Collapses the legacy CT/MW → DM_CMGIFT_REQ → DM_CMGIFT_ACK chain
// (SSHandler.cpp:456 / 13610 / 13634 / 13670) and the CHARTUPDATE
// round-trip (483 / 13789 / 13931) into coroutines over
// CmGiftRegistry + ICmGiftRepository.

#include "handlers.h"
#include "../senders/senders.h"
#include "../wire_codec.h"

#include "fourstory/db/co_offload.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
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

// The legacy error-reply tail (OnDM_CMGIFT_ACK, SSHandler.cpp:13760):
// tool path → CT_CMGIFT_ACK on the ctrl-svr (legacy dereferences
// m_pCtrlSvr unguarded — we guard and drop); in-game path →
// MW_CMGIFTRESULT_REQ to the GM's main map.
boost::asio::awaitable<void>
ReplyGiftResult(const HandlerContext& ctx, std::uint8_t tool,
                std::uint32_t gm_id, std::uint8_t ret)
{
    if (tool)
    {
        if (ctx.ctrl_svr)
            if (auto cs = ctx.ctrl_svr->Get())
            {
                co_await senders::SendCtCmGiftAck(cs, ret, gm_id);
                co_return;
            }
        spdlog::info("ReplyGiftResult: ctrl-svr offline — CT_CMGIFT_ACK"
                     "(ret={}, manager={}) dropped", ret, gm_id);
        co_return;
    }
    if (!ctx.chars) co_return;
    auto gm = ctx.chars->Find(gm_id);
    if (!gm) co_return;
    std::uint8_t msi = 0;
    {
        std::lock_guard g(gm->lock);
        msi = gm->main_server_id;
    }
    if (auto p = FindMapPeer(ctx, msi))
        co_await senders::SendMwCmGiftResultReq(p, ret, gm_id);
    co_return;
}

// The collapsed delivery pipeline (legacy entry + DM_ACK logic).
boost::asio::awaitable<void>
DeliverCmGift(const HandlerContext& ctx, std::string target_name,
              std::uint16_t gift_id, std::uint8_t tool,
              std::uint32_t gm_id)
{
    if (!ctx.cmgifts)
    {
        spdlog::warn("DeliverCmGift: cmgifts not wired");
        co_return;
    }

    auto gift = ctx.cmgifts->Get(gift_id);
    if (!gift)
    {
        co_await ReplyGiftResult(ctx, tool, gm_id, kCmGiftId);
        co_return;
    }

    // take_type gifts run the per-target TCMGiftCanTake check
    // (legacy DM_CMGIFT_REQ); others go straight to SUCCESS.
    std::uint8_t ret = kCmGiftSuccess;
    if (gift->take_type)
    {
        if (!ctx.cmgift_repo)
        {
            spdlog::warn("DeliverCmGift: cmgift_repo not wired — "
                         "take-type gift {} dropped as FAIL", gift_id);
            co_await ReplyGiftResult(ctx, tool, gm_id, kCmGiftFail);
            co_return;
        }
        ret = co_await fourstory::db::CoOffloadIf(ctx.db_pool,
            [repo = ctx.cmgift_repo, target_name, gift_id]
            { return repo->CanTake(target_name, gift_id); });
    }

    // --- legacy OnDM_CMGIFT_ACK delivery logic -------------------
    if (ret == kCmGiftSuccess || ret == kCmGiftDuplicate)
    {
        const std::uint16_t requested_id = gift_id;
        if (ret == kCmGiftDuplicate)
        {
            auto err = ctx.cmgifts->Get(gift->err_gift_id);
            if (!err)
            {
                // No fallback gift — report DUPLICATE as-is.
                co_await ReplyGiftResult(ctx, tool, gm_id, ret);
                co_return;
            }
            gift = err;
            ret  = kCmGiftErrPost;
        }

        if (gift->tool_only <= tool)
        {
            std::shared_ptr<TChar> target;
            if (ctx.chars)
                target = ctx.chars->FindByName(target_name);
            if (target)
            {
                std::uint32_t target_id = 0;
                std::uint8_t  msi = 0;
                {
                    std::lock_guard g(target->lock);
                    target_id = target->char_id;
                    msi = target->main_server_id;
                }
                if (auto p = FindMapPeer(ctx, msi))
                {
                    co_await senders::SendMwCmGiftReq(p, target_id,
                        *gift, gm_id, tool, requested_id, ret);
                    co_return;
                }
                ret = kCmGiftFail;           // main map offline
            }
            else
                ret = kCmGiftTarget;         // target not online
        }
        else
            ret = kCmGiftFail;               // tool-only gate
    }

    co_await ReplyGiftResult(ctx, tool, gm_id, ret);
    co_return;
}

} // namespace

boost::asio::awaitable<void>
OnCtCmGiftReq(std::shared_ptr<PeerSession> peer,
              std::vector<std::byte>       body,
              const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::string   target;
    std::uint16_t gift_id = 0;
    std::uint32_t manager = 0;
    if (!r.ReadString(target) || !r.Read(gift_id) || !r.Read(manager))
    {
        spdlog::warn("OnCtCmGiftReq[{}]: malformed body ({} bytes)",
            ip, body.size());
        co_return;
    }
    co_await DeliverCmGift(ctx, std::move(target), gift_id,
        /*tool=*/1, manager);
}

boost::asio::awaitable<void>
OnMwCmGiftAck(std::shared_ptr<PeerSession> peer,
              std::vector<std::byte>       body,
              const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    std::string   target;
    std::uint16_t gift_id = 0;
    std::uint32_t gm_char = 0;
    if (!r.ReadString(target) || !r.Read(gift_id) || !r.Read(gm_char))
    {
        spdlog::warn("OnMwCmGiftAck[{}]: malformed body ({} bytes)",
            ip, body.size());
        co_return;
    }
    co_await DeliverCmGift(ctx, std::move(target), gift_id,
        /*tool=*/0, gm_char);
}

boost::asio::awaitable<void>
OnCtCmGiftListReq(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.cmgifts || !ctx.ctrl_svr)
    {
        spdlog::warn("OnCtCmGiftListReq[{}]: cmgifts/ctrl_svr not "
                     "wired", ip);
        co_return;
    }
    wire::Reader r(body.data(), body.size());
    std::uint32_t manager = 0;
    if (!r.Read(manager))
    {
        spdlog::warn("OnCtCmGiftListReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }
    if (auto cs = ctx.ctrl_svr->Get())
        co_await senders::SendCtCmGiftListAck(cs, manager,
            ctx.cmgifts->Snapshot());
    else
        spdlog::info("OnCtCmGiftListReq[{}]: ctrl-svr offline — list "
                     "dropped (legacy if(m_pCtrlSvr))", ip);
    co_return;
}

boost::asio::awaitable<void>
OnCtCmGiftChartUpdateReq(std::shared_ptr<PeerSession> peer,
                         std::vector<std::byte>       body,
                         const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.cmgifts)
    {
        spdlog::warn("OnCtCmGiftChartUpdateReq[{}]: cmgifts not wired",
            ip);
        co_return;
    }
    if (!ctx.cmgift_repo)
    {
        spdlog::warn("OnCtCmGiftChartUpdateReq[{}]: cmgift_repo not "
                     "wired — dropping (no DB configured)", ip);
        co_return;
    }

    struct Entry
    {
        std::uint8_t  type = 0;
        CmGift        gift;                  // ADD / UPDATE payload
        std::uint16_t del_id = 0;            // DEL payload
    };

    wire::Reader r(body.data(), body.size());
    std::uint16_t count = 0;
    if (!r.Read(count))
    {
        spdlog::warn("OnCtCmGiftChartUpdateReq[{}]: short body "
                     "({} bytes)", ip, body.size());
        co_return;
    }
    std::vector<Entry> entries;
    for (std::uint16_t i = 0; i < count; ++i)
    {
        Entry e{};
        if (!r.Read(e.type)) co_return;
        if (e.type == kCguAdd || e.type == kCguUpdate)
        {
            auto& g = e.gift;
            if (!r.Read(g.gift_id) || !r.Read(g.gift_type) ||
                !r.Read(g.value) || !r.Read(g.count) ||
                !r.Read(g.take_type) || !r.Read(g.max_take_count) ||
                !r.Read(g.tool_only) || !r.Read(g.err_gift_id) ||
                !r.ReadString(g.title) || !r.ReadString(g.msg))
            {
                spdlog::warn("OnCtCmGiftChartUpdateReq[{}]: truncated "
                             "entry {}/{}", ip, i, count);
                co_return;
            }
        }
        else if (e.type == kCguDel)
        {
            if (!r.Read(e.del_id)) co_return;
        }
        else
        {
            spdlog::warn("OnCtCmGiftChartUpdateReq[{}]: unknown entry "
                         "type {} — aborting parse", ip, e.type);
            co_return;
        }
        entries.push_back(std::move(e));
    }
    std::uint32_t manager = 0;
    if (!r.Read(manager))
    {
        spdlog::warn("OnCtCmGiftChartUpdateReq[{}]: missing manager id",
            ip);
        co_return;
    }

    // Persist each entry; failures are skipped, not aborted (legacy
    // per-entry `if(query->Call())`). ADD adopts the SP-assigned id.
    struct Applied
    {
        std::uint8_t  type = 0;
        CmGift        gift;
        std::uint16_t del_id = 0;
    };
    const auto applied = co_await fourstory::db::CoOffloadIf(
        ctx.db_pool,
        [repo = ctx.cmgift_repo, entries]() -> std::vector<Applied>
        {
            std::vector<Applied> out;
            for (const auto& e : entries)
            {
                if (e.type == kCguAdd)
                {
                    if (auto id = repo->Add(e.gift))
                    {
                        Applied a{};
                        a.type = kCguAdd;
                        a.gift = e.gift;
                        a.gift.gift_id = *id;
                        out.push_back(std::move(a));
                    }
                }
                else if (e.type == kCguUpdate)
                {
                    if (repo->Set(e.gift))
                        out.push_back({kCguUpdate, e.gift, 0});
                }
                else if (e.type == kCguDel)
                {
                    if (repo->Del(e.del_id))
                        out.push_back({kCguDel, {}, e.del_id});
                }
            }
            return out;
        });

    for (const auto& a : applied)
    {
        if (a.type == kCguAdd)
            ctx.cmgifts->Add(a.gift);
        else if (a.type == kCguUpdate)
            ctx.cmgifts->Update(a.gift);
        else
            ctx.cmgifts->Erase(a.del_id);
    }

    // Legacy replies with the refreshed full catalogue.
    if (ctx.ctrl_svr)
    {
        if (auto cs = ctx.ctrl_svr->Get())
            co_await senders::SendCtCmGiftListAck(cs, manager,
                ctx.cmgifts->Snapshot());
    }
    co_return;
}

} // namespace tworldsvr::handlers
