// W6-50 — tournament operator tools + state core.
//
// Legacy flow (SSHandler.cpp:12072-12802): CT_TOURNAMENTEVENT_REQ ops
// hop between the batch thread (catalogue / current / players) and
// the timer thread (schedules / times) via SM_TOURNAMENTEVENT_REQ/
// _ACK self-posts, and reach the DB thread via DM_TNMTEVENT* /
// DM_TOURNAMENTEVENTCHARINFO / DM_TOURNAMENTAPPLY round-trips. The
// asio port runs everything on the io_context strand: the SM legs
// become direct calls (their wire handlers stay dispatchable for
// parity), the DM legs collapse into ITournamentRepository.
//
// Deferred to later slices: the SM_TOURNAMENT_REQ step advance (the
// scheduler tick), TET_PLAYEREND's TournamentSelectPlayer /
// TournamentMatch triggers, TournamentInfo's step>=MATCH match
// fan-out, the GetRanking / IsFirstGroup fills, and the
// TVIEW_TOURNAMENTPLAYER boot reload.

#include "handlers.h"

#include "../senders/senders.h"
#include "../services/tournament_constants.h"
#include "../wire_codec.h"

#include "fourstory/db/co_offload.h"
#include "MessageId.h"

#include <boost/asio/awaitable.hpp>
#include <spdlog/spdlog.h>

#include <ctime>
#include <utility>

namespace tworldsvr::handlers {

namespace {

namespace tnmt = tworldsvr::tournament;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

// ---- shared wire codecs -------------------------------------------

// Entry tail as TET_ENTRYADD carries it / TET_LIST echoes it
// (SSHandler.cpp:12109 / 12470): everything after the entry id.
bool ReadEntryTail(wire::Reader& r, TournamentEntrySeed& e)
{
    std::uint8_t reward_count = 0;
    if (!r.ReadString(e.name) || !r.Read(e.type) || !r.Read(e.cls) ||
        !r.Read(e.fee) || !r.Read(e.fee_back) ||
        !r.Read(e.permit_item_id) || !r.Read(e.permit_count) ||
        !r.Read(e.min_level) || !r.Read(e.max_level) ||
        !r.Read(reward_count))
        return false;
    for (std::uint8_t j = 0; j < reward_count; ++j)
    {
        TnmtReward w{};
        if (!r.Read(w.chart_type) || !r.Read(w.item_id) ||
            !r.Read(w.count) || !r.Read(w.cls) ||
            !r.Read(w.check_shield))
            return false;
        e.rewards.push_back(w);
    }
    return true;
}

void WriteEntryTail(std::vector<std::byte>& out,
                    const TournamentEntrySeed& e)
{
    wire::WriteString(out, e.name);
    wire::WritePOD<std::uint8_t>(out, e.type);
    wire::WritePOD<std::uint32_t>(out, e.cls);
    wire::WritePOD<std::uint32_t>(out, e.fee);
    wire::WritePOD<std::uint32_t>(out, e.fee_back);
    wire::WritePOD<std::uint16_t>(out, e.permit_item_id);
    wire::WritePOD<std::uint8_t>(out, e.permit_count);
    wire::WritePOD<std::uint8_t>(out, e.min_level);
    wire::WritePOD<std::uint8_t>(out, e.max_level);
    wire::WritePOD<std::uint8_t>(
        out, static_cast<std::uint8_t>(e.rewards.size()));
    for (const auto& w : e.rewards)
    {
        wire::WritePOD<std::uint8_t>(out, w.chart_type);
        wire::WritePOD<std::uint16_t>(out, w.item_id);
        wire::WritePOD<std::uint8_t>(out, w.count);
        wire::WritePOD<std::uint32_t>(out, w.cls);
        wire::WritePOD<std::uint8_t>(out, w.check_shield);
    }
}

// ---- ctrl-svr echo -------------------------------------------------

boost::asio::awaitable<void>
SendCtrlAck(const HandlerContext& ctx, std::vector<std::byte> body)
{
    if (!ctx.ctrl_svr)
        co_return;
    if (auto cs = ctx.ctrl_svr->Get())
        co_await cs->Wire()->SendPacket(
            ToUint16(MessageId::CT_TOURNAMENTEVENT_ACK),
            std::move(body));
    else
        spdlog::info("tournament: ctrl-svr offline - "
                     "CT_TOURNAMENTEVENT_ACK dropped");
    co_return;
}

// ---- TournamentInfo (TWorldSvr.cpp:6898) ---------------------------

boost::asio::awaitable<void>
BroadcastTournamentInfo(const HandlerContext&        ctx,
                        std::shared_ptr<PeerSession> only)
{
    if (!ctx.tournaments)
        co_return;
    const auto info = ctx.tournaments->Info();
    if (only)
        co_await senders::SendMwTournamentInfoReq(only, info);
    else if (ctx.peers)
        for (auto& p : ctx.peers->Snapshot())
            co_await senders::SendMwTournamentInfoReq(p, info);
    // Legacy also re-fires TournamentMatch() when step >= MATCH —
    // the match engine is a later slice.
    if (info.step >= tnmt::kStepMatch)
        spdlog::debug("tournament: TournamentMatch() fan-out "
                      "deferred (step={})", info.step);
    co_return;
}

// ---- TournamentUpdate (TWorldSvr.cpp:6934) -------------------------
//
// Election + the collapsed SM_TOURNAMENTUPDATE_REQ rebuild + info
// broadcast + the DM_TOURNAMENTCLEAR persist.
boost::asio::awaitable<void>
RunTournamentElection(const HandlerContext& ctx,
                      std::uint16_t         updated_tour_id)
{
    if (!ctx.tournaments)
        co_return;
    const auto res = ctx.tournaments->ElectNextSchedule(updated_tour_id);
    if (!res.changed)
        co_return;
    if (ctx.tournament_repo)
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.tournament_repo] { repo->ClearPersisted(); });
    ctx.tournaments->RebuildCurrent(res.id, res.steps);
    co_await BroadcastTournamentInfo(ctx, nullptr);
    co_return;
}

// Eviction fan-out shared by SCHEDULEADD / boot-shaped paths: the
// legacy SetTournamentTime posts SM_TOURNAMENTEVENT_ACK
// (TET_SCHEDULEDEL, manager 0) and runs DelTournamentSchedule —
// catalogue drop + re-election + TTnmtEventDel persist, no ctrl echo.
boost::asio::awaitable<void>
HandleEvictions(const HandlerContext&             ctx,
                const std::vector<std::uint16_t>& evicted)
{
    for (const std::uint16_t id : evicted)
    {
        ctx.tournaments->DropTournament(id);
        co_await RunTournamentElection(ctx, id);
        if (ctx.tournament_repo)
            co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                [repo = ctx.tournament_repo, id]
                { repo->DeleteEvent(id); });
        spdlog::info("tournament: schedule {} evicted", id);
    }
    co_return;
}

// ---- SM_TOURNAMENTEVENT_ACK logic (SSHandler.cpp:12442) ------------
//
// Takes the ACK payload (mgid/type prefix included) and runs the
// batch-thread side effects. `payload` is consumed.
boost::asio::awaitable<void>
SmTournamentEventAckLogic(const HandlerContext&  ctx,
                          std::vector<std::byte> payload)
{
    wire::Reader r(payload.data(), payload.size());
    std::uint32_t mgid = 0;
    std::uint8_t  type = 0;
    if (!r.Read(mgid) || !r.Read(type))
    {
        spdlog::warn("SmTournamentEventAck: short payload ({} bytes)",
            payload.size());
        co_return;
    }

    switch (type)
    {
    case tnmt::kTetList:
    {
        // Echo the schedule listing + append the catalogue
        // (legacy Copy + append, SSHandler.cpp:12452).
        std::vector<std::byte> out = payload;
        const auto rows = ctx.tournaments->CatalogueList();
        wire::WritePOD<std::uint8_t>(
            out, static_cast<std::uint8_t>(rows.size()));
        for (const auto& row : rows)
        {
            wire::WritePOD<std::uint16_t>(out, row.id);
            wire::WritePOD<std::uint8_t>(
                out, static_cast<std::uint8_t>(row.entries.size()));
            for (const auto& er : row.entries)
            {
                wire::WritePOD<std::uint8_t>(out, er.seed.entry_id);
                WriteEntryTail(out, er.seed);
                wire::WritePOD<std::uint8_t>(
                    out,
                    static_cast<std::uint8_t>(er.players.size()));
                for (const auto& p : er.players)
                {
                    wire::WritePOD<std::uint32_t>(out, p.char_id);
                    wire::WriteString(out, p.name);
                    wire::WritePOD<std::uint8_t>(out, p.level);
                    wire::WritePOD<std::uint8_t>(out, p.cls);
                    wire::WritePOD<std::uint8_t>(out, p.country);
                }
            }
        }
        co_await SendCtrlAck(ctx, std::move(out));
        break;
    }
    case tnmt::kTetScheduleAdd:
    case tnmt::kTetEntryAdd:
        // Verbatim echo (SSHandler.cpp:12545 / 12587).
        co_await SendCtrlAck(ctx, std::move(payload));
        break;
    case tnmt::kTetScheduleDel:
    {
        std::uint16_t tid = 0;
        if (!r.Read(tid))
            co_return;
        ctx.tournaments->DropTournament(tid);
        if (mgid)
            co_await SendCtrlAck(ctx, std::move(payload));
        break;
    }
    case tnmt::kTetPlayerAdd:
    {
        // The DM_TOURNAMENTEVENTCHARINFO fan-in (SSHandler.cpp:12602).
        std::uint16_t tid      = 0;
        std::uint8_t  entry_id = 0;
        std::uint32_t char_id  = 0;
        if (!r.Read(tid) || !r.Read(entry_id) || !r.Read(char_id))
            co_return;

        TnmtPlayerBrief info{};
        info.country = tnmt::kCountryN;
        info.cls     = tnmt::kClassCount;
        if (char_id)
        {
            info.char_id = char_id;
            if (!r.Read(info.country) || !r.ReadString(info.name) ||
                !r.Read(info.level) || !r.Read(info.cls))
                co_return;

            // Guild-name fill (legacy m_mapCharGuild + FindTGuild).
            std::string guild_name;
            if (ctx.chars && ctx.guilds)
                if (auto c = ctx.chars->Find(char_id))
                {
                    std::uint32_t gid = 0;
                    {
                        std::lock_guard g(c->lock);
                        gid = c->guild_id;
                    }
                    if (gid)
                        if (auto guild = ctx.guilds->Find(gid))
                        {
                            std::lock_guard g(guild->lock);
                            guild_name = guild->name;
                        }
                }

            if (ctx.tournaments->AddPlayer1st(entry_id, info,
                                              guild_name))
            {
                if (ctx.tournament_repo)
                {
                    TnmtApplyOp op{};
                    op.add      = true;
                    op.char_id  = char_id;
                    op.entry_id = entry_id;
                    op.chief_id = char_id;
                    co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                        [repo = ctx.tournament_repo, op]
                        { repo->ApplyPlayer(op); });
                }
            }
            else
            {
                // Entry vanished / already registered — echo zeros
                // (legacy `else dwCharID = 0` keeps the init values).
                char_id = 0;
                info    = TnmtPlayerBrief{};
                info.country = tnmt::kCountryN;
                info.cls     = tnmt::kClassCount;
            }
        }

        std::vector<std::byte> out;
        wire::WritePOD<std::uint32_t>(out, mgid);
        wire::WritePOD<std::uint8_t>(out, type);
        wire::WritePOD<std::uint8_t>(out, entry_id);
        wire::WritePOD<std::uint32_t>(out, char_id);
        wire::WriteString(out, info.name);
        wire::WritePOD<std::uint8_t>(out, info.level);
        wire::WritePOD<std::uint8_t>(out, info.cls);
        wire::WritePOD<std::uint8_t>(out, info.country);
        co_await SendCtrlAck(ctx, std::move(out));
        break;
    }
    case tnmt::kTetEntryDel:
    default:
        break;
    }
    co_return;
}

// ---- SM_TOURNAMENTEVENT_REQ logic (SSHandler.cpp:12292) ------------

boost::asio::awaitable<void>
SmTournamentEventReqLogic(const HandlerContext&         ctx,
                          const std::vector<std::byte>& payload,
                          std::int64_t                  now)
{
    wire::Reader r(payload.data(), payload.size());
    std::uint32_t mgid = 0;
    std::uint8_t  type = 0;
    if (!r.Read(mgid) || !r.Read(type))
    {
        spdlog::warn("SmTournamentEventReq: short payload ({} bytes)",
            payload.size());
        co_return;
    }

    switch (type)
    {
    case tnmt::kTetList:
    {
        std::vector<std::byte> ack;
        wire::WritePOD<std::uint32_t>(ack, mgid);
        wire::WritePOD<std::uint8_t>(ack, type);
        const auto rows = ctx.tournaments->ScheduleList();
        wire::WritePOD<std::uint8_t>(
            ack, static_cast<std::uint8_t>(rows.size()));
        for (const auto& row : rows)
        {
            wire::WritePOD<std::uint16_t>(ack, row.id);
            wire::WritePOD<std::uint8_t>(ack, row.time.week);
            wire::WritePOD<std::uint8_t>(ack, row.time.day);
            wire::WritePOD<std::uint32_t>(ack, row.time.start_sec);
            wire::WritePOD<std::uint8_t>(
                ack, static_cast<std::uint8_t>(row.steps.size()));
            for (const auto& s : row.steps)
            {
                wire::WritePOD<std::uint8_t>(ack, s.step);
                wire::WritePOD<std::uint32_t>(ack, s.period);
                wire::WritePOD<std::int64_t>(ack, s.start);
            }
        }
        co_await SmTournamentEventAckLogic(ctx, std::move(ack));
        break;
    }
    case tnmt::kTetScheduleAdd:
    {
        std::uint16_t tid = 0;
        TournamentBattleTime bt{};
        std::uint8_t count = 0;
        if (!r.Read(tid) || !r.Read(bt.week) || !r.Read(bt.day) ||
            !r.Read(bt.start_sec) || !r.Read(count))
            co_return;

        TournamentRegistry::StepMap steps;
        for (std::uint8_t i = 0; i < count; ++i)
        {
            TournamentStep s{};
            if (!r.Read(s.step) || !r.Read(s.period))
                co_return;
            steps.emplace(TournamentRegistry::StepKey(s.step, 0), s);
        }

        // Legacy replaces the battle time up front for known ids
        // (SSHandler.cpp:12363).
        if (ctx.tournaments->HasSchedule(tid))
            ctx.tournaments->ReplaceTime(tid, bt);

        const auto res = ctx.tournaments->SetTournamentTime(
            std::move(steps), bt, tid, /*month_base=*/false,
            /*enable=*/false, now);
        co_await HandleEvictions(ctx, res.evicted);

        // Legacy: re-elect only when the ADD targeted an existing id.
        if (tid)
            co_await RunTournamentElection(ctx, res.id);

        std::vector<std::byte> ack;
        wire::WritePOD<std::uint32_t>(ack, mgid);
        wire::WritePOD<std::uint8_t>(ack, type);
        wire::WritePOD<std::uint16_t>(ack, res.id);
        co_await SmTournamentEventAckLogic(ctx, std::move(ack));

        if (res.id && ctx.tournament_repo)
        {
            // Collapsed DM_TNMTEVENTSCHEDULEADD (SSHandler.cpp:12679).
            std::vector<TnmtEventStepRow> rows;
            for (const auto& srow :
                 ctx.tournaments->ScheduleList())
                if (srow.id == res.id)
                    for (const auto& s : srow.steps)
                        rows.push_back(
                            TnmtEventStepRow{s.step, s.period});
            co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                [repo = ctx.tournament_repo, id = res.id, bt, rows]
                { repo->SaveEventSchedule(id, bt, rows); });
        }
        break;
    }
    case tnmt::kTetScheduleDel:
    {
        std::uint16_t tid = 0;
        if (!r.Read(tid))
            co_return;

        // DelTournamentSchedule (TWorldSvr.cpp:5866): erase +
        // re-elect + persist delete.
        ctx.tournaments->DeleteSchedule(tid);
        co_await RunTournamentElection(ctx, tid);
        if (ctx.tournament_repo)
            co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                [repo = ctx.tournament_repo, tid]
                { repo->DeleteEvent(tid); });

        std::vector<std::byte> ack;
        wire::WritePOD<std::uint32_t>(ack, mgid);
        wire::WritePOD<std::uint8_t>(ack, type);
        wire::WritePOD<std::uint16_t>(ack, tid);
        co_await SmTournamentEventAckLogic(ctx, std::move(ack));
        break;
    }
    case tnmt::kTetEntryAdd:
    {
        std::uint16_t tid = 0;
        if (!r.Read(tid))
            co_return;
        if (ctx.tournaments->HasSchedule(tid))
            co_await RunTournamentElection(ctx, tid);

        std::vector<std::byte> ack;
        wire::WritePOD<std::uint32_t>(ack, mgid);
        wire::WritePOD<std::uint8_t>(ack, type);
        co_await SmTournamentEventAckLogic(ctx, std::move(ack));
        break;
    }
    default:
        break;
    }
    co_return;
}

} // namespace

// ---- CT_TOURNAMENTEVENT_REQ (SSHandler.cpp:12072) ------------------

boost::asio::awaitable<void>
OnCtTournamentEventReq(std::shared_ptr<PeerSession> peer,
                       std::vector<std::byte>       body,
                       const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.tournaments)
    {
        spdlog::warn("OnCtTournamentEventReq[{}]: tournaments not "
                     "wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t mgid = 0;
    std::uint8_t  type = 0;
    if (!r.Read(mgid) || !r.Read(type))
    {
        spdlog::warn("OnCtTournamentEventReq[{}]: short body "
                     "({} bytes)", ip, body.size());
        co_return;
    }

    switch (type)
    {
    case tnmt::kTetList:
    case tnmt::kTetScheduleAdd:
    case tnmt::kTetScheduleDel:
        // Legacy SayToTIMER copy — direct call on the strand.
        co_await SmTournamentEventReqLogic(ctx, body,
            static_cast<std::int64_t>(std::time(nullptr)));
        break;
    case tnmt::kTetEntryAdd:
    {
        std::uint16_t tid   = 0;
        std::uint8_t  count = 0;
        if (!r.Read(tid) || !r.Read(count))
            co_return;

        std::vector<TournamentEntrySeed> entries;
        for (std::uint8_t i = 0; i < count; ++i)
        {
            TournamentEntrySeed e{};
            if (!r.Read(e.entry_id) || !ReadEntryTail(r, e))
            {
                spdlog::warn("OnCtTournamentEventReq[{}]: malformed "
                             "TET_ENTRYADD ({} bytes)", ip,
                    body.size());
                co_return;
            }
            entries.push_back(std::move(e));
        }

        // Replace the catalogue row; registered players of the old
        // entries are unregistered + persist-removed
        // (TNMTEntryDelete's DM_TOURNAMENTAPPLY fan-out).
        const auto removed =
            ctx.tournaments->ReplaceEntries(tid, entries);
        if (ctx.tournament_repo)
            for (const std::uint32_t char_id : removed)
            {
                TnmtApplyOp op{};
                op.add     = false;
                op.char_id = char_id;
                co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                    [repo = ctx.tournament_repo, op]
                    { repo->ApplyPlayer(op); });
            }

        // SM leg: re-elect when scheduled + the ctrl echo.
        {
            std::vector<std::byte> sm;
            wire::WritePOD<std::uint32_t>(sm, mgid);
            wire::WritePOD<std::uint8_t>(sm, type);
            wire::WritePOD<std::uint16_t>(sm, tid);
            co_await SmTournamentEventReqLogic(ctx, sm,
                static_cast<std::int64_t>(std::time(nullptr)));
        }

        // Collapsed DM_TNMTEVENTENTRYADD persist
        // (SSHandler.cpp:12735).
        if (ctx.tournament_repo)
            co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                [repo = ctx.tournament_repo, tid, entries]
                { repo->SaveEventEntries(tid, entries); });
        break;
    }
    case tnmt::kTetEntryDel:
    {
        std::uint16_t tid      = 0;
        std::uint8_t  entry_id = 0;
        if (!r.Read(tid) || !r.Read(entry_id))
            co_return;
        const auto removed =
            ctx.tournaments->DeleteEntry(tid, entry_id);
        if (ctx.tournament_repo)
            for (const std::uint32_t char_id : removed)
            {
                TnmtApplyOp op{};
                op.add     = false;
                op.char_id = char_id;
                co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                    [repo = ctx.tournament_repo, op]
                    { repo->ApplyPlayer(op); });
            }
        break;                       // legacy: no echo
    }
    case tnmt::kTetPlayerAdd:
    {
        std::uint16_t tid      = 0;
        std::uint8_t  entry_id = 0;
        std::string   target;
        if (!r.Read(tid) || !r.Read(entry_id) ||
            !r.ReadString(target))
            co_return;

        // Guards (SSHandler.cpp:12185): wrong tournament, past the
        // registration window, or unknown entry → zeroed ack.
        const bool error =
            tid != ctx.tournaments->CurrentId() ||
            ctx.tournaments->CurrentStep() >= tnmt::kStepEnter ||
            !ctx.tournaments->CurrentEntryExists(entry_id);
        if (error)
        {
            std::vector<std::byte> ack;
            wire::WritePOD<std::uint32_t>(ack, mgid);
            wire::WritePOD<std::uint8_t>(ack, type);
            wire::WritePOD<std::uint8_t>(ack, entry_id);
            wire::WritePOD<std::uint32_t>(ack, 0);
            wire::WriteString(ack, std::string{});
            wire::WritePOD<std::uint8_t>(ack, 0);
            wire::WritePOD<std::uint8_t>(ack, tnmt::kClassCount);
            wire::WritePOD<std::uint8_t>(ack, tnmt::kCountryN);
            co_await SendCtrlAck(ctx, std::move(ack));
            co_return;
        }

        if (!ctx.tournament_repo)
        {
            spdlog::warn("OnCtTournamentEventReq[{}]: tournament_repo "
                         "not wired - TET_PLAYERADD dropped (no DB)",
                ip);
            co_return;
        }

        // Collapsed DM_TOURNAMENTEVENTCHARINFO (SSHandler.cpp:11731):
        // name lookup; >1 hits → drop without a reply (legacy
        // deletes the message), 0 hits → char_id 0 fan-in.
        const auto rows = co_await fourstory::db::CoOffloadIf(
            ctx.db_pool,
            [repo = ctx.tournament_repo, target]
            { return repo->FindCharInfoByName(target); });
        if (rows.size() > 1)
            co_return;

        std::vector<std::byte> ack;
        wire::WritePOD<std::uint32_t>(ack, mgid);
        wire::WritePOD<std::uint8_t>(ack, type);
        wire::WritePOD<std::uint16_t>(ack, tid);
        wire::WritePOD<std::uint8_t>(ack, entry_id);
        if (rows.empty())
            wire::WritePOD<std::uint32_t>(ack, 0);
        else
        {
            const auto& c = rows.front();
            wire::WritePOD<std::uint32_t>(ack, c.char_id);
            wire::WritePOD<std::uint8_t>(ack, c.country);
            wire::WriteString(ack, c.name);
            wire::WritePOD<std::uint8_t>(ack, c.level);
            wire::WritePOD<std::uint8_t>(ack, c.cls);
        }
        co_await SmTournamentEventAckLogic(ctx, std::move(ack));
        break;
    }
    case tnmt::kTetPlayerDel:
    {
        std::uint16_t tid      = 0;
        std::uint8_t  entry_id = 0;
        std::string   target;
        if (!r.Read(tid) || !r.Read(entry_id) ||
            !r.ReadString(target))
            co_return;

        std::uint32_t char_id = 0;
        if (const auto removed = ctx.tournaments->RemovePlayerByName(
                tid, entry_id, target))
        {
            char_id = *removed;
            if (ctx.tournament_repo)
            {
                TnmtApplyOp op{};
                op.add     = false;
                op.char_id = char_id;
                co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                    [repo = ctx.tournament_repo, op]
                    { repo->ApplyPlayer(op); });
            }
        }

        std::vector<std::byte> ack;
        wire::WritePOD<std::uint32_t>(ack, mgid);
        wire::WritePOD<std::uint8_t>(ack, type);
        wire::WritePOD<std::uint8_t>(ack, entry_id);
        wire::WritePOD<std::uint32_t>(ack, char_id);
        co_await SendCtrlAck(ctx, std::move(ack));
        break;
    }
    case tnmt::kTetPlayerEnd:
    {
        // TournamentSelectPlayer / TournamentMatch triggers are the
        // match-engine slice; the ack ships now.
        if (ctx.tournaments->CurrentStep() >= tnmt::kStepParty)
            spdlog::info("OnCtTournamentEventReq[{}]: TET_PLAYEREND "
                         "select/match triggers deferred (step={})",
                ip, ctx.tournaments->CurrentStep());
        std::vector<std::byte> ack;
        wire::WritePOD<std::uint32_t>(ack, mgid);
        wire::WritePOD<std::uint8_t>(ack, type);
        co_await SendCtrlAck(ctx, std::move(ack));
        break;
    }
    default:
        break;
    }
    co_return;
}

// ---- SM_TOURNAMENT_REQ step advance (SSHandler.cpp:11406) ----------

namespace {

boost::asio::awaitable<void>
SmTournamentReqLogic(const HandlerContext& ctx, std::uint16_t id,
                     std::uint8_t group, std::uint8_t step,
                     std::uint32_t period)
{
    const auto adv = ctx.tournaments->AdvanceStep(id, group, step);
    if (!adv)
        co_return;              // catalogue guard — legacy silent drop

    if (ctx.peers)
        for (auto& p : ctx.peers->Snapshot())
            co_await senders::SendMwTournamentEnableReq(p, group,
                step, period, adv->next_step_start);

    // Legacy follow-ups (TournamentSelectPlayer when !selected at
    // PARTY/MATCH, TournamentMatch at MATCH) are the match-engine
    // slice.
    if (step == tnmt::kStepParty || step == tnmt::kStepMatch)
        spdlog::info("tournament: step {} reached - select/match "
                     "triggers deferred to the match-engine slice",
            step);
    co_return;
}

} // namespace

// ---- scheduler tick (TWorldSvr.cpp:4012) ---------------------------

boost::asio::awaitable<void>
RunTournamentTick(const HandlerContext& ctx, std::int64_t now)
{
    if (!ctx.tournaments)
        co_return;

    // The legacy walks once per second and consumes one due entry
    // per pass; a coarser sweep period drains the whole backlog in
    // order (every consumed entry zeroes its clock, so this always
    // terminates).
    for (;;)
    {
        using Kind = TournamentRegistry::DueAction::Kind;
        const auto due = ctx.tournaments->PopDueTick(now);
        if (due.kind == Kind::kNone)
            co_return;

        if (due.kind == Kind::kStep)
        {
            // SM_TOURNAMENT_REQ self-post + the DM_TOURNAMENTSTATUS
            // persist (SSHandler.cpp:4033-4042).
            co_await SmTournamentReqLogic(ctx, due.id, due.group,
                due.step, due.period);
            if (ctx.tournament_repo)
                co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                    [repo = ctx.tournament_repo, due]
                    { repo->SaveStatus(due.id, due.group, due.step); });
            continue;
        }

        // kReschedule (TWorldSvr.cpp:4047): the final group's END
        // elapsed — recompute next month's window (only when the
        // battle-time row still exists), then re-elect either way.
        if (const auto bt = ctx.tournaments->TimeFor(due.id))
        {
            auto steps = ctx.tournaments->StepsFor(due.id);
            if (!steps.empty())
            {
                const auto res = ctx.tournaments->SetTournamentTime(
                    std::move(steps), *bt, due.id,
                    /*month_base=*/true, /*enable=*/true, now);
                co_await HandleEvictions(ctx, res.evicted);
            }
        }
        co_await RunTournamentElection(ctx, due.id);
    }
}

// ---- MW_TOURNAMENT_ACK player vertical (SSHandler.cpp:11520) -------

namespace {

// Reply head shared by every MW_TOURNAMENT_REQ echo: the char
// identity + the sub-protocol id.
std::vector<std::byte> TnmtHead(std::uint32_t char_id,
                                std::uint32_t key,
                                std::uint16_t protocol)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint32_t>(body, char_id);
    wire::WritePOD<std::uint32_t>(body, key);
    wire::WritePOD<std::uint16_t>(body, protocol);
    return body;
}

// Short-form result echo (SSSender.cpp:3500).
boost::asio::awaitable<void>
SendTnmtResult(std::shared_ptr<PeerSession> peer,
               std::uint32_t char_id, std::uint32_t key,
               std::uint16_t protocol, std::uint8_t result)
{
    auto body = TnmtHead(char_id, key, protocol);
    wire::WritePOD<std::uint8_t>(body, result);
    co_await senders::SendMwTournamentReq(peer, std::move(body));
    co_return;
}

// Class-filtered reward block with the post-hoc count byte (legacy
// memcpy patch): {BYTE shield, BYTE chart, WORD item, BYTE count}.
void WriteFilteredRewards(std::vector<std::byte>&        out,
                          const std::vector<TnmtReward>& rewards,
                          std::uint8_t                   char_class)
{
    const std::size_t count_at = out.size();
    wire::WritePOD<std::uint8_t>(out, 0);
    std::uint8_t count = 0;
    for (const auto& w : rewards)
    {
        if (!(w.cls & (1u << char_class)))
            continue;
        wire::WritePOD<std::uint8_t>(out, w.check_shield);
        wire::WritePOD<std::uint8_t>(out, w.chart_type);
        wire::WritePOD<std::uint16_t>(out, w.item_id);
        wire::WritePOD<std::uint8_t>(out, w.count);
        ++count;
    }
    out[count_at] = std::byte{count};
}

void WriteRosterRow(std::vector<std::byte>& out,
                    const TournamentRegistry::PlayerRosterRow& r)
{
    wire::WritePOD<std::uint32_t>(out, r.char_id);
    wire::WritePOD<std::uint8_t>(out, r.country);
    wire::WriteString(out, r.name);
    wire::WritePOD<std::uint8_t>(out, r.level);
    wire::WritePOD<std::uint8_t>(out, r.cls);
    wire::WritePOD<std::uint32_t>(out, r.rank);
    wire::WritePOD<std::uint32_t>(out, r.month_rank);
}

// Live-char snapshot the switch works from.
struct TnmtCharView
{
    std::uint32_t char_id = 0;
    std::uint32_t key     = 0;
    std::uint8_t  country = 0;
    std::uint8_t  level   = 0;
    std::uint8_t  cls     = 0;
    std::string   name;
    std::string   guild_name;
};

std::string GuildNameOf(const HandlerContext& ctx,
                        std::uint32_t guild_id)
{
    if (!guild_id || !ctx.guilds)
        return {};
    auto g = ctx.guilds->Find(guild_id);
    if (!g)
        return {};
    std::lock_guard lk(g->lock);
    return g->name;
}

// IsFirstGroup / the TournamentApply first-grade scan
// (TWorldSvr.cpp:5645 / 6310): slots 1..first_group_count-1 of the
// fame first-grade array (MonthRankRegistry owns it in the port;
// empty until the first month rollover).
bool InFirstGradeGroup(const HandlerContext& ctx,
                       std::uint8_t country, std::uint32_t char_id)
{
    if (!ctx.month_rank || country > tnmt::kCountryB)
        return false;
    const auto  group = ctx.month_rank->FirstGradeGroup();
    const std::uint8_t count = ctx.tournaments->FirstGroupCount();
    for (std::uint8_t j = 1; j < count &&
         j < kFirstGradeGroupCount; ++j)
        if (group[country][j].char_id == char_id)
            return true;
    return false;
}

// TournamentApplyInfo (TWorldSvr.cpp:6379).
boost::asio::awaitable<void>
SendTnmtApplyInfo(const HandlerContext&        ctx,
                  std::shared_ptr<PeerSession> peer,
                  const TnmtCharView&          ch)
{
    const auto snap = ctx.tournaments->ApplyInfoReply(ch.char_id);
    if (!snap.ok)
        co_return;

    auto body = TnmtHead(ch.char_id, ch.key,
        ToUint16(MessageId::MW_TOURNAMENTAPPLYINFO_REQ));
    wire::WritePOD<std::uint8_t>(body,
        static_cast<std::uint8_t>(snap.entries.size()));
    for (const auto& e : snap.entries)
    {
        wire::WritePOD<std::uint8_t>(body, e.seed.group);
        wire::WritePOD<std::uint8_t>(body, e.seed.entry_id);
        wire::WriteString(body, e.seed.name);
        wire::WritePOD<std::uint8_t>(body, e.seed.type);
        wire::WritePOD<std::uint32_t>(body, e.seed.cls);
        wire::WritePOD<std::uint8_t>(body, e.applied ? 1 : 0);
        wire::WritePOD<std::uint32_t>(body, e.seed.fee);
        wire::WritePOD<std::uint32_t>(body, e.seed.fee_back);
        wire::WritePOD<std::uint8_t>(body, e.seed.permit_count);
        wire::WritePOD<std::uint8_t>(body, e.seed.min_level);
        wire::WritePOD<std::uint8_t>(body,
            e.seed.max_level == 0xFF ? snap.max_level
                                     : e.seed.max_level);
        wire::WritePOD<std::uint8_t>(body, e.free_first);
        wire::WritePOD<std::uint16_t>(body, e.normal_count);
        WriteFilteredRewards(body, e.seed.rewards, ch.cls);
        wire::WritePOD<std::uint8_t>(body,
            static_cast<std::uint8_t>(e.first_pool.size()));
        for (const auto& r : e.first_pool)
            WriteRosterRow(body, r);
    }
    co_await senders::SendMwTournamentReq(peer, std::move(body));
    co_return;
}

// TournamentPartyList (TWorldSvr.cpp:6658).
boost::asio::awaitable<void>
SendTnmtPartyList(const HandlerContext&        ctx,
                  std::shared_ptr<PeerSession> peer,
                  const TnmtCharView&          ch,
                  std::uint32_t                chief_id)
{
    const auto snap = ctx.tournaments->PartyListReply(chief_id);
    if (!snap.ok)
        co_return;

    auto body = TnmtHead(ch.char_id, ch.key,
        ToUint16(MessageId::MW_TOURNAMENTPARTYLIST_REQ));
    wire::WritePOD<std::uint32_t>(body, snap.chief_id);
    wire::WritePOD<std::uint8_t>(body,
        static_cast<std::uint8_t>(snap.members.size()));
    for (const auto& r : snap.members)
        WriteRosterRow(body, r);
    co_await senders::SendMwTournamentReq(peer, std::move(body));
    co_return;
}

// TournamentPartyAdd tail (TWorldSvr.cpp:6537) shared by the
// online-target and DB-looked-up paths.
boost::asio::awaitable<void>
RunTnmtPartyAdd(const HandlerContext&        ctx,
                std::shared_ptr<PeerSession> peer,
                const TnmtCharView&          ch,
                const TnmtPlayerBrief&       target,
                const std::string&           target_guild)
{
    const auto out = ctx.tournaments->TryPartyAdd(ch.char_id,
        ch.country, target, target_guild);
    if (out.silent)
        co_return;

    const std::uint16_t proto =
        ToUint16(MessageId::MW_TOURNAMENTPARTYADD_REQ);
    if (out.added)
    {
        if (ctx.tournament_repo)
        {
            TnmtApplyOp op{};
            op.add      = true;
            op.char_id  = target.char_id;
            op.entry_id = out.entry_id;
            op.chief_id = out.chief_id;
            co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                [repo = ctx.tournament_repo, op]
                { repo->ApplyPlayer(op); });
        }
        auto body = TnmtHead(ch.char_id, ch.key, proto);
        wire::WritePOD<std::uint8_t>(body,
            tnmt::kResultSuccess);
        wire::WriteString(body, target.name);
        wire::WritePOD<std::uint32_t>(body, target.char_id);
        co_await senders::SendMwTournamentReq(peer, std::move(body));
        co_await SendTnmtPartyList(ctx, peer, ch, ch.char_id);
        co_return;
    }
    if (out.result == tnmt::kResultLevel)
    {
        // Level-band refusal carries the target's name
        // (TWorldSvr.cpp:6579).
        auto body = TnmtHead(ch.char_id, ch.key, proto);
        wire::WritePOD<std::uint8_t>(body, tnmt::kResultLevel);
        wire::WriteString(body, target.name);
        co_await senders::SendMwTournamentReq(peer, std::move(body));
        co_return;
    }
    co_await SendTnmtResult(peer, ch.char_id, ch.key, proto,
        out.result);
    co_return;
}

} // namespace

boost::asio::awaitable<void>
OnMwTournamentAck(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.tournaments || !ctx.chars)
    {
        spdlog::warn("OnMwTournamentAck[{}]: tournaments/chars not "
                     "wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id  = 0;
    std::uint32_t key      = 0;
    std::uint16_t protocol = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(protocol))
    {
        spdlog::warn("OnMwTournamentAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    // FindTChar(id, key) — mismatch is a silent drop.
    auto c = ctx.chars->Find(char_id);
    if (!c)
        co_return;
    TnmtCharView ch{};
    std::uint32_t guild_id = 0;
    {
        std::lock_guard lk(c->lock);
        if (c->key != key)
            co_return;
        ch.char_id = char_id;
        ch.key     = key;
        ch.country = c->country;
        ch.level   = c->level;
        ch.cls     = c->klass;
        ch.name    = c->name;
        guild_id   = c->guild_id;
    }
    ch.guild_name = GuildNameOf(ctx, guild_id);

    switch (protocol)
    {
    case ToUint16(MessageId::MW_TOURNAMENTSCHEDULE_ACK):
    {
        // TournamentSchedule (TWorldSvr.cpp:7002).
        const auto snap = ctx.tournaments->ScheduleReply();
        if (!snap.ok)
            co_return;
        auto out = TnmtHead(ch.char_id, ch.key,
            ToUint16(MessageId::MW_TOURNAMENTSCHEDULE_REQ));
        wire::WritePOD<std::uint8_t>(out, snap.group);
        wire::WritePOD<std::uint8_t>(out, snap.step);
        const std::size_t count_at = out.size();
        wire::WritePOD<std::uint8_t>(out, 0);
        std::uint8_t count = 0;
        for (const auto& s : snap.steps)
        {
            if (!s.period)
                continue;
            wire::WritePOD<std::uint8_t>(out, s.group);
            wire::WritePOD<std::uint8_t>(out, s.step);
            wire::WritePOD<std::int64_t>(out, s.start);
            ++count;
        }
        out[count_at] = std::byte{count};
        co_await senders::SendMwTournamentReq(peer, std::move(out));
        break;
    }
    case ToUint16(MessageId::MW_TOURNAMENTAPPLYINFO_ACK):
        co_await SendTnmtApplyInfo(ctx, peer, ch);
        break;
    case ToUint16(MessageId::MW_TOURNAMENTAPPLY_ACK):
    {
        // TournamentApply (TWorldSvr.cpp:6290).
        std::uint8_t  entry_id = 0;
        std::string   hwid;
        std::uint32_t ip_addr = 0;
        if (!r.Read(entry_id) || !r.ReadString(hwid) ||
            !r.Read(ip_addr))
            co_return;

        TnmtPlayerBrief info{ch.char_id, ch.country, ch.name,
            ch.level, ch.cls};
        const auto out = ctx.tournaments->TryApply(info,
            ch.guild_name, entry_id, hwid, ip_addr,
            InFirstGradeGroup(ctx, ch.country, ch.char_id));

        const std::uint16_t proto =
            ToUint16(MessageId::MW_TOURNAMENTAPPLY_REQ);
        if (!out.added)
        {
            // Errors and the already-registered SUCCESS quirk both
            // take the short form.
            co_await SendTnmtResult(peer, ch.char_id, ch.key, proto,
                out.result);
            co_return;
        }
        if (ctx.tournament_repo)
        {
            TnmtApplyOp op{};
            op.add      = true;
            op.char_id  = ch.char_id;
            op.entry_id = entry_id;
            op.chief_id = ch.char_id;
            op.hwid     = hwid;
            op.ip_addr  = ip_addr;
            co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                [repo = ctx.tournament_repo, op]
                { repo->ApplyPlayer(op); });
        }
        auto reply = TnmtHead(ch.char_id, ch.key, proto);
        wire::WritePOD<std::uint8_t>(reply, tnmt::kResultSuccess);
        wire::WritePOD<std::uint8_t>(reply, entry_id);
        co_await senders::SendMwTournamentReq(peer, std::move(reply));
        co_await SendTnmtApplyInfo(ctx, peer, ch);
        break;
    }
    case ToUint16(MessageId::MW_TOURNAMENTJOINLIST_ACK):
    {
        // TournamentJoinList (TWorldSvr.cpp:6461).
        const auto snap = ctx.tournaments->JoinListReply(ch.char_id);
        if (!snap.ok)
            co_return;
        auto out = TnmtHead(ch.char_id, ch.key,
            ToUint16(MessageId::MW_TOURNAMENTJOINLIST_REQ));
        wire::WritePOD<std::uint8_t>(out,
            static_cast<std::uint8_t>(snap.entries.size()));
        for (const auto& e : snap.entries)
        {
            wire::WritePOD<std::uint8_t>(out, e.seed.group);
            wire::WritePOD<std::uint8_t>(out, e.seed.entry_id);
            wire::WriteString(out, e.seed.name);
            wire::WritePOD<std::uint8_t>(out, e.seed.type);
            wire::WritePOD<std::uint32_t>(out, e.seed.cls);
            wire::WritePOD<std::uint8_t>(out, e.applied ? 1 : 0);
            WriteFilteredRewards(out, e.seed.rewards, ch.cls);
            wire::WritePOD<std::uint8_t>(out,
                static_cast<std::uint8_t>(e.players.size()));
            for (const auto& p : e.players)
                WriteRosterRow(out, p);
        }
        co_await senders::SendMwTournamentReq(peer, std::move(out));
        break;
    }
    case ToUint16(MessageId::MW_TOURNAMENTPARTYLIST_ACK):
    {
        std::uint32_t chief_id = 0;
        if (!r.Read(chief_id))
            co_return;
        co_await SendTnmtPartyList(ctx, peer, ch, chief_id);
        break;
    }
    case ToUint16(MessageId::MW_TOURNAMENTPARTYADD_ACK):
    {
        // Online target → live TChar fields; offline → the collapsed
        // DM_GETCHARINFO lookup (SSHandler.cpp:11575 + 11689).
        std::string target;
        if (!r.ReadString(target))
            co_return;

        if (auto t = ctx.chars->FindByName(target))
        {
            TnmtPlayerBrief brief{};
            std::uint32_t t_guild = 0;
            {
                std::lock_guard lk(t->lock);
                brief.char_id = t->char_id;
                brief.country = t->country;
                brief.name    = t->name;
                brief.level   = t->level;
                brief.cls     = t->klass;
                t_guild       = t->guild_id;
            }
            co_await RunTnmtPartyAdd(ctx, peer, ch, brief,
                GuildNameOf(ctx, t_guild));
            co_return;
        }

        if (!ctx.tournament_repo)
        {
            spdlog::warn("OnMwTournamentAck[{}]: tournament_repo "
                         "not wired - PARTYADD lookup dropped", ip);
            co_return;
        }
        const auto rows = co_await fourstory::db::CoOffloadIf(
            ctx.db_pool,
            [repo = ctx.tournament_repo, target]
            { return repo->FindCharInfoByName(target); });
        if (rows.size() > 1)
            co_return;              // legacy multi-hit silent drop
        TnmtPlayerBrief brief{};    // 0-hit → char_id 0 → NOTFOUND
        if (!rows.empty())
            brief = rows.front();
        co_await RunTnmtPartyAdd(ctx, peer, ch, brief,
            /*target_guild=*/{});
        break;
    }
    case ToUint16(MessageId::MW_TOURNAMENTPARTYDEL_ACK):
    {
        // TournamentPartyDel (TWorldSvr.cpp:6634).
        std::uint32_t target_id = 0;
        if (!r.Read(target_id))
            co_return;
        const auto out = ctx.tournaments->PartyDel(ch.char_id,
            target_id);
        if (!out.removed)
            co_return;
        if (ctx.tournament_repo)
        {
            TnmtApplyOp op{};
            op.add     = false;
            op.char_id = target_id;
            co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                [repo = ctx.tournament_repo, op]
                { repo->ApplyPlayer(op); });
        }
        co_await SendTnmtPartyList(ctx, peer, ch, out.chief_id);
        break;
    }
    case ToUint16(MessageId::MW_TOURNAMENTMATCHLIST_ACK):
    {
        // TournamentMatchList (TWorldSvr.cpp:6822).
        const auto snap = ctx.tournaments->MatchListReply(ch.char_id);
        if (!snap.ok)
            co_return;
        auto out = TnmtHead(ch.char_id, ch.key,
            ToUint16(MessageId::MW_TOURNAMENTMATCHLIST_REQ));
        wire::WritePOD<std::uint8_t>(out,
            static_cast<std::uint8_t>(snap.entries.size()));
        for (const auto& e : snap.entries)
        {
            wire::WritePOD<std::uint8_t>(out, e.seed.group);
            wire::WritePOD<std::uint8_t>(out, e.seed.entry_id);
            wire::WriteString(out, e.seed.name);
            wire::WritePOD<std::uint8_t>(out, e.seed.type);
            wire::WritePOD<std::uint32_t>(out, e.seed.cls);
            wire::WritePOD<std::uint8_t>(out, e.applied ? 1 : 0);
            WriteFilteredRewards(out, e.seed.rewards, ch.cls);
            wire::WritePOD<std::uint8_t>(out,
                static_cast<std::uint8_t>(e.players.size()));
            for (const auto& m : e.players)
            {
                wire::WritePOD<std::uint8_t>(out, m.slot_id);
                WriteRosterRow(out, m.row);
                wire::WritePOD<std::uint8_t>(out, m.result[0]);
                wire::WritePOD<std::uint8_t>(out, m.result[1]);
                wire::WritePOD<std::uint8_t>(out, m.result[2]);
            }
        }
        co_await senders::SendMwTournamentReq(peer, std::move(out));
        break;
    }
    case ToUint16(MessageId::MW_TOURNAMENTEVENTLIST_ACK):
    case ToUint16(MessageId::MW_TOURNAMENTEVENTINFO_ACK):
    case ToUint16(MessageId::MW_TOURNAMENTEVENTJOIN_ACK):
        // The spectator-betting trio needs the ticket/batting model
        // (TNMTEnterGate + GetBattingAmount) — the match/betting
        // slice owns it.
        spdlog::info("OnMwTournamentAck[{}]: betting op {:#06x} "
                     "deferred to the match-engine slice", ip,
            protocol);
        break;
    default:
        break;
    }
    co_return;
}

// ---- SM wire handlers (parity with the legacy self-posts) ---------

boost::asio::awaitable<void>
OnSmTournamentReq(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.tournaments)
    {
        spdlog::warn("OnSmTournamentReq[{}]: tournaments not wired",
            ip);
        co_return;
    }
    wire::Reader r(body.data(), body.size());
    std::uint16_t id     = 0;
    std::uint8_t  group  = 0;
    std::uint8_t  step   = 0;
    std::uint32_t period = 0;
    if (!r.Read(id) || !r.Read(group) || !r.Read(step) ||
        !r.Read(period))
    {
        spdlog::warn("OnSmTournamentReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }
    co_await SmTournamentReqLogic(ctx, id, group, step, period);
    co_return;
}

boost::asio::awaitable<void>
OnSmTournamentEventReq(std::shared_ptr<PeerSession> peer,
                       std::vector<std::byte>       body,
                       const HandlerContext&        ctx)
{
    if (!ctx.tournaments)
    {
        spdlog::warn("OnSmTournamentEventReq[{}]: tournaments not "
                     "wired", peer->Wire()->RemoteIPv4());
        co_return;
    }
    co_await SmTournamentEventReqLogic(ctx, body,
        static_cast<std::int64_t>(std::time(nullptr)));
    co_return;
}

boost::asio::awaitable<void>
OnSmTournamentEventAck(std::shared_ptr<PeerSession> peer,
                       std::vector<std::byte>       body,
                       const HandlerContext&        ctx)
{
    if (!ctx.tournaments)
    {
        spdlog::warn("OnSmTournamentEventAck[{}]: tournaments not "
                     "wired", peer->Wire()->RemoteIPv4());
        co_return;
    }
    co_await SmTournamentEventAckLogic(ctx, std::move(body));
    co_return;
}

boost::asio::awaitable<void>
OnSmTournamentUpdateReq(std::shared_ptr<PeerSession> peer,
                        std::vector<std::byte>       body,
                        const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.tournaments)
    {
        spdlog::warn("OnSmTournamentUpdateReq[{}]: tournaments not "
                     "wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint16_t tid   = 0;
    std::uint8_t  count = 0;
    if (!r.Read(tid) || !r.Read(count))
    {
        spdlog::warn("OnSmTournamentUpdateReq[{}]: short body "
                     "({} bytes)", ip, body.size());
        co_return;
    }
    std::vector<TournamentStep> steps;
    for (std::uint8_t i = 0; i < count; ++i)
    {
        TournamentStep s{};
        if (!r.Read(s.group) || !r.Read(s.step) ||
            !r.Read(s.period) || !r.Read(s.start))
        {
            spdlog::warn("OnSmTournamentUpdateReq[{}]: malformed "
                         "step list", ip);
            co_return;
        }
        steps.push_back(s);
    }

    ctx.tournaments->RebuildCurrent(tid, steps);
    co_await BroadcastTournamentInfo(ctx, nullptr);
    co_return;
}

} // namespace tworldsvr::handlers
